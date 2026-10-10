/*
** GeneralsX Android SDL3 bootstrap.
**
** GeneralsX @build BenderAI 13/07/2026 Prove the Android Activity, ARM64
** native library, SDL3 event loop, and renderer before attaching the full
** Zero Hour engine and DXVK native build.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#include <android/log.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace
{
constexpr const char *LOG_TAG = "GeneralsXAndroid";
void *g_dxvk_d3d8_library = nullptr;
void *g_dxvk_d3d9_library = nullptr;

bool LoadDxvkLibrary(const char *library_name, const char *entry_point, void **library_out)
{
	void *library = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);
	if (library == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to load %s: %s", library_name, dlerror());
		return false;
	}

	dlerror();
	void *entry = dlsym(library, entry_point);
	const char *symbol_error = dlerror();
	if (symbol_error != nullptr || entry == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Missing %s in %s: %s",
			entry_point, library_name, symbol_error != nullptr ? symbol_error : "unknown error");
		dlclose(library);
		return false;
	}

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Loaded %s and resolved %s", library_name, entry_point);
	*library_out = library;
	return true;
}

bool CreateVulkanSurface(SDL_Window *window, VkInstance *instance_out, VkSurfaceKHR *surface_out)
{
	Uint32 extension_count = 0;
	const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
	if (extensions == nullptr || extension_count == 0) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL Vulkan extensions unavailable: %s", SDL_GetError());
		return false;
	}

	bool has_android_surface = false;
	for (Uint32 i = 0; i < extension_count; ++i) {
		if (SDL_strcmp(extensions[i], VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == 0) {
			has_android_surface = true;
			break;
		}
	}
	if (!has_android_surface) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL did not request VK_KHR_android_surface");
		return false;
	}

	VkApplicationInfo application_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	application_info.pApplicationName = "GeneralsXZH Android Bootstrap";
	application_info.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instance_info.pApplicationInfo = &application_info;
	instance_info.enabledExtensionCount = extension_count;
	instance_info.ppEnabledExtensionNames = extensions;

	VkResult result = vkCreateInstance(&instance_info, nullptr, instance_out);
	if (result != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "vkCreateInstance failed: %d", result);
		return false;
	}

	if (!SDL_Vulkan_CreateSurface(window, *instance_out, nullptr, surface_out)) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		vkDestroyInstance(*instance_out, nullptr);
		*instance_out = VK_NULL_HANDLE;
		return false;
	}

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG,
		"Created Vulkan 1.3 instance and VK_KHR_android_surface through SDL3");
	return true;
}

bool CreateDxvkD3D8Interface()
{
	using Direct3DCreate8Proc = void *(*)(unsigned int);
	auto create_d3d8 = reinterpret_cast<Direct3DCreate8Proc>(dlsym(g_dxvk_d3d8_library, "Direct3DCreate8"));
	if (create_d3d8 == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Direct3DCreate8 lookup failed: %s", dlerror());
		return false;
	}

	// D3D_SDK_VERSION from the Direct3D 8 headers. The bootstrap retains this
	// process-lifetime object until the full engine owns the interface.
	void *d3d8 = create_d3d8(220u);
	if (d3d8 == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "DXVK Direct3DCreate8 returned null");
		return false;
	}

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "DXVK Direct3DCreate8 initialized the Vulkan adapter path");
	return true;
}
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Starting SDL3 Android bootstrap");
	SDL_SetAppMetadata("GeneralsXZH", "0.1-bootstrap", "com.nathanbradshaw.generalsxzh");

	// GeneralsX @build BenderAI 13/07/2026 Prove both Android DXVK
	// libraries and their exported Direct3D entry points load in the APK.
	if (!LoadDxvkLibrary("libdxvk_d3d9.so", "Direct3DCreate9", &g_dxvk_d3d9_library)
		|| !LoadDxvkLibrary("libdxvk_d3d8.so", "Direct3DCreate8", &g_dxvk_d3d8_library)) {
		return 2;
	}

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	SDL_Window *window = SDL_CreateWindow(
		"GeneralsXZH Android Bootstrap",
		1280,
		720,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);
	if (window == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	VkInstance vulkan_instance = VK_NULL_HANDLE;
	VkSurfaceKHR vulkan_surface = VK_NULL_HANDLE;
	if (!CreateVulkanSurface(window, &vulkan_instance, &vulkan_surface)
		|| !CreateDxvkD3D8Interface()) {
		if (vulkan_surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(vulkan_instance, vulkan_surface, nullptr);
		}
		if (vulkan_instance != VK_NULL_HANDLE) {
			vkDestroyInstance(vulkan_instance, nullptr);
		}
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 3;
	}
	vkDestroySurfaceKHR(vulkan_instance, vulkan_surface, nullptr);
	vkDestroyInstance(vulkan_instance, nullptr);
	SDL_DestroyWindow(window);

	// SDL's Android 2D renderer uses GLES and cannot share a Vulkan-tagged
	// window. Recreate a plain window only to keep the bootstrap canvas visible.
	window = SDL_CreateWindow(
		"GeneralsXZH Android Bootstrap",
		1280,
		720,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (window == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Canvas SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
	if (renderer == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_CreateRenderer failed: %s", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Bootstrap window and renderer are ready");
	bool running = true;
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (event.type == SDL_EVENT_KEY_DOWN
				&& (event.key.key == SDLK_AC_BACK || event.key.key == SDLK_ESCAPE)) {
				running = false;
			}
		}

		SDL_SetRenderDrawColor(renderer, 5, 18, 42, 255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 0, 170, 255, 255);
		SDL_FRect banner = { 80.0f, 80.0f, 1120.0f, 120.0f };
		SDL_RenderFillRect(renderer, &banner);
		SDL_RenderPresent(renderer);
		SDL_Delay(16);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "SDL3 Android bootstrap stopped cleanly");
	return 0;
}
