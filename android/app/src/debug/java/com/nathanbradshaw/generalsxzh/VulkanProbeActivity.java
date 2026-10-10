package com.nathanbradshaw.generalsxzh;

// GeneralsX @build Codex 09/10/2026 Debug-only device driver query before
// loading retail data; no gameplay state or system driver changes.
public final class VulkanProbeActivity extends android.app.Activity {
    private static native String probeNative(String directory);
    @Override protected void onCreate(android.os.Bundle state) {
        super.onCreate(state);
        android.widget.TextView text = new android.widget.TextView(this);
        text.setText("Checking GeneralsX Vulkan driver…");
        text.setTextSize(20); text.setPadding(32,32,32,32); setContentView(text);
        new Thread(() -> {
            String result;
            try {
                System.loadLibrary("generalsx_vulkan");
                result = probeNative(getApplicationInfo().nativeLibraryDir);
            } catch (Throwable error) { result = "Vulkan probe failed: " + error; }
            final String report = result;
            android.util.Log.i("GeneralsXVulkanProbe", report);
            runOnUiThread(() -> text.setText(report));
        }, "GeneralsXDriverProbe").start();
    }
}
