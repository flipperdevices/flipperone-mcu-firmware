#!/usr/bin/env python3
"""Run Nightly MCU for the current firmware PR."""

import json
import os
from pathlib import Path
import re
import signal
import time
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

REPOSITORY = 'flipperdevices/flipperone-system-testing'
WORKFLOW = 'pr-mcu-smoke.yml'
FIRMWARE = 'https://update.flipperzero.one/builds/flipper-one-mcu/'
REPORTS = ('https://bsb-qa-runners-crash-reports.s3.eu-central-1.amazonaws.com/'
           'runner-logs/flipperone-system-tests/allure/pr-mcu-smoke')


def github(path, data=None, *, timeout=30, attempts=3):
    request = Request(
        f'https://api.github.com/repos/{REPOSITORY}{path}',
        data=json.dumps(data).encode() if data is not None else None,
        headers={
            'Authorization': 'Bearer ' + os.environ['MCU_PR_TEST_TOKEN'],
            'Accept': 'application/vnd.github+json',
            'Content-Type': 'application/json',
            'X-GitHub-Api-Version': '2022-11-28',
        },
    )
    if data is not None:
        attempts = 1  # Dispatch and cancellation must not be sent twice.
    for attempt in range(attempts):
        try:
            with urlopen(request, timeout=timeout) as response:
                raw = response.read()
            return json.loads(raw) if raw else None
        except OSError as error:
            retryable = not isinstance(error, HTTPError) or error.code in (404, 429, 500, 502, 503, 504)
            if not retryable or attempt + 1 == attempts:
                raise
            time.sleep(attempt + 1)


def download_receipt(run):
    url = f"{REPORTS}/{run['id']}/{run['run_attempt']}/mcu-smoke-result.json"
    with urlopen(url, timeout=30) as response:
        data = response.read(1024**2 + 1)
    if len(data) > 1024**2:
        raise ValueError('Result receipt exceeds the limit')
    return json.loads(data)


def summary(text):
    print(text, flush=True)
    with Path(os.environ['GITHUB_STEP_SUMMARY']).open('a') as stream:
        stream.write(text + '\n\n')


def make_request():
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    pr = event['pull_request']
    if pr['head']['repo']['full_name'] != os.environ['GITHUB_REPOSITORY']:
        raise ValueError('Fork PR is not eligible')
    build = os.environ.get('FONE_MCU_LINUX_BUILD', '')
    channel = os.environ.get('FONE_MCU_LINUX_CHANNEL') or 'nightly'
    if channel not in ('nightly', 'release-candidate'):
        raise ValueError('Invalid Linux bundle channel')
    if build and not re.fullmatch(r'[1-9][0-9]*', build):
        raise ValueError('Invalid Linux bundle build number')
    if not build and channel != 'nightly':
        raise ValueError('A non-nightly channel requires a pinned Linux bundle build')
    if not os.environ.get('MCU_IMAGES'):
        raise ValueError('Firmware publication returned no image metadata')
    images = json.loads(os.environ['MCU_IMAGES'])
    base = FIRMWARE + quote(pr['head']['ref'], safe='/') + '/'
    for field in ('uf2', 'table'):
        images[field + '_url'] = base + quote(images.pop(field + '_name'))
    return {
        **images,
        'schema': 2,
        'request_id': f"mcu-{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}",
        'run_id': int(os.environ['GITHUB_RUN_ID']),
        'run_attempt': int(os.environ['GITHUB_RUN_ATTEMPT']),
        'pr_number': pr['number'],
        'firmware_commit': pr['head']['sha'],
        'source_sha': os.environ['GITHUB_SHA'],
        'linux_build_number': int(build) if build else None,
        'linux_channel': channel,
    }


