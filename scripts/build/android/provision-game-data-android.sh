#!/usr/bin/env bash
# Provision user-owned Zero Hour retail data into Android app-private storage.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
ADB="${ANDROID_HOME}/platform-tools/adb"
PACKAGE="com.nathanbradshaw.generalsxzh"
GAME_DATA_SRC="${GX_GAME_DATA:-${HOME}/GeneralsX/GeneralsZH}"
FONTS_SRC="${GX_FONTS:-${HOME}/GeneralsX/ios-staging/fonts}"
CONFIG_SRC="${PROJECT_ROOT}/ios/config"
REPO_EXTRAS_MENU="${PROJECT_ROOT}/GeneralsZH/Data/Window/Menus/ExtrasMenu.wnd"

require_path() {
    if [[ ! -e "$1" ]]; then
        echo "ERROR: Missing $2: $1" >&2
        exit 1
    fi
}

require_path "${ADB}" "adb"
require_path "${GAME_DATA_SRC}" "staged Zero Hour retail data"
require_path "${FONTS_SRC}" "staged mobile fonts"
require_path "${CONFIG_SRC}/dxvk.conf" "DXVK configuration"
require_path "${CONFIG_SRC}/Options.ini" "default options"
require_path "${REPO_EXTRAS_MENU}" "GeneralsX Extras menu"

if ! "${ADB}" get-state >/dev/null 2>&1; then
    echo "ERROR: No running Android device/emulator was found" >&2
    exit 1
fi
if ! "${ADB}" shell run-as "${PACKAGE}" true >/dev/null 2>&1; then
    echo "ERROR: ${PACKAGE} is not installed as a debuggable app." >&2
    echo "  Build and install the debug APK before provisioning data." >&2
    exit 1
fi

echo "Stopping GeneralsXZH before updating its game data"
"${ADB}" shell am force-stop "${PACKAGE}"
"${ADB}" shell run-as "${PACKAGE}" mkdir -p \
    files/GameData/fonts files/GameData/Window/Menus

echo "Provisioning user-owned Zero Hour data from ${GAME_DATA_SRC}"
echo "This is a multi-gigabyte transfer and may take several minutes."
while IFS= read -r -d '' entry; do
    name="$(basename "${entry}")"
    case "${name}" in
        .*|*.dylib|*.so|*.DLL|*.dll|*.dat|*.ico|*.bmp|*.doc|*.lcf|*.txt|\
        run.sh|GeneralsXZH|GeneralsXZH.dxvk-cache|*_d3d9.log|\
        MoltenVK_icd.json|dxvk.conf|fontconfig|Launcher.txt|MSS|Manuals|\
        steamapps|steam_appid.txt|00000000.*|RedistInstallers|_CommonRedist)
            continue
            ;;
    esac
    echo "  ${name}"
    tar -C "${GAME_DATA_SRC}" -cf - \
        --exclude='.*' --exclude='*.dylib' --exclude='*.so' \
        --exclude='*.DLL' --exclude='*.dll' --exclude='*.dat' \
        --exclude='*.ico' --exclude='*.bmp' --exclude='*.doc' \
        --exclude='*.lcf' --exclude='*.txt' \
        "${name}" | "${ADB}" shell run-as "${PACKAGE}" \
            /system/bin/tar -xf - -C files/GameData
done < <(find "${GAME_DATA_SRC}" -mindepth 1 -maxdepth 1 -print0)

tar -C "${FONTS_SRC}" -cf - . | \
    "${ADB}" shell run-as "${PACKAGE}" /system/bin/tar -xf - -C files/GameData/fonts
tar -C "${CONFIG_SRC}" -cf - dxvk.conf Options.ini | \
    "${ADB}" shell run-as "${PACKAGE}" /system/bin/tar -xf - -C files/GameData
"${ADB}" shell run-as "${PACKAGE}" cp \
    files/GameData/Options.ini files/GameData/DefaultOptions.ini
tar -C "$(dirname "${REPO_EXTRAS_MENU}")" -cf - \
    "$(basename "${REPO_EXTRAS_MENU}")" | \
    "${ADB}" shell run-as "${PACKAGE}" /system/bin/tar -xf - -C files/GameData/Window/Menus

# GeneralsX @build Codex 09/10/2026 Provision a maintained public TLS trust
# bundle after game data; verify bytes before replacing the active bundle.
python3 "${SCRIPT_DIR}/provision-ca-bundle-android.py"

echo "Android game data is ready: app-private files/GameData"
echo "Launch with: ${SCRIPT_DIR}/install-run-android-zh.sh"
