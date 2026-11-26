# shellcheck shell=bash
# Set up the Flipper One MCU toolchain pinned in toolchain.json, from bash or zsh:
#
#   . scripts/toolchain/fbtenv.sh
#
# The first time, it downloads the release archive for this machine, checks its
# sha256 against toolchain.json and unpacks it into
# ${FLIPPERONE_TOOLCHAIN_HOME:-~/.flipper/flipperone-mcu-toolchain}/<tag>/. Then, and on
# every later run, it sources that bundle's own fbtenv.sh, which puts the tools first on
# PATH and exports PICO_SDK_PATH and the rest, and points this tree's .toolchain link at
# the bundle for IDE configs. Sourcing it again is harmless.
#
# Offline: set FLIPPERONE_TOOLCHAIN_ARCHIVE to an already downloaded archive. It is
# checked against toolchain.json all the same.
#
# Plain POSIX tools only (curl or wget, sha256sum or shasum, tar): no Python needed.

_fo_bootstrap() {
    local script="$1" repo pin tag repository platform line name sha256 home dir
    local partial download url actual progress

    repo="$(cd "$(dirname "$script")/../.." && pwd -P)" || return 1
    pin="$repo/toolchain.json"
    if [ ! -f "$pin" ]; then
        echo "fbtenv.sh: $pin not found" >&2
        return 1
    fi

    case "$(uname -s)-$(uname -m)" in
        Linux-x86_64) platform=linux-x86_64 ;;
        Linux-aarch64 | Linux-arm64) platform=linux-aarch64 ;;
        Darwin-arm64) platform=darwin-arm64 ;;
        # An x86_64 shell on Apple silicon runs under Rosetta; the arm64 tools run natively
        Darwin-x86_64)
            if [ "$(sysctl -n hw.optional.arm64 2>/dev/null)" = 1 ]; then
                platform=darwin-arm64
            else
                echo "fbtenv.sh: Intel Macs are not supported by the toolchain" >&2
                return 1
            fi
            ;;
        *)
            echo "fbtenv.sh: no toolchain for $(uname -s) $(uname -m)" >&2
            return 1
            ;;
    esac

    # toolchain.json has a fixed layout, one line per archive (scripts/toolchain/toolchain.py)
    tag="$(sed -n 's/^  "toolchain": "\(v[0-9][0-9]*\)",$/\1/p' "$pin")"
    repository="$(sed -n 's/^  "repository": "\([^"]*\)",$/\1/p' "$pin")"
    line="$(grep "^    \"$platform\": {" "$pin" || true)"
    name="$(printf '%s\n' "$line" | sed -n 's/.*"name": "\([^"]*\)".*/\1/p')"
    sha256="$(printf '%s\n' "$line" | sed -n 's/.*"sha256": "\([0-9a-f]\{64\}\)".*/\1/p')"
    if [ -z "$tag" ] || [ -z "$repository" ] || [ -z "$name" ] || [ -z "$sha256" ]; then
        echo "fbtenv.sh: cannot read the $platform archive from $pin" >&2
        return 1
    fi

    home="${FLIPPERONE_TOOLCHAIN_HOME:-$HOME/.flipper/flipperone-mcu-toolchain}"
    # The tree link and the environment need an absolute path
    case "$home" in /*) ;; *) home="$PWD/$home" ;; esac
    dir="$home/$tag"
    if [ "$(cat "$dir/.archive-sha256" 2>/dev/null)" != "$sha256" ]; then
        mkdir -p "$home" || return 1
        if [ -n "${FLIPPERONE_TOOLCHAIN_ARCHIVE:-}" ]; then
            download="$FLIPPERONE_TOOLCHAIN_ARCHIVE"
        else
            download="$home/$name.$$.part"
            url="https://github.com/$repository/releases/download/$tag/$name"
            echo "fbtenv.sh: downloading $url" >&2
            if [ -t 2 ]; then progress=--progress-bar; else progress=-sS; fi
            if command -v curl >/dev/null 2>&1; then
                curl -fL --retry 3 "$progress" -o "$download" "$url"
            elif command -v wget >/dev/null 2>&1; then
                wget -q -O "$download" "$url"
            else
                echo "fbtenv.sh: neither curl nor wget is installed" >&2
                false
            fi || { rm -f "$download"; return 1; }
        fi

        if command -v sha256sum >/dev/null 2>&1; then
            actual="$(sha256sum "$download" | cut -d' ' -f1)"
        else
            actual="$(shasum -a 256 "$download" | cut -d' ' -f1)"
        fi
        if [ "$actual" != "$sha256" ]; then
            echo "fbtenv.sh: $download has sha256 $actual, toolchain.json expects $sha256" >&2
            [ -n "${FLIPPERONE_TOOLCHAIN_ARCHIVE:-}" ] || rm -f "$download"
            return 1
        fi

        # Unpack beside the final place and rename, so an interrupted run leaves no
        # half-unpacked bundle behind under the real name
        echo "fbtenv.sh: unpacking $name into $dir" >&2
        partial="$home/.$tag.partial.$$"
        rm -rf "$partial" && mkdir -p "$partial" && tar -xzf "$download" -C "$partial" &&
            printf '%s\n' "$sha256" >"$partial/.archive-sha256" || {
            rm -rf "$partial"
            return 1
        }
        [ -n "${FLIPPERONE_TOOLCHAIN_ARCHIVE:-}" ] || rm -f "$download"
        # Another shell may have installed it meanwhile: keep that copy
        if [ "$(cat "$dir/.archive-sha256" 2>/dev/null)" = "$sha256" ]; then
            rm -rf "$partial"
        else
            rm -rf "$dir" && mv "$partial" "$dir" || { rm -rf "$partial"; return 1; }
            rm -rf "${dir:?}/${partial##*/}"  # moved into a copy installed meanwhile
        fi
    fi

    # shellcheck source=/dev/null
    . "$dir/fbtenv.sh" || return 1
    if [ "${FLIPPERONE_TOOLCHAIN:-}" != "$tag" ]; then
        echo "fbtenv.sh: $dir is toolchain ${FLIPPERONE_TOOLCHAIN:-?}, expected $tag" >&2
        return 1
    fi

    if [ -L "$repo/.toolchain" ] || [ ! -e "$repo/.toolchain" ]; then
        [ "$(readlink "$repo/.toolchain")" = "$dir" ] || ln -sfn "$dir" "$repo/.toolchain" ||
            echo "fbtenv.sh: cannot point $repo/.toolchain at $dir" >&2
    else
        echo "fbtenv.sh: $repo/.toolchain is not a link, left alone" >&2
    fi
}

if [ -n "${BASH_SOURCE:-}" ]; then
    if [ "${BASH_SOURCE[0]}" = "$0" ]; then
        echo "fbtenv.sh: source this file instead of running it: . $0" >&2
        exit 1
    fi
    _fo_bootstrap "${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION:-}" ]; then
    eval '_fo_bootstrap "${(%):-%x}"'
else
    echo "fbtenv.sh: source this file from bash or zsh" >&2
    false
fi
_fo_status=$?
unset -f _fo_bootstrap
eval "unset _fo_status; return $_fo_status"
