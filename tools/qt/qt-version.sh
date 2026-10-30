# Pinned Qt for the tibia app: CipSoft-Tibia/Qt5 (third_party/qt5-cip) built from
# source. Identity is the submodule commit SHA; human label stays 6.10.3.
#
#   QT_ROOT          install root (default ~/Qt)
#   QT_CIP_SHA       override submodule SHA (CI / testing)
#   QT_EMSDK_VERSION emsdk for wasm kits (must match CIP / prior aqt pin)
#   QT_GHCR_IMAGE    GHCR repository for warm prefixes (default ghcr.io/qtclient/qt-cip)
QT_VERSION_LABEL="${QT_VERSION_LABEL:-6.10.3}"
# Back-compat for scripts that still expand $QT_VERSION in paths/logs.
QT_VERSION="${QT_VERSION:-$QT_VERSION_LABEL}"
QT_EMSDK_VERSION="${QT_EMSDK_VERSION:-4.0.7}"
QT_GHCR_IMAGE="${QT_GHCR_IMAGE:-ghcr.io/qtclient/qt-cip}"

# Modules passed to configure -skip (everything else we do not need for tibia).
# Keep in sync with find_package(Qt6 …) in client/tibia/CMakeLists.txt.
# Skip qtmultimedia: tibia does not link it, and a host FFmpeg (often x86_64
# under /usr/local on Apple Silicon) breaks the arm64 link of libffmpegmediaplugin.
QT_SKIP_MODULES="${QT_SKIP_MODULES:-qtwebengine qtwebview qtmultimedia qt3d qtcharts qtdatavis3d qtgraphs qtquick3d qtquick3dphysics qtquickeffectmaker qtquicktimeline qtlottie qtlocation qtpositioning qtsensors qtconnectivity qtserialbus qtserialport qtcoap qtmqtt qtopcua qtgrpc qthttpserver qtremoteobjects qtscxml qtspeech qtvirtualkeyboard qtwayland qtactiveqt qttools qtdoc qttranslations qtqa qtrepotools}"

# Worktree dirs kept via sparse-checkout (inverse of QT_SKIP_MODULES + cmake/coin).
# Without this, a full qt5-cip checkout is multi-GB and IDE git status times out
# ("too many active changes") once skipped module trees are deleted.
QT_SPARSE_MODULES="${QT_SPARSE_MODULES:-cmake coin qtbase qtsvg qtdeclarative qtimageformats qtshadertools qt5compat qtnetworkauth qtwebsockets qtwebchannel qtlanguageserver}"

_qt_here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
_qt_otc="$(cd "$_qt_here/../.." && pwd)"
QT_CIP_SRC="${QT_CIP_SRC:-$_qt_otc/third_party/qt5-cip}"

# Cone sparse-checkout so skipped modules never populate the worktree.
qt_cip_sparse_checkout() {
  local src="${1:-$QT_CIP_SRC}"
  [[ -d "$src" ]] || return 0
  if [[ ! -d "$src/.git" && ! -f "$src/.git" ]]; then
    return 0
  fi
  # Already matching — avoid rewriting sparse-checkout on every make qt.
  if git -C "$src" sparse-checkout list >/dev/null 2>&1; then
    local cur want
    cur="$(git -C "$src" sparse-checkout list 2>/dev/null | tr '\n' ' ' | xargs)"
    want="$(echo $QT_SPARSE_MODULES | tr ' ' '\n' | sort | tr '\n' ' ' | xargs)"
    cur="$(echo "$cur" | tr ' ' '\n' | sort | tr '\n' ' ' | xargs)"
    [[ "$cur" == "$want" ]] && return 0
  fi
  echo "[qt] sparse-checkout qt5-cip → $QT_SPARSE_MODULES" >&2
  git -C "$src" sparse-checkout init --cone
  # Word-split intentionally (module list).
  # shellcheck disable=SC2086
  git -C "$src" sparse-checkout set $QT_SPARSE_MODULES
}

