#!/usr/bin/env python3
"""Dispatch system qualification for this PR build and propagate its exact result."""
import hashlib
import io
import json
import os
from pathlib import Path
import re
import signal
import time
from urllib.error import HTTPError
from urllib.request import HTTPRedirectHandler, Request, build_opener, urlopen
import zipfile

REPOSITORY = 'flipperdevices/flipperone-system-testing'
WORKFLOW = 'pr-mcu-smoke.yml'
MAX_RESULT = 1024**2
MAX_API = 16 * 1024**2


def require(ok, message):
    if not ok:
        raise ValueError(message)


def workflow_file():
    value = os.environ.get('FONE_SYSTEM_TESTS_WORKFLOW') or WORKFLOW
    require(value in (WORKFLOW, 'nightly-mcu.yml'), 'Unsupported FONE_SYSTEM_TESTS_WORKFLOW')
    return value


def check_configuration():
    required = ['FONE_SYSTEM_TESTS_TOKEN']
    if (os.environ.get('FONE_MCU_LINUX_CHANNEL') or 'nightly') != 'nightly':
        required.append('FONE_MCU_LINUX_BUILD')
    missing = [name for name in required if not os.environ.get(name)]
    require(not missing, 'Missing repository settings: ' + ', '.join(missing))
    workflow_file()


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *args):
        return None


class GitHub:
    def __init__(self, token):
        require(bool(token), 'FONE_SYSTEM_TESTS_TOKEN is required')
        self.token = token

    def request(self, path, data=None):
        request = Request('https://api.github.com/repos/' + REPOSITORY + path,
            data=json.dumps(data).encode() if data is not None else None,
            headers={'Authorization': 'Bearer ' + self.token, 'Accept': 'application/vnd.github+json',
                     'Content-Type': 'application/json', 'X-GitHub-Api-Version': '2022-11-28'})
        with urlopen(request, timeout=30) as response:
            raw = response.read(MAX_API + 1)
        require(len(raw) <= MAX_API, 'GitHub response exceeds the limit')
        return json.loads(raw) if raw else None

    def receipt(self, artifact_id):
        request = Request(f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{artifact_id}/zip',
                          headers={'Authorization': 'Bearer ' + self.token})
        try:
            build_opener(NoRedirect()).open(request, timeout=30)
        except HTTPError as error:
            require(error.code == 302, 'Missing artifact redirect')
            location = error.headers['Location']
        else:
            raise ValueError('Expected an artifact download redirect')
        require(location.startswith('https://'), 'Artifact download must use HTTPS')
        # Signed storage URLs receive no API credentials.
        with urlopen(location, timeout=30) as response:
            content = response.read(MAX_RESULT + 1)
        require(len(content) <= MAX_RESULT, 'Result artifact exceeds the limit')
        with zipfile.ZipFile(io.BytesIO(content)) as archive:
            names = archive.namelist()
            require(names == ['mcu-smoke-result.json'], 'Unexpected result artifact members')
            require(archive.getinfo(names[0]).file_size <= MAX_RESULT, 'Result JSON exceeds the limit')
            return json.loads(archive.read(names[0]))


def summary(text):
    print(text, flush=True)
    with Path(os.environ['GITHUB_STEP_SUMMARY']).open('a') as stream:
        stream.write(text + '\n\n')


