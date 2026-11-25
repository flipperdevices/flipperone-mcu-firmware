"""toolchain.py (the pin and its copies) and fbtenv.sh (the bootstrap), without network."""

import copy
import hashlib
import io
import json
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import toolchain  # noqa: E402

REPO = HERE.parents[2]


def release(tag='v7', sha='a'):
    """A release.json as flipperone-mcu-toolchain publishes it."""
    archives = {}
    for platform in toolchain.PLATFORMS:
        suffix = '.zip' if platform.startswith('windows') else '.tar.gz'
        archives[platform] = {'name': f'flipperone-mcu-toolchain-{tag}-{platform}{suffix}', 'sha256': sha * 64,
                              'size': 1}
    return {
        'schema': 1, 'kind': 'flipperone-mcu-toolchain-release', 'tag': tag, 'source_commit': 'c' * 40,
        'manifest_sha256': 'd' * 64, 'archives': archives,
        'image': {'name': 'ghcr.io/flipperdevices/flipperone-mcu-toolchain', 'digest': 'sha256:' + 'e' * 64},
        'components': {
            'arm-none-eabi-gcc': {'version': '14.2.1-1.1', 'license': 'GPL'},
            'pico-sdk': {'version': '2.3.1', 'license': 'BSD', 'commit': 'f' * 40},
            'picotool': {'version': '2.3.1', 'license': 'BSD'},
        },
    }


CMAKELISTS = 'x\nset(sdkVersion 2.3.1)\nset(toolchainVersion 14_2_Rel1)\nset(picotoolVersion 2.3.1)\ny\n'


def test_the_committed_pin_is_canonical_and_consistent():
    assert toolchain.main(['check']) == 0


def test_render_round_trips_and_keeps_one_line_per_archive():
    pin = toolchain.pin_from_release(release())
    toolchain.validate(pin)
    text = toolchain.render(pin)
    assert json.loads(text) == pin
    for platform in toolchain.PLATFORMS:
        assert sum(f'"{platform}": {{"name": ' in line for line in text.splitlines()) == 1


@pytest.mark.parametrize('breakage', [
    lambda p: p.update(toolchain='7'),
    lambda p: p.update(image='ghcr.io/x:latest'),
    lambda p: p['archives'].pop('darwin-arm64'),
    lambda p: p['archives']['linux-x86_64'].update(sha256='A' * 64),
    lambda p: p['archives']['linux-x86_64'].update(name='other.tar.gz'),
    lambda p: p['components'].pop('pico-sdk'),
])
def test_validate_rejects(breakage):
    pin = toolchain.pin_from_release(release())
    breakage(pin)
    with pytest.raises(toolchain.PinError):
        toolchain.validate(pin)


def test_copies_follow_the_pin():
    pin = toolchain.pin_from_release(release())
    assert toolchain.check_copies(pin, CMAKELISTS) == []

    newer = copy.deepcopy(release())
    newer['components']['pico-sdk']['version'] = '2.4.0'
    newer['components']['arm-none-eabi-gcc']['version'] = '15.1.0-1.1'
    pin = toolchain.pin_from_release(newer)
    assert len(toolchain.check_copies(pin, CMAKELISTS)) == 2
    cmakelists = toolchain.update_copies(pin, CMAKELISTS)
    assert 'set(sdkVersion 2.4.0)\nset(toolchainVersion 15_1_Rel1)\nset(picotoolVersion 2.3.1)' in cmakelists
    assert cmakelists.startswith('x\n') and cmakelists.endswith('y\n')
    assert toolchain.check_copies(pin, cmakelists) == []


def test_a_later_arm_release_of_the_same_gcc_is_kept():
    pin = toolchain.pin_from_release(release())
    cmakelists = CMAKELISTS.replace('14_2_Rel1', '14_2_Rel2')
    assert toolchain.check_copies(pin, cmakelists) == []
    assert toolchain.update_copies(pin, cmakelists) == cmakelists


def test_template_paths_follow_the_pin():
    pin = toolchain.pin_from_release(release('v7'))
    templates = {'a.json': '~/.flipperone-mcu-toolchain/v6/x ~/.flipperone-mcu-toolchain/v7/y', 'b.json': '{}'}
    assert toolchain.template_problems(pin, templates) == ['vscode_template/a.json: toolchain v6, toolchain.json says v7']
    assert toolchain.template_problems(pin, toolchain.update_templates(pin, templates)) == []


@pytest.fixture
def repo(tmp_path, monkeypatch):
    """A scratch tree with the pin files, for update."""
    monkeypatch.setattr(toolchain, 'PIN', tmp_path / 'toolchain.json')
    monkeypatch.setattr(toolchain, 'CMAKELISTS', tmp_path / 'CMakeLists.txt')
    monkeypatch.setattr(toolchain, 'VSCODE_TEMPLATE', tmp_path)
    toolchain.PIN.write_text(toolchain.render(toolchain.pin_from_release(release('v7'))))
    toolchain.CMAKELISTS.write_text(CMAKELISTS)
    (tmp_path / 'launch.json').write_text('"${userHome}/.flipperone-mcu-toolchain/v7/tools"')
    return tmp_path


