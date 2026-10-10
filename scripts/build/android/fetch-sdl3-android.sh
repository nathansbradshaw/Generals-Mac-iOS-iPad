#!/usr/bin/env bash
# Download the pinned official SDL3 and SDL3_image Android AARs.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SDL_VERSION="3.4.2"
SDL_ARCHIVE="SDL3-devel-${SDL_VERSION}-android.zip"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/${SDL_ARCHIVE}"
SDL_ARCHIVE_SHA256="7266be52ecebd1ddc8f1d35df87e1ff7e555a767e5d73409627975bc1548a07a"
SDL_AAR_SHA256="7eeda8ba7811a417025c197c830a1893db73490f24d38cd5bb9f71295b0228a2"
SDL_AAR="${PROJECT_ROOT}/android/app/libs/SDL3-${SDL_VERSION}.aar"
SDL_IMAGE_VERSION="3.4.0"
SDL_IMAGE_ARCHIVE="SDL3_image-devel-${SDL_IMAGE_VERSION}-android.zip"
SDL_IMAGE_URL="https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL_IMAGE_VERSION}/${SDL_IMAGE_ARCHIVE}"
SDL_IMAGE_ARCHIVE_SHA256="f544e52f2545a13062343171c04fc3c3f488d61c7e8ce2a6204759ba668d7cce"
SDL_IMAGE_AAR_SHA256="d907141a47a9f7da6f0eddd4cc2489032a7131022dca6ebc02d0ff1661b2347b"
SDL_IMAGE_AAR="${PROJECT_ROOT}/android/app/libs/SDL3_image-${SDL_IMAGE_VERSION}.aar"
CACHE_DIR="${PROJECT_ROOT}/build/android-downloads"
CACHE_ARCHIVE="${CACHE_DIR}/${SDL_ARCHIVE}"
SDL_IMAGE_CACHE_ARCHIVE="${CACHE_DIR}/${SDL_IMAGE_ARCHIVE}"

sha256_file() {
    shasum -a 256 "$1" | awk '{print $1}'
}

mkdir -p "${CACHE_DIR}" "$(dirname "${SDL_AAR}")"
if [[ ! -f "${SDL_AAR}" ]] || [[ "$(sha256_file "${SDL_AAR}")" != "${SDL_AAR_SHA256}" ]]; then
    if [[ ! -f "${CACHE_ARCHIVE}" ]] || [[ "$(sha256_file "${CACHE_ARCHIVE}")" != "${SDL_ARCHIVE_SHA256}" ]]; then
        echo "Downloading SDL3 ${SDL_VERSION} Android development archive"
        curl --fail --location --retry 3 --output "${CACHE_ARCHIVE}" "${SDL_URL}"
    fi
    if [[ "$(sha256_file "${CACHE_ARCHIVE}")" != "${SDL_ARCHIVE_SHA256}" ]]; then
        echo "ERROR: SDL3 Android archive checksum mismatch" >&2
        exit 1
    fi
    unzip -o "${CACHE_ARCHIVE}" "SDL3-${SDL_VERSION}.aar" -d "$(dirname "${SDL_AAR}")" >/dev/null
fi
echo "SDL3 Android AAR is ready: ${SDL_AAR}"

if [[ ! -f "${SDL_IMAGE_AAR}" ]] || [[ "$(sha256_file "${SDL_IMAGE_AAR}")" != "${SDL_IMAGE_AAR_SHA256}" ]]; then
    if [[ ! -f "${SDL_IMAGE_CACHE_ARCHIVE}" ]] || [[ "$(sha256_file "${SDL_IMAGE_CACHE_ARCHIVE}")" != "${SDL_IMAGE_ARCHIVE_SHA256}" ]]; then
        echo "Downloading SDL3_image ${SDL_IMAGE_VERSION} Android development archive"
        curl --fail --location --retry 3 --output "${SDL_IMAGE_CACHE_ARCHIVE}" "${SDL_IMAGE_URL}"
    fi
    if [[ "$(sha256_file "${SDL_IMAGE_CACHE_ARCHIVE}")" != "${SDL_IMAGE_ARCHIVE_SHA256}" ]]; then
        echo "ERROR: SDL3_image Android archive checksum mismatch" >&2
        exit 1
    fi
    unzip -o "${SDL_IMAGE_CACHE_ARCHIVE}" "SDL3_image-${SDL_IMAGE_VERSION}.aar" -d "$(dirname "${SDL_IMAGE_AAR}")" >/dev/null
fi

if [[ "$(sha256_file "${SDL_AAR}")" != "${SDL_AAR_SHA256}" ]]; then
    echo "ERROR: SDL3 Android AAR checksum mismatch" >&2
    exit 1
fi
if [[ "$(sha256_file "${SDL_IMAGE_AAR}")" != "${SDL_IMAGE_AAR_SHA256}" ]]; then
    echo "ERROR: SDL3_image Android AAR checksum mismatch" >&2
    exit 1
fi

echo "SDL3 Android AAR is ready: ${SDL_AAR}"
echo "SDL3_image Android AAR is ready: ${SDL_IMAGE_AAR}"
