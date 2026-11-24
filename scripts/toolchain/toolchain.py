#!/usr/bin/env python3
"""Keep toolchain.json, the pinned flipperone-mcu-toolchain release, in sync.

    python3 scripts/toolchain/toolchain.py check
    python3 scripts/toolchain/toolchain.py update [--tag vN | --release-json FILE]

`check` fails when toolchain.json is not in its canonical layout, or when a copy
disagrees: the version lines at the top of CMakeLists.txt (Pico VS Code extension)
and the toolchain paths in vscode_template/.

`update` reads release.json of the newest toolchain release (or of --tag) and
rewrites toolchain.json and those copies. Nothing is written when the pin is already
at that release. Under GitHub Actions it reports `tag` and `changed` as step outputs.

The layout is canonical because scripts/toolchain/fbtenv.sh reads the file with sed,
not with a JSON parser: one line per archive, keys in a fixed order. Write it only
through this script.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PIN = REPO_ROOT / 'toolchain.json'
CMAKELISTS = REPO_ROOT / 'CMakeLists.txt'
VSCODE_TEMPLATE = REPO_ROOT / 'vscode_template'

REPOSITORY = 'flipperdevices/flipperone-mcu-toolchain'
PLATFORMS = ('linux-x86_64', 'linux-aarch64', 'darwin-arm64', 'windows-x64')
TAG_RE = re.compile(r'v[1-9][0-9]*')
SHA256_RE = re.compile(r'[0-9a-f]{64}')
IMAGE_RE = re.compile(r'ghcr\.io/[a-z0-9._/-]+@sha256:[0-9a-f]{64}')
# The Pico VS Code extension reads these lines literally; see CMakeLists.txt.
VSCODE_LINE = 'set({name} {value})'
BUNDLE_PATH_RE = re.compile(r'(\.flipperone-mcu-toolchain/)(v[0-9]+)/')


class PinError(Exception):
    pass


def tag_number(tag: str) -> int:
    if not TAG_RE.fullmatch(tag):
        raise PinError(f'toolchain tags are v<N>, N a positive integer; got {tag!r}')
    return int(tag[1:])


def pin_from_release(release: dict) -> dict:
    """The toolchain.json content for a release.json."""
    if release.get('schema') != 1 or release.get('kind') != 'flipperone-mcu-toolchain-release':
        raise PinError('not a flipperone-mcu-toolchain release.json (schema 1)')
    image = release['image']
    return {
        'schema': 1,
        'toolchain': release['tag'],
        'repository': REPOSITORY,
        'source_commit': release['source_commit'],
        'image': f"{image['name']}@{image['digest']}",
        'archives': {p: release['archives'][p] for p in PLATFORMS},
        'components': release['components'],
    }


def validate(pin: dict) -> None:
    problems = []
    if pin.get('schema') != 1:
        problems.append('schema is not 1')
    try:
        tag_number(pin.get('toolchain', ''))
    except PinError as e:
        problems.append(str(e))
    if not IMAGE_RE.fullmatch(pin.get('image', '')):
        problems.append(f"image {pin.get('image')!r} is not ghcr.io/<name>@sha256:<digest>")
    archives = pin.get('archives', {})
    if sorted(archives) != sorted(PLATFORMS):
        problems.append(f'archives must be exactly {", ".join(PLATFORMS)}')
    for platform, archive in archives.items():
        if set(archive) != {'name', 'sha256', 'size'}:
            problems.append(f'{platform}: archive keys must be name, sha256, size')
            continue
        suffix = '.zip' if platform.startswith('windows') else '.tar.gz'
        expected = f"flipperone-mcu-toolchain-{pin.get('toolchain')}-{platform}{suffix}"
        if archive['name'] != expected:
            problems.append(f'{platform}: archive name {archive["name"]!r}, expected {expected!r}')
        if not SHA256_RE.fullmatch(archive['sha256']):
            problems.append(f'{platform}: sha256 is not 64 lowercase hex digits')
        if not isinstance(archive['size'], int) or archive['size'] <= 0:
            problems.append(f'{platform}: size is not a positive integer')
    for name in ('arm-none-eabi-gcc', 'pico-sdk', 'picotool'):
        if 'version' not in pin.get('components', {}).get(name, {}):
            problems.append(f'components: no version for {name}')
    if problems:
        raise PinError('toolchain.json: ' + '; '.join(problems))


def render(pin: dict) -> str:
    """The canonical text of toolchain.json."""
    def line(key, value, last=False):
        return f'  {json.dumps(key)}: {json.dumps(value)}{"" if last else ","}'

    def block(key, entries, last=False):
        inner = [f'    {json.dumps(k)}: {json.dumps(v)}' for k, v in entries.items()]
        return [f'  {json.dumps(key)}: {{', ',\n'.join(inner), '  }' + ('' if last else ',')]

    lines = ['{']
    for key in ('schema', 'toolchain', 'repository', 'source_commit', 'image'):
        lines.append(line(key, pin[key]))
    lines += block('archives', {p: {k: pin['archives'][p][k] for k in ('name', 'sha256', 'size')} for p in PLATFORMS})
    lines += block('components', pin['components'], last=True)
    lines.append('}')
    return '\n'.join(lines) + '\n'


def vscode_versions(pin: dict) -> dict[str, str]:
    """The Pico VS Code extension's version lines for this pin.

    The extension only knows Arm GNU toolchain releases, named <major>_<minor>_Rel1.
    The bundle's xPack GCC is the same GCC release, so only major.minor is carried.
    """
    gcc = pin['components']['arm-none-eabi-gcc']['version']
    major, minor = gcc.split('.')[:2]
    return {
        'sdkVersion': pin['components']['pico-sdk']['version'],
        'toolchainVersion': f'{major}_{minor}_Rel1',
        'picotoolVersion': pin['components']['picotool']['version'],
    }


def _vscode_line_re(name: str) -> re.Pattern:
    return re.compile(rf'^set\({name} ([^)\s]+)\)$', re.MULTILINE)


def check_copies(pin: dict, cmakelists: str) -> list[str]:
    problems = []
    for name, value in vscode_versions(pin).items():
        found = _vscode_line_re(name).findall(cmakelists)
        if len(found) != 1:
            problems.append(f"CMakeLists.txt: expected one 'set({name} ...)' line, found {len(found)}")
        elif name == 'toolchainVersion':
            if not found[0].startswith(value.rsplit('_', 1)[0] + '_'):
                problems.append(f'CMakeLists.txt: toolchainVersion {found[0]} is not GCC '
                                f'{pin["components"]["arm-none-eabi-gcc"]["version"]} (expected {value})')
        elif found[0] != value:
            problems.append(f'CMakeLists.txt: {name} is {found[0]}, toolchain.json says {value}')
    return problems


def update_copies(pin: dict, cmakelists: str) -> str:
    for name, value in vscode_versions(pin).items():
        pattern = _vscode_line_re(name)
        current = pattern.findall(cmakelists)
        # Keep a toolchainVersion of the same GCC major.minor (e.g. a later RelN)
        if name == 'toolchainVersion' and current and current[0].startswith(value.rsplit('_', 1)[0] + '_'):
            continue
        cmakelists = pattern.sub(VSCODE_LINE.format(name=name, value=value), cmakelists)
    return cmakelists


def template_problems(pin: dict, templates: dict[str, str]) -> list[str]:
    return [f'vscode_template/{name}: toolchain {tag}, toolchain.json says {pin["toolchain"]}'
            for name, text in sorted(templates.items())
            for tag in sorted(set(m.group(2) for m in BUNDLE_PATH_RE.finditer(text))) if tag != pin['toolchain']]


def update_templates(pin: dict, templates: dict[str, str]) -> dict[str, str]:
    return {name: BUNDLE_PATH_RE.sub(rf'\g<1>{pin["toolchain"]}/', text) for name, text in templates.items()}


def _templates() -> dict[str, str]:
    return {p.name: p.read_text(encoding='utf-8') for p in sorted(VSCODE_TEMPLATE.glob('*.json'))}


def _get(url: str) -> bytes:
    headers = {'Accept': 'application/json', 'User-Agent': 'flipperone-mcu-firmware-toolchain-bump'}
    token = os.environ.get('GITHUB_TOKEN')
    if token and url.startswith('https://api.github.com/'):
        headers['Authorization'] = f'Bearer {token}'
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as response:
        return response.read()


def latest_tag() -> str:
    """The newest v<N> release; 'latest' on GitHub excludes prereleases such as 'mirror'."""
    data = json.loads(_get(f'https://api.github.com/repos/{REPOSITORY}/releases?per_page=100'))
    tags = [r['tag_name'] for r in data if not r['draft'] and not r['prerelease'] and TAG_RE.fullmatch(r['tag_name'])]
    if not tags:
        raise PinError(f'{REPOSITORY} has no v<N> release')
    return max(tags, key=tag_number)


def fetch_release(tag: str) -> dict:
    return json.loads(_get(f'https://github.com/{REPOSITORY}/releases/download/{tag}/release.json'))


def _read(path: Path) -> str | None:
    return path.read_text(encoding='utf-8') if path.exists() else None


def _output(**values: str) -> None:
    target = os.environ.get('GITHUB_OUTPUT')
    if target:
        with open(target, 'a', encoding='utf-8') as f:
            for key, value in values.items():
                f.write(f'{key}={value}\n')


def cmd_check(_args) -> int:
    text = PIN.read_text(encoding='utf-8')
    pin = json.loads(text)
    validate(pin)
    problems = []
    if text != render(pin):
        problems.append('toolchain.json is not in its canonical layout; rewrite it with this script')
    problems += check_copies(pin, CMAKELISTS.read_text(encoding='utf-8'))
    problems += template_problems(pin, _templates())
    for problem in problems:
        print(f'error: {problem}', file=sys.stderr)
    if not problems:
        print(f'toolchain.json: {pin["toolchain"]}, consistent')
    return 1 if problems else 0


def cmd_update(args) -> int:
    current_text = _read(PIN)
    current = json.loads(current_text) if current_text else {'toolchain': 'none'}
    if args.release_json:
        release = json.loads(Path(args.release_json).read_text(encoding='utf-8'))
    else:
        release = fetch_release(args.tag or latest_tag())
    pin = pin_from_release(release)
    validate(pin)
    older = current_text and tag_number(pin['toolchain']) < tag_number(current['toolchain'])
    if older and not args.allow_downgrade:
        raise PinError(f'{pin["toolchain"]} is older than the pinned {current["toolchain"]}; '
                       'pass --allow-downgrade to go back')
    cmakelists = update_copies(pin, CMAKELISTS.read_text(encoding='utf-8'))
    changed = render(pin) != current_text
    PIN.write_text(render(pin), encoding='utf-8')
    CMAKELISTS.write_text(cmakelists, encoding='utf-8')
    for name, text in update_templates(pin, _templates()).items():
        (VSCODE_TEMPLATE / name).write_text(text, encoding='utf-8')
    print(f'toolchain.json: {current["toolchain"]} -> {pin["toolchain"]}' if changed
          else f'toolchain.json: already at {pin["toolchain"]}')
    _output(tag=pin['toolchain'], changed=str(changed).lower())
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='toolchain.py', description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('check', help='check toolchain.json and its copies').set_defaults(func=cmd_check)
    update = sub.add_parser('update', help='pin a toolchain release')
    source = update.add_mutually_exclusive_group()
    source.add_argument('--tag', help='release tag to pin (default: the newest v<N>)')
    source.add_argument('--release-json', help='a local release.json instead of downloading one')
    update.add_argument('--allow-downgrade', action='store_true', help='allow pinning an older release')
    update.set_defaults(func=cmd_update)
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except PinError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