def test_update_writes_the_pin_and_reports_it(repo, monkeypatch):
    out = repo / 'github_output'
    monkeypatch.setenv('GITHUB_OUTPUT', str(out))
    monkeypatch.setattr(toolchain, 'fetch_release', lambda tag: release(tag, 'b'))
    monkeypatch.setattr(toolchain, 'latest_tag', lambda: 'v8')
    assert toolchain.main(['update']) == 0
    assert json.loads(toolchain.PIN.read_text())['toolchain'] == 'v8'
    assert json.loads(toolchain.PIN.read_text())['image'].endswith('e' * 64)
    assert out.read_text() == 'tag=v8\nchanged=true\n'
    assert (repo / 'launch.json').read_text() == '"${userHome}/.flipperone-mcu-toolchain/v8/tools"'
    assert toolchain.main(['update']) == 0
    assert out.read_text().endswith('tag=v8\nchanged=false\n')


def test_update_refuses_to_go_back_unless_asked(repo, monkeypatch):
    monkeypatch.setattr(toolchain, 'fetch_release', lambda tag: release(tag))
    assert toolchain.main(['update', '--tag', 'v6']) == 1
    assert toolchain.main(['update', '--tag', 'v6', '--allow-downgrade']) == 0


# fbtenv.sh

FAKE_BUNDLE_ENV = '''\
export FLIPPERONE_TOOLCHAIN='{tag}'
export FLIPPERONE_TOOLCHAIN_ROOT="$(cd "$(dirname "${{BASH_SOURCE[0]:-${{(%):-%x}}}}")" && pwd -P)"
'''


def fake_archive(path, tag):
    data = FAKE_BUNDLE_ENV.format(tag=tag).encode()
    with tarfile.open(path, 'w:gz') as tar:
        info = tarfile.TarInfo('fbtenv.sh')
        info.size = len(data)
        tar.addfile(info, io.BytesIO(data))
    return hashlib.sha256(path.read_bytes()).hexdigest()


@pytest.fixture
def checkout(tmp_path):
    """A tree with fbtenv.sh and a toolchain.json pinning a local fake archive."""
    tree = tmp_path / 'fw'
    (tree / 'scripts' / 'toolchain').mkdir(parents=True)
    shutil.copy(REPO / 'scripts' / 'toolchain' / 'fbtenv.sh', tree / 'scripts' / 'toolchain')
    archive = tmp_path / 'bundle.tar.gz'
    pin = toolchain.pin_from_release(release('v7', '0'))
    sha256 = fake_archive(archive, 'v7')
    for entry in pin['archives'].values():
        entry['sha256'] = sha256
    (tree / 'toolchain.json').write_text(toolchain.render(pin))
    return tree, archive


SHELLS = [shell for shell in ('bash', 'zsh') if shutil.which(shell)]


def source(shell, tree, archive, home, extra=''):
    script = f'. "{tree}/scripts/toolchain/fbtenv.sh"; echo "rc=$? $FLIPPERONE_TOOLCHAIN"{extra}'
    env = {'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'HOME': str(home),
           'FLIPPERONE_TOOLCHAIN_HOME': str(home / 'tc'), 'FLIPPERONE_TOOLCHAIN_ARCHIVE': str(archive)}
    return subprocess.run([shell, '-c', script], env=env, capture_output=True, text=True, timeout=60)


@pytest.mark.parametrize('shell', SHELLS)
def test_bootstrap_installs_once_then_reuses(shell, checkout, tmp_path):
    tree, archive = checkout
    first = source(shell, tree, archive, tmp_path)
    assert 'rc=0 v7' in first.stdout, first.stderr
    assert 'unpacking' in first.stderr
    installed = tmp_path / 'tc' / 'v7'
    assert (installed / 'fbtenv.sh').is_file()
    assert sorted(p.name for p in (tmp_path / 'tc').iterdir()) == ['v7']  # no partial left over

    archive.unlink()  # a second run needs no archive
    again = source(shell, tree, archive, tmp_path, '; type _fo_bootstrap >/dev/null 2>&1 && echo leaked')
    assert 'rc=0 v7' in again.stdout and 'leaked' not in again.stdout, again.stderr
    assert again.stderr == ''


@pytest.mark.parametrize('shell', SHELLS)
def test_bootstrap_replaces_an_unfinished_install(shell, checkout, tmp_path):
    tree, archive = checkout
    (tmp_path / 'tc' / 'v7' / 'leftover').mkdir(parents=True)
    assert 'rc=0 v7' in source(shell, tree, archive, tmp_path).stdout
    assert sorted(p.name for p in (tmp_path / 'tc' / 'v7').iterdir()) == ['.archive-sha256', 'fbtenv.sh']
    assert sorted(p.name for p in (tmp_path / 'tc').iterdir()) == ['v7']


@pytest.mark.parametrize('shell', SHELLS)
def test_bootstrap_rejects_an_archive_with_another_hash(shell, checkout, tmp_path):
    tree, archive = checkout
    fake_archive(archive, 'v6')
    result = source(shell, tree, archive, tmp_path)
    assert 'rc=1' in result.stdout and 'expects' in result.stderr
    assert not (tmp_path / 'tc' / 'v7').exists()
    assert archive.exists()  # a local archive is the user's, not deleted


def test_bootstrap_refuses_to_run_as_a_program(checkout):
    tree, _ = checkout
    result = subprocess.run(['bash', str(tree / 'scripts' / 'toolchain' / 'fbtenv.sh')], capture_output=True,
                            text=True, timeout=60)
    assert result.returncode == 1 and 'source this file' in result.stderr
