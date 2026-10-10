package com.nathanbradshaw.generalsxzh;

import android.system.Os;
import org.libsdl.app.SDLActivity;

// GeneralsX @build Codex 08/10/2026 Debug-only, fixed-input native replay test.
// Runs the provisioned replay without creating the engine's SDL/Vulkan window.
public final class ReplayTestActivity extends GeneralsXActivity {
    @Override
    protected String[] getArguments() {
        super.getArguments();
        try {
            Os.setenv("GENERALSX_CRC_TRACE", getFilesDir().getAbsolutePath() + "/replay-crc", true);
            // GeneralsX @build Codex 09/10/2026 Preserve bounded extended state
            // traces for reproducing sustained cross-platform match failures.
            // GeneralsX @build Codex 09/10/2026 Capture beyond relay nonce expiry for sustained device validation.
            int endFrame = getIntent().getIntExtra("traceEndFrame", 2100);
            if (endFrame < 0 || endFrame > 30000) {
                throw new IllegalArgumentException("traceEndFrame must be within 0..30000");
            }
            Os.setenv("GENERALSX_CRC_TRACE_END_FRAME", Integer.toString(endFrame), true);

        } catch (android.system.ErrnoException error) {
            throw new IllegalStateException("Cannot enable replay CRC tracing", error);
        }
        return new String[] {"-headless", "-replay", "crossplay-test.rep"};
    }
}
