package com.nathanbradshaw.generalsxzh;

import android.system.Os;
import org.libsdl.app.SDLActivity;

// GeneralsX @build Codex 08/10/2026 Fixed debug-only join path for invisible emulator tests.
public final class OnlineTestActivity extends GeneralsXActivity {
    @Override
    protected String[] getArguments() {
        super.getArguments();
        try {
            // GeneralsX @build Codex 08/10/2026 Keep generated shaders for physical GPU crash diagnosis.
            java.io.File shaderDump = new java.io.File(getFilesDir(), "shader-dump");
            if (!shaderDump.isDirectory() && !shaderDump.mkdirs()) {
                throw new IllegalStateException("Cannot create private shader diagnostic directory");
            }
            Os.setenv("DXVK_SHADER_DUMP_PATH", shaderDump.getAbsolutePath(), true);
            // GeneralsX @build Codex 09/10/2026 Keep hosted relay configuration
            // private and opt-in; never pass credentials through Activity extras.
            java.io.File networkFile = new java.io.File(getFilesDir(), "online-test-network.json");
            if (networkFile.isFile()) {
                String json = new String(java.nio.file.Files.readAllBytes(networkFile.toPath()),
                        java.nio.charset.StandardCharsets.UTF_8);
                org.json.JSONObject network = new org.json.JSONObject(json);
                for (String key : new String[] {"GENERALSX_ONLINE_URL", "GENERALSX_ONLINE_STUN_SERVERS", "GENERALSX_ONLINE_TURN_SERVERS"}) {
                    String value = network.optString(key, "");
                    if (!value.isEmpty()) Os.setenv(key, value, true);
                }
            }
            // GeneralsX @build Codex 09/10/2026 Exercise normal phone menus and
            // touch commands with a longer diagnostic trace when requested.
            boolean manual = getIntent().getBooleanExtra("manual", false);
            // GeneralsX @build Codex 09/10/2026 Join the owned combat fixture
            // automatically while leaving team/army/Ready under phone control.
            if (getIntent().getBooleanExtra("autoReady", !manual))
                Os.setenv("GENERALSX_AUTOREADY", "1", true);
            String commands = getIntent().getStringExtra("commands");
            if (commands == null) commands = manual ? "" : "surrender";
            Os.setenv("GENERALSX_SMOKE_COMMANDS", commands, true);
            // GeneralsX @build Codex 09/10/2026 Capture beyond relay nonce expiry for sustained device validation.
            int endFrame = getIntent().getIntExtra("traceEndFrame", 2100);
            if (endFrame < 0 || endFrame > 30000)
                throw new IllegalArgumentException("traceEndFrame must be within 0..30000");
            Os.setenv("GENERALSX_CRC_TRACE_END_FRAME", Integer.toString(endFrame), true);
            Os.setenv("GENERALSX_CRC_TRACE", getFilesDir().getAbsolutePath() + "/online-crc", true);
        } catch (android.system.ErrnoException | java.io.IOException | org.json.JSONException error) {
            throw new IllegalStateException("Cannot enable online CRC tracing", error);
        }
        boolean join = !getIntent().getBooleanExtra("manual", false)
                || getIntent().getBooleanExtra("autoJoin", false);
        return join ? new String[] {"-joinAutostart", "-nologo", "-noshellmap"}
                    : new String[] {"-nologo", "-noshellmap"};
    }
}