qt_cip_sha() {
  qt_resolve_cip_sha
}

# Resolve CIP Qt SHA: QT_CIP_SHA env → checked-out qt5-cip → gitlink → SUBMODULE_PINS.txt.
qt_resolve_cip_sha() {
  if [[ -n "${QT_CIP_SHA:-}" ]]; then
    echo "$QT_CIP_SHA"
    return
  fi
  if [[ -d "$QT_CIP_SRC/.git" ]] || [[ -f "$QT_CIP_SRC/.git" ]]; then
    git -C "$QT_CIP_SRC" rev-parse HEAD 2>/dev/null && return
  fi
  local sha
  sha="$(git -C "$_qt_otc" rev-parse "HEAD:third_party/qt5-cip" 2>/dev/null || true)"
  if [[ -n "$sha" ]]; then
    echo "$sha"
    return
  fi
  if [[ -f "$_qt_otc/third_party/SUBMODULE_PINS.txt" ]]; then
    awk '/^qt5-cip/{print $2; exit}' "$_qt_otc/third_party/SUBMODULE_PINS.txt"
  fi
}

qt_kit_slug() {
  # Desktop host kit slug for GHCR / prefix layout.
  case "$(uname -s)" in
    Darwin)
      case "$(uname -m)" in
        arm64|aarch64) echo "macos-arm64" ;;
        *) echo "macos-x64" ;;
      esac
      ;;
    Linux)
      case "$(uname -m)" in
        aarch64|arm64) echo "linux-arm64" ;;
        *) echo "linux-x64" ;;
      esac
      ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows-x64" ;;
    *) echo "unknown" ;;
  esac
}

qt_host_dir_name() {
  # Install directory name under cip-<sha>/ (CMake-friendly, mirrors old aqt layout).
  case "$(uname -s)" in
    Darwin) echo "macos" ;;
    Linux) echo "gcc_64" ;;
    MINGW*|MSYS*|CYGWIN*) echo "msvc2022_64" ;;
    *) echo "host" ;;
  esac
}

# Git Bash `tar -C` / `mkdir` choke on Actions `D:\a\_temp\…` (backslash escapes).
# Normalize to /d/a/_temp/… for bash tools; qt-prefix.sh still emits cygpath -m for CMake.
qt_unix_path() {
  local p="${1:-}"
  [[ -n "$p" ]] || { echo "$p"; return 0; }
  if command -v cygpath >/dev/null 2>&1; then
    case "$p" in
      [A-Za-z]:*|*\\*) cygpath -u "$p" ;;
      *) echo "$p" ;;
    esac
  else
    echo "$p"
  fi
}

# Install directory under cip-<sha>/ for a GHCR kit slug.
qt_install_dir_for_kit() {
  case "${1:-}" in
    wasm_multithread) echo "wasm_multithread" ;;
    macos-arm64|macos-x64) echo "macos" ;;
    linux-x64|linux-arm64) echo "gcc_64" ;;
    windows-x64) echo "msvc2022_64" ;;
    "") echo "host" ;;
    *) echo "$1" ;;
  esac
}

qt_prefix_has_qt6() {
  [[ -f "${1:-}/lib/cmake/Qt6/Qt6Config.cmake" ]]
}

# Unpack a CIP Qt prefix archive into dest (shared by GHCR + release pulls).
qt_unpack_prefix_archive() {
  local archive="${1:-}" dest="${2:-}"
  [[ -n "$archive" && -n "$dest" && -f "$archive" ]] || return 1
  mkdir -p "$dest"
  case "$archive" in
    *.zst|*.tar.zst)
      if command -v zstd >/dev/null; then
        zstd -d -c "$archive" | tar -xf - -C "$dest"
      elif tar --zstd -xf "$archive" -C "$dest"; then
        :
      else
        echo "[qt] need zstd to unpack $archive" >&2
        return 1
      fi
      ;;
    *) tar -xzf "$archive" -C "$dest" ;;
  esac
}
