#!/usr/bin/env bash
# GeneralsX @build Codex 09/10/2026 Pinned app-local Turnip driver and loader for Android ARM64.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DRIVER_ROOT="${GX_ANDROID_DRIVER_ROOT:-${PROJECT_ROOT}/build/android-turnip}"
ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
NDK="${ANDROID_NDK_HOME:-${ANDROID_HOME}/ndk/28.2.13676358}"
API=30
MESA_VERSION=24.3.4
MESA_SHA=e641ae27191d387599219694560d221b7feaa91c900bcec46bf444218ed66025
ADRENO_COMMIT=8fae8ce254dfc1344527e05301e43f37dea2df80
BISON="${GX_BISON:-$(command -v bison)}"
if ! "$BISON" --version | head -n 1 | grep -Eq ' (3|[4-9])\.'; then
    echo 'ERROR: Mesa needs Bison >=3. Set GX_BISON to a local modern Bison executable.' >&2
    exit 1
fi
case "$(uname -s)" in
 Darwin) HOST_TAG=darwin-x86_64 ;;
 Linux) HOST_TAG=linux-x86_64 ;;
 *) echo 'ERROR: supported build hosts are macOS and Linux.' >&2; exit 1 ;;
esac
LLVM="${NDK}/toolchains/llvm/prebuilt/${HOST_TAG}/bin"
JNI="${PROJECT_ROOT}/android/app/build/generated/jniLibs/arm64-v8a"
mkdir -p "$DRIVER_ROOT" "$DRIVER_ROOT/empty-pkgconfig" "$JNI"
if [[ ! -f "$DRIVER_ROOT/mesa-${MESA_VERSION}.tar.xz" ]]; then
    curl --fail --location --retry 3 "https://archive.mesa3d.org/mesa-${MESA_VERSION}.tar.xz" -o "$DRIVER_ROOT/mesa-${MESA_VERSION}.tar.xz"
fi
printf '%s  %s\n' "$MESA_SHA" "$DRIVER_ROOT/mesa-${MESA_VERSION}.tar.xz" | shasum -a 256 -c -
if [[ ! -d "$DRIVER_ROOT/mesa-${MESA_VERSION}" ]]; then
    tar -xf "$DRIVER_ROOT/mesa-${MESA_VERSION}.tar.xz" -C "$DRIVER_ROOT"
fi
if [[ ! -d "$DRIVER_ROOT/libadrenotools/.git" ]]; then
    git clone https://github.com/bylaws/libadrenotools.git "$DRIVER_ROOT/libadrenotools"
fi
if [[ "$(git -C "$DRIVER_ROOT/libadrenotools" rev-parse HEAD)" != "$ADRENO_COMMIT" ]]; then
    git -C "$DRIVER_ROOT/libadrenotools" fetch origin "$ADRENO_COMMIT"
    git -C "$DRIVER_ROOT/libadrenotools" checkout --detach "$ADRENO_COMMIT"
fi
git -C "$DRIVER_ROOT/libadrenotools" submodule update --init --recursive
if [[ ! -x "$DRIVER_ROOT/.venv/bin/meson" ]]; then
    python3 -m venv "$DRIVER_ROOT/.venv"
    "$DRIVER_ROOT/.venv/bin/pip" install meson==1.7.0 mako==1.4.3 PyYAML==6.0.3 packaging==26.3
fi
export PATH="$DRIVER_ROOT/.venv/bin:$PATH"
cat > "$DRIVER_ROOT/android30.ini" <<CROSS
[binaries]
bison = '$BISON'
c = '$LLVM/aarch64-linux-android${API}-clang'
cpp = ['$LLVM/aarch64-linux-android${API}-clang++', '-static-libstdc++']
ar = '$LLVM/llvm-ar'
strip = '$LLVM/llvm-strip'
pkg-config = '$(command -v pkg-config)'
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
[properties]
pkg_config_libdir = '$DRIVER_ROOT/empty-pkgconfig'
needs_exe_wrapper = true
[built-in options]
c_args = ['-Wno-error', '-fPIC']
cpp_args = ['-Wno-error', '-fPIC']
c_link_args = ['-Wl,-z,max-page-size=16384']
cpp_link_args = ['-Wl,-z,max-page-size=16384']
CROSS
MESON_ARGS=(setup "$DRIVER_ROOT/mesa-build-api30" "$DRIVER_ROOT/mesa-${MESA_VERSION}" --cross-file "$DRIVER_ROOT/android30.ini" --buildtype release -Dplatforms=android -Dplatform-sdk-version=30 -Dandroid-stub=true -Dandroid-libbacktrace=disabled -Degl=disabled -Dgallium-drivers= -Dvulkan-drivers=freedreno -Dfreedreno-kmds=kgsl -Dllvm=disabled -Dzstd=disabled -Dexpat=disabled -Dglx=disabled -Dgbm=disabled -Dgles1=disabled -Dgles2=disabled -Dopengl=false -Dbuild-tests=false -Ddefault_library=static)
if [[ -f "$DRIVER_ROOT/mesa-build-api30/meson-private/coredata.dat" ]]; then MESON_ARGS+=(--reconfigure --clearcache); fi
meson "${MESON_ARGS[@]}"
ninja -j 2 -C "$DRIVER_ROOT/mesa-build-api30" src/freedreno/vulkan/libvulkan_freedreno.so
cmake -S "$DRIVER_ROOT/libadrenotools" -B "$DRIVER_ROOT/adrenotools-build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build "$DRIVER_ROOT/adrenotools-build" -j 2
cmake -S "$PROJECT_ROOT/cmake/android-vulkan-loader" -B "$PROJECT_ROOT/build/android-vulkan-loader" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build "$PROJECT_ROOT/build/android-vulkan-loader" -j 2
cp "$DRIVER_ROOT/mesa-build-api30/src/freedreno/vulkan/libvulkan_freedreno.so" "$DRIVER_ROOT/adrenotools-build/libadrenotools.so" "$PROJECT_ROOT/build/android-vulkan-loader/libgeneralsx_vulkan.so" "$JNI/"
for library in main_hook hook_impl file_redirect_hook gsl_alloc_hook; do
    cp "$DRIVER_ROOT/adrenotools-build/src/hook/lib${library}.so" "$JNI/"
done
NOTICES="$PROJECT_ROOT/android/app/src/main/assets/third-party-vulkan"
mkdir -p "$NOTICES/mesa"
cp "$DRIVER_ROOT/libadrenotools/LICENSE" "$NOTICES/adrenotools-LICENSE"
cp "$DRIVER_ROOT/libadrenotools/lib/linkernsbypass/LICENSE" "$NOTICES/linkernsbypass-LICENSE"
cp "$DRIVER_ROOT/mesa-${MESA_VERSION}/docs/license.rst" "$NOTICES/mesa/"
cp -R "$DRIVER_ROOT/mesa-${MESA_VERSION}/licenses/." "$NOTICES/mesa/"
printf 'Mesa %s source sha256 %s\nlibadrenotools commit %s\n' "$MESA_VERSION" "$MESA_SHA" "$ADRENO_COMMIT" > "$NOTICES/versions.txt"
echo 'Android app-local Vulkan driver, hooks, bridge and license notices staged.'
