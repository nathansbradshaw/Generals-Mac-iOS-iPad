#!/usr/bin/env bash
# Provision a self-hosted GeneralsOnline account in the iOS app container.
#
# Usage:
#   ./scripts/go-online/provision-ios-online-account.sh
#   ./scripts/go-online/provision-ios-online-account.sh --user-id ID --name NAME
#       [--device UUID] [--bundle BUNDLE_ID] [--env-file PATH] [--no-launch]
#
# Environment:
#   GENERALSX_ONLINE_JWT_KEY  Backend JwtSettings.Key. If unset, the script
#                             loads it from the gitignored .env file.
#
# The refresh token is written to a mode-600 temporary file, copied into the
# app's private data container, and never printed.
# GeneralsX @build nathanbradshaw 13/07/2026 Add physical-iOS account provisioning.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

USER_ID=34621
DISPLAY_NAME="nathan"
BUNDLE_ID="${GX_BUNDLE_ID:-com.nathanbradshaw.generalszh}"
DEVICE_ID=""
ENV_FILE="${PROJECT_ROOT}/.env"
DO_LAUNCH=1

usage() {
    sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
}

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user-id)
            [[ $# -ge 2 ]] || fail "--user-id requires a value"
            USER_ID="$2"
            shift 2
            ;;
        --name)
            [[ $# -ge 2 ]] || fail "--name requires a value"
            DISPLAY_NAME="$2"
            shift 2
            ;;
        --device)
            [[ $# -ge 2 ]] || fail "--device requires a UUID"
            DEVICE_ID="$2"
            shift 2
            ;;
        --bundle)
            [[ $# -ge 2 ]] || fail "--bundle requires a bundle identifier"
            BUNDLE_ID="$2"
            shift 2
            ;;
        --env-file)
            [[ $# -ge 2 ]] || fail "--env-file requires a path"
            ENV_FILE="$2"
            shift 2
            ;;
        --no-launch)
            DO_LAUNCH=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown argument '$1'. Run with --help for usage."
            ;;
    esac
done

command -v python3 >/dev/null 2>&1 || fail "python3 was not found"
command -v xcrun >/dev/null 2>&1 || fail "Xcode command-line tools were not found"
[[ ${USER_ID} =~ ^[0-9]+$ ]] || fail "--user-id must be numeric"
[[ -n ${DISPLAY_NAME} ]] || fail "--name cannot be empty"

if [[ -z ${GENERALSX_ONLINE_JWT_KEY:-} ]]; then
    [[ -f ${ENV_FILE} ]] || fail "Set GENERALSX_ONLINE_JWT_KEY or create ${ENV_FILE}"
    set -a
    # shellcheck disable=SC1090
    source "${ENV_FILE}"
    set +a
fi
[[ -n ${GENERALSX_ONLINE_JWT_KEY:-} ]] || fail "GENERALSX_ONLINE_JWT_KEY is empty"

if [[ -z ${DEVICE_ID} ]]; then
    DEVICE_OUTPUT="$(xcrun devicectl list devices 2>&1)" || {
        echo "${DEVICE_OUTPUT}" >&2
        fail "Apple CoreDevice is unavailable"
    }
    DEVICE_ID="$(printf '%s\n' "${DEVICE_OUTPUT}" | grep -Ei 'connected|available \(paired\)' \
        | grep -oE '[0-9A-Fa-f-]{36}' | head -1 || true)"
fi
[[ -n ${DEVICE_ID} ]] || fail "No available paired iPhone/iPad was found"

umask 077
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/generalsx-ios-account.XXXXXX")"
trap 'rm -rf "${TEMP_DIR}"' EXIT
CREDENTIALS_FILE="${TEMP_DIR}/credentials.json"

echo "==> Minting refresh token for ${DISPLAY_NAME} (${USER_ID})"
TOKEN="$(python3 "${SCRIPT_DIR}/mint_refresh_token.py" \
    --user-id "${USER_ID}" --name "${DISPLAY_NAME}" \
    --key "${GENERALSX_ONLINE_JWT_KEY}")"
[[ -n ${TOKEN} ]] || fail "Token minting returned no data"
printf '{\n "refresh_token": "%s"\n}\n' "${TOKEN}" > "${CREDENTIALS_FILE}"
unset TOKEN

DESTINATION="Library/Application Support/GeneralsX/GeneralsZH/GeneralsOnlineData/credentials.json"
echo "==> Installing credentials in ${BUNDLE_ID} on ${DEVICE_ID}"
xcrun devicectl device copy to \
    --device "${DEVICE_ID}" \
    --domain-type appDataContainer \
    --domain-identifier "${BUNDLE_ID}" \
    --source "${CREDENTIALS_FILE}" \
    --destination "${DESTINATION}"

if [[ ${DO_LAUNCH} == 1 ]]; then
    echo "==> Relaunching GeneralsXZH"
    xcrun devicectl device process launch \
        --device "${DEVICE_ID}" --terminate-existing "${BUNDLE_ID}"
fi

echo "SUCCESS: ${DISPLAY_NAME} is provisioned for GeneralsOnline on the iOS device."