def make_request(directory, artifact_id):
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    pr = event['pull_request']
    require(pr['head']['repo']['full_name'] == os.environ['GITHUB_REPOSITORY'], 'Fork PR is not eligible')
    uf2 = list(directory.glob('flipper-one-mcu-f2-firmware-*.uf2'))
    table = list(directory.glob('flipper-one-mcu-partition-table-*.uf2'))
    require(len(uf2) == len(table) == 1, 'Expected exactly one f2 UF2 and partition table')
    build = os.environ.get('FONE_MCU_LINUX_BUILD', '')
    channel = os.environ.get('FONE_MCU_LINUX_CHANNEL') or 'nightly'
    require(channel in ('nightly', 'release-candidate'), 'Invalid Linux bundle channel')
    require(bool(re.fullmatch(r'[1-9][0-9]*', build)) if build else channel == 'nightly',
            'Set FONE_MCU_LINUX_BUILD to a compatible pinned bundle build for a non-nightly channel')
    result = {'schema': 1, 'request_id': f"mcu-{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}",
        'run_id': int(os.environ['GITHUB_RUN_ID']), 'run_attempt': int(os.environ['GITHUB_RUN_ATTEMPT']),
        'artifact_id': int(artifact_id), 'pr_number': pr['number'], 'firmware_commit': pr['head']['sha'],
        'source_sha': os.environ['GITHUB_SHA'], 'linux_build_number': int(build) if build else None, 'linux_channel': channel}
    for path, field in ((uf2[0], 'uf2'), (table[0], 'table')):
        require(not path.is_symlink() and 0 < path.stat().st_size <= 32 * 1024**2, 'Invalid MCU image')
        result[field + '_name'] = path.name
        result[field + '_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def find_run(client, request_id, workflow=WORKFLOW):
    expected = 'PR MCU smoke · ' + request_id
    # Paginate; never adopt the latest run or another concurrent PR's run.
    for page in range(1, 6):
        runs = client.request(f'/actions/workflows/{workflow}/runs?event=workflow_dispatch&per_page=100&page={page}')['workflow_runs']
        matches = [run for run in runs if run['display_title'] == expected]
        require(len(matches) <= 1, 'Ambiguous system smoke run')
        if matches:
            return matches[0]
        if len(runs) < 100:
            break
    return None


def verify_receipt(receipt, request, run):
    expected = {'schema': 1, 'request_id': request['request_id'],
        'firmware_commit': request['firmware_commit'], 'uf2_sha256': request['uf2_sha256'],
        'source_run_id': request['run_id'], 'source_run_attempt': request['run_attempt'],
        'run_id': str(run['id'])}
    require(all(receipt.get(k) == v for k, v in expected.items()), 'System result belongs to a different build')
    require(run['conclusion'] == 'success' and receipt.get('test_outcome') == 'success'
            and receipt.get('units_outcome') == 'success' and receipt.get('system_outcome') == 'success',
            'MCU system qualification did not pass')


def run(client, request, *, now=time.monotonic, sleep=time.sleep, timeout=3 * 60 * 60):
    ref = os.environ.get('FONE_SYSTEM_TESTS_REF') or 'main'
    workflow = workflow_file()
    title = request['request_id']
    began = now()
    remote = None
    completed = False
    summary(f"MCU system qualification requested for PR #{request['pr_number']}, `{request['firmware_commit']}`")
    try:
        field = 'mcu_request' if workflow == 'nightly-mcu.yml' else 'request'
        try:
            client.request(f'/actions/workflows/{workflow}/dispatches', {'ref': ref, 'inputs': {field: json.dumps(request)}})
        except HTTPError as error:
            if error.code == 404:
                raise ValueError('Receiver workflow is unavailable. Before the receiver is merged, set '
                    'FONE_SYSTEM_TESTS_WORKFLOW=nightly-mcu.yml and FONE_SYSTEM_TESTS_REF to its draft branch; '
                    'also verify token access.') from None
            raise
        while now() - began < min(timeout, 300):
            remote = find_run(client, title, workflow)
            if remote:
                break
            sleep(10)
        require(remote is not None, 'System workflow was not found within five minutes')
        url = f"https://github.com/{REPOSITORY}/actions/runs/{remote['id']}"
        summary(f'[System test run]({url}) · [Reports and diagnostics]({url}#artifacts)')
        while now() - began < timeout:
            remote = client.request(f"/actions/runs/{remote['id']}")
            if remote['status'] == 'completed':
                completed = True
                break
            sleep(20)
        require(completed, 'Timed out waiting for MCU system qualification')
        summary(f"MCU system qualification: **{remote['conclusion']}** — [run]({url})")
        artifacts = client.request(f"/actions/runs/{remote['id']}/artifacts?per_page=100")['artifacts']
        selected = [a for a in artifacts if a['name'] == 'mcu-smoke-result-' + title and not a['expired']]
        require(len(selected) == 1, 'System workflow returned no unique result receipt; inspect its run')
        result = client.receipt(selected[0]['id'])
        report_url = result.get('report_url', '')
        if report_url.startswith('https://') and not any(c in report_url for c in '\n\r()'):
            summary(f'[Allure report]({report_url})')
        verify_receipt(result, request, remote)
        return 0
    finally:
        if remote and not completed:
            try:
                client.request(f"/actions/runs/{remote['id']}/cancel", {})
            except (OSError, ValueError):
                summary('Remote cancellation could not be confirmed; the system job has its own timeout.')


def main():
    def interrupted(signum, _frame):
        raise SystemExit(128 + signum)
    signal.signal(signal.SIGTERM, interrupted)
    try:
        check_configuration()
        client = GitHub(os.environ.get('FONE_SYSTEM_TESTS_TOKEN', ''))
        request = make_request(Path('firmware'), os.environ['MCU_ARTIFACT_ID'])
        return run(client, request)
    except (OSError, ValueError) as error:
        summary('MCU system qualification failed: ' + str(error))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
