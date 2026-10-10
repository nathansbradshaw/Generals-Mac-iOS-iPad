// GeneralsX @feature Codex 09/10/2026 App-local Vulkan driver fallback for
// Qualcomm devices whose system driver lacks the engine's Vulkan 1.3 features.
// No system files or other apps' drivers are changed.
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <jni.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <cstring>

namespace {
std::once_flag loadOnce;
PFN_vkGetInstanceProcAddr realGetProc = nullptr;
std::string driverStatus;
bool customDriver = false;

bool queryDevice(PFN_vkGetInstanceProcAddr proc, VkPhysicalDeviceProperties& props) {
    auto create = reinterpret_cast<PFN_vkCreateInstance>(proc(nullptr, "vkCreateInstance"));
    if (!create) return false;
    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "GeneralsX driver detection";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    if (create(&info, nullptr, &instance) != VK_SUCCESS) return false;
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(proc(instance, "vkEnumeratePhysicalDevices"));
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(proc(instance, "vkGetPhysicalDeviceProperties"));
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(proc(instance, "vkDestroyInstance"));
    uint32_t count = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    bool found = false;
    if (enumerate && properties) {
        VkResult result = enumerate(instance, &count, &physical);
        found = (result == VK_SUCCESS || result == VK_INCOMPLETE) && count > 0;
        if (found) properties(physical, &props);
    }
    if (destroy) destroy(instance, nullptr);
    return found;
}

void loadDriver() {
    void* system = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!system) { driverStatus = "Cannot load Android Vulkan loader"; return; }
    realGetProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(system, "vkGetInstanceProcAddr"));
    if (!realGetProc) { driverStatus = "Android Vulkan loader has no entry point"; return; }
    VkPhysicalDeviceProperties properties = {};
    if (!queryDevice(realGetProc, properties)) { driverStatus = "System Vulkan has no physical device"; return; }
    driverStatus = std::string("System driver: ") + properties.deviceName;
    // Supported stock drivers retain their normal path, including Mali devices.
    if (properties.apiVersion >= VK_API_VERSION_1_3 || properties.vendorID != 0x5143)
        return;
    const char* directory = std::getenv("GENERALSX_VULKAN_HOOK_DIR");
    if (!directory || !*directory) { driverStatus += "; app-local driver directory unavailable"; return; }
    std::string root(directory);
    if (root.back() != '/') root += '/';
    void* tools = dlopen((root + "libadrenotools.so").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!tools) { driverStatus += "; Adreno loader unavailable"; return; }
    using OpenDriver = void* (*)(int, int, const char*, const char*, const char*, const char*, const char*, void**);
    auto openDriver = reinterpret_cast<OpenDriver>(dlsym(tools, "adrenotools_open_libvulkan"));
    if (!openDriver) { driverStatus += "; Adreno loader entry point unavailable"; return; }
    // ADRENOTOOLS_DRIVER_CUSTOM=1. The directory is the application's private
    // nativeLibraryDir; it holds the packaged hooks and the pinned Mesa driver.
    void* library = openDriver(RTLD_NOW | RTLD_LOCAL, 1, nullptr, directory,
        root.c_str(), "libvulkan_freedreno.so", nullptr, nullptr);
    if (!library) { driverStatus += "; app-local Vulkan loader failed"; return; }
    auto proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
    VkPhysicalDeviceProperties replacement = {};
    if (!proc || !queryDevice(proc, replacement) || replacement.apiVersion < VK_API_VERSION_1_3) {
        driverStatus += "; app-local driver did not expose Vulkan 1.3";
        return;
    }
    realGetProc = proc;
    customDriver = true;
    driverStatus = std::string("App-local driver: ") + replacement.deviceName;
}

PFN_vkGetInstanceProcAddr driverProc() {
    std::call_once(loadOnce, [] {
        loadDriver();
        std::fprintf(stderr, "[GeneralsX Vulkan] %s; custom=%d\n", driverStatus.c_str(), customDriver);
    });
    return realGetProc;
}
}

// GeneralsX @bugfix Codex 09/10/2026 SDL Android can request a replacement
// surface while its Java SurfaceView is absent. Never send a null native window
// into Android's Vulkan loader; return a recoverable surface-loss result.
static VKAPI_ATTR VkResult VKAPI_CALL createAndroidSurface(
        VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* info,
        const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    if (!info || !info->window) {
        if (surface) *surface = VK_NULL_HANDLE;
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    auto proc = driverProc();
    auto create = proc ? reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        proc(instance, "vkCreateAndroidSurfaceKHR")) : nullptr;
    return create ? create(instance, info, allocator, surface)
                  : VK_ERROR_EXTENSION_NOT_PRESENT;
}

// SDL and DXVK both load this bridge, ensuring surface creation and rendering
// use the same Vulkan loader instance and driver dispatch tables.
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    auto proc = driverProc();
    if (proc && instance && name && std::strcmp(name, "vkCreateAndroidSurfaceKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(createAndroidSurface);
    return proc ? proc(instance, name) : nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nathanbradshaw_generalsxzh_VulkanProbeActivity_probeNative(JNIEnv* env, jclass, jstring directory) {
    const char* path = env->GetStringUTFChars(directory, nullptr);
    if (!path) return nullptr;
    setenv("GENERALSX_VULKAN_HOOK_DIR", path, 1);
    env->ReleaseStringUTFChars(directory, path);
    auto proc = driverProc();
    VkPhysicalDeviceProperties props = {};
    std::string report = driverStatus + "\ncustom=" + (customDriver ? "true" : "false");
    if (proc && queryDevice(proc, props)) {
        report += "\nVulkan " + std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
            std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.apiVersion));
        report += "\nVulkan 1.3 device=" + std::string(props.apiVersion >= VK_API_VERSION_1_3 ? "yes" : "no");
    }
    if (proc) {
        auto create = reinterpret_cast<PFN_vkCreateInstance>(proc(nullptr, "vkCreateInstance"));
        VkApplicationInfo app = {};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &app;
        VkInstance instance = VK_NULL_HANDLE;
        VkResult result = create ? create(&info, nullptr, &instance) : VK_ERROR_INITIALIZATION_FAILED;
        report += "\ncreateInstance(1.3)=" + std::to_string(result);
        if (result == VK_SUCCESS) {
            auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(proc(instance, "vkEnumeratePhysicalDevices"));
            auto getFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(proc(instance, "vkGetPhysicalDeviceFeatures2"));
            uint32_t count = 1;
            VkPhysicalDevice physical = VK_NULL_HANDLE;
            if (enumerate && getFeatures && enumerate(instance, &count, &physical) == VK_SUCCESS && count) {
                VkPhysicalDeviceVulkan13Features f13 = {};
                f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                VkPhysicalDeviceVulkan12Features f12 = {};
                f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                f12.pNext = &f13;
                VkPhysicalDeviceFeatures2 features = {};
                features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                features.pNext = &f12;
                getFeatures(physical, &features);
                report += "\ntimelineSemaphore=" + std::to_string(f12.timelineSemaphore);
                report += "\ndynamicRendering=" + std::to_string(f13.dynamicRendering);
                report += "\nsynchronization2=" + std::to_string(f13.synchronization2);
            }
            auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(proc(instance, "vkDestroyInstance"));
            if (destroy) destroy(instance, nullptr);
        }
    }
    std::fprintf(stderr, "[GeneralsX Vulkan probe] %s\n", report.c_str());
    return env->NewStringUTF(report.c_str());
}
