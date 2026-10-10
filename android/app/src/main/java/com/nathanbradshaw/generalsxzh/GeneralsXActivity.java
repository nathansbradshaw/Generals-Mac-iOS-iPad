package com.nathanbradshaw.generalsxzh;

import android.system.Os;
import org.libsdl.app.SDLActivity;

// GeneralsX @feature Codex 09/10/2026 Share the app-local Vulkan loader between
// SDL surfaces and DXVK; supported stock drivers keep their normal GPU path.
public class GeneralsXActivity extends SDLActivity {
    // GeneralsX @bugfix Codex 09/10/2026 Hide system bars before the SDL thread
    // creates its Vulkan surface, avoiding a stale drawable with shifted touches.
    @Override protected void onCreate(android.os.Bundle savedState) {
        super.onCreate(savedState);
        applyFullscreen();
    }

    // GeneralsX @bugfix Codex 09/10/2026 SDL's style command is queued. Apply
    // the same decor flags synchronously before the first SurfaceView layout.
    private void applyFullscreen() {
        android.view.Window window = getWindow();
        window.getDecorView().setSystemUiVisibility(
            android.view.View.SYSTEM_UI_FLAG_FULLSCREEN |
            android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        android.view.WindowManager.LayoutParams attributes = window.getAttributes();
        attributes.layoutInDisplayCutoutMode =
            android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        window.setAttributes(attributes);
        // GeneralsX @bugfix Codex 09/10/2026 Keep the consumer buffer at the
        // game's native landscape resolution from its first layout. Transient
        // system-bar insets must not lock Vulkan into a smaller drawable.
        android.view.Display display = getDisplay();
        if (mSurface != null && display != null) {
            android.view.Display.Mode mode = display.getMode();
            int width = Math.max(mode.getPhysicalWidth(), mode.getPhysicalHeight());
            int height = Math.min(mode.getPhysicalWidth(), mode.getPhysicalHeight());
            mSurface.getHolder().setFixedSize(width, height);
        }
        setWindowStyle(true);
    }

    // GeneralsX @bugfix Codex 09/10/2026 D3D uses a windowed presentation on
    // Android, but SDL's corresponding style command must keep the Activity
    // immersive. Otherwise entering a match or closing the keyboard restores
    // system bars and shrinks the SurfaceView underneath its fixed buffer.
    @Override protected boolean sendCommand(int command, Object data) {
        if (command == COMMAND_CHANGE_WINDOW_STYLE) {
            data = Integer.valueOf(1);
        }
        return super.sendCommand(command, data);
    }

    @Override protected void onResume() {
        super.onResume();
        applyFullscreen();
    }

    @Override protected String[] getArguments() {
        java.io.File bridge = new java.io.File(getApplicationInfo().nativeLibraryDir, "libgeneralsx_vulkan.so");
        if (bridge.isFile()) {
            try {
                Os.setenv("GENERALSX_VULKAN_HOOK_DIR", getApplicationInfo().nativeLibraryDir, true);
                Os.setenv("GENERALSX_VULKAN_LIBRARY", bridge.getAbsolutePath(), true);
                Os.setenv("SDL_VULKAN_LIBRARY", bridge.getAbsolutePath(), true);
            } catch (android.system.ErrnoException error) {
                throw new IllegalStateException("Cannot configure app-local Vulkan loader", error);
            }
        }
        return super.getArguments();
    }
}