def cancel_run(run_id, request_id, workflow):
    # A lost dispatch response leaves no ID. Recover it by the unique request title.
    if run_id is None:
        expected = 'PR MCU smoke · ' + request_id
        for attempt in range(2):
            try:
                runs = github(f'/actions/workflows/{workflow}/runs?event=workflow_dispatch&per_page=100',
                              timeout=2, attempts=1)['workflow_runs']
                matches = [run for run in runs if run['display_title'] == expected]
                if len(matches) > 1:
                    raise ValueError('Ambiguous system smoke run')
                if matches:
                    run_id = matches[0]['id']
                    break
            except OSError:
                if attempt:
                    raise
            if attempt == 0:
                time.sleep(1)
    if run_id is None:
        raise ValueError('Dispatched run was not found for cancellation')
    github(f'/actions/runs/{run_id}/cancel', {}, timeout=2)


def verify_receipt(receipt, request, run):
    expected = {
        'schema': 2,
        'request_id': request['request_id'],
        'firmware_commit': request['firmware_commit'],
        'uf2_sha256': request['uf2_sha256'],
        'table_sha256': request['table_sha256'],
        'source_run_id': request['run_id'],
        'source_run_attempt': request['run_attempt'],
        'run_id': str(run['id']),
        'run_attempt': run['run_attempt'],
    }
    if any(receipt.get(key) != value for key, value in expected.items()):
        raise ValueError('System result belongs to a different build')
    outcomes = [run['conclusion'], receipt.get('test_outcome'),
                receipt.get('units_outcome'), receipt.get('system_outcome')]
    if any(outcome != 'success' for outcome in outcomes):
        raise ValueError('MCU system qualification did not pass')


def run(request):
    ref = os.environ.get('FONE_SYSTEM_TESTS_REF') or 'main'
    workflow = os.environ.get('FONE_SYSTEM_TESTS_WORKFLOW') or WORKFLOW
    if workflow not in (WORKFLOW, 'nightly-mcu.yml'):
        raise ValueError('Unsupported FONE_SYSTEM_TESTS_WORKFLOW')
    field = 'mcu_request' if workflow == 'nightly-mcu.yml' else 'request'
    deadline = time.monotonic() + 3 * 60 * 60
    run_id = None
    summary(f"MCU system qualification requested for PR #{request['pr_number']}, `{request['firmware_commit']}`")
    cancel_needed = True
    try:
        response = github(f'/actions/workflows/{workflow}/dispatches', {
            'ref': ref, 'inputs': {field: json.dumps(request)}, 'return_run_details': True})
        returned_id = response.get('workflow_run_id') if isinstance(response, dict) else None
        if type(returned_id) is not int or returned_id <= 0:
            raise ValueError('Dispatch returned no valid run ID')
        run_id = returned_id
        url = f'https://github.com/{REPOSITORY}/actions/runs/{run_id}'
        summary(f'[System test run]({url}) · [Reports and diagnostics]({url}#artifacts)')
        while time.monotonic() < deadline:
            remote = github(f'/actions/runs/{run_id}')
            if remote['status'] == 'completed':
                cancel_needed = False
                break
            time.sleep(20)
        else:
            raise TimeoutError('Timed out waiting for MCU system qualification')
        summary(f"MCU system qualification: **{remote['conclusion']}** — [run]({url})")
        result = download_receipt(remote)
        report_url = result.get('report_url', '')
        if report_url.startswith('https://') and not any(c in report_url for c in '\n\r()'):
            summary(f'[Allure report]({report_url})')
        verify_receipt(result, request, remote)
    except HTTPError as error:
        if run_id is None and 400 <= error.code < 500:
            cancel_needed = False  # GitHub rejected the dispatch.
        raise
    finally:
        if cancel_needed:
            try:
                cancel_run(run_id, request['request_id'], workflow)
            except (OSError, ValueError):
                summary('Remote cancellation could not be confirmed; the system job has its own timeout.')


def main():
    def interrupted(signum, _frame):
        raise SystemExit(128 + signum)
    signal.signal(signal.SIGTERM, interrupted)
    try:
        if not os.environ.get('MCU_PR_TEST_TOKEN'):
            raise ValueError('MCU_PR_TEST_TOKEN is required')
        request = make_request()
        run(request)
        return 0
    except (OSError, ValueError) as error:
        summary('MCU system qualification failed: ' + str(error))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
