package com.nathanbradshaw.generalsxzh;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import org.libsdl.app.SDLActivity;

// GeneralsX @build Codex 09/10/2026 Exercise multi-finger SDL input on the
// physical phone without controlling the host desktop. Debug APKs only.
public final class DebugTouchReceiver extends BroadcastReceiver {
    private static boolean running;
    private static final class SurfaceAccess extends SDLActivity {
        static View surface() { return mSurface; }
    }

    @Override public void onReceive(Context context, Intent intent) {
        View surface = SurfaceAccess.surface();
        String gesture = intent.getStringExtra("gesture");
        if (surface == null || running || !("pan".equals(gesture) ||
                "pinch".equals(gesture) || "escape".equals(gesture))) {
            setResultCode(1);
            setResultData("Game surface unavailable, gesture busy, or invalid gesture");
            return;
        }
        running = true;
        Handler handler = new Handler(Looper.getMainLooper());
        long downTime = SystemClock.uptimeMillis();
        float cx = surface.getWidth() * 0.5f;
        float cy = surface.getHeight() * 0.4f;
        float gap = surface.getWidth() * 0.07f;
        emit(surface, downTime, MotionEvent.ACTION_DOWN, new float[][]{{cx-gap,cy}});
        handler.postDelayed(() -> emit(surface, downTime,
            MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
            new float[][]{{cx-gap,cy},{cx+gap,cy}}), 80);
        if ("escape".equals(gesture)) {
            handler.postDelayed(() -> emit(surface, downTime,
                MotionEvent.ACTION_POINTER_DOWN | (2 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                new float[][]{{cx-gap,cy},{cx+gap,cy},{cx,cy+gap}}), 160);
            handler.postDelayed(() -> emit(surface, downTime,
                MotionEvent.ACTION_POINTER_UP | (2 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                new float[][]{{cx-gap,cy},{cx+gap,cy},{cx,cy+gap}}), 250);
        } else {
            for (int step = 1; step <= 8; ++step) {
                final float fraction = step / 8.0f;
                handler.postDelayed(() -> emit(surface, downTime, MotionEvent.ACTION_MOVE,
                    points(gesture,cx,cy,gap,fraction)), 80 + step * 50L);
            }
        }
        final float[][] end = points(gesture,cx,cy,gap,1.0f);
        handler.postDelayed(() -> emit(surface, downTime,
            MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), end), 560);
        handler.postDelayed(() -> {
            emit(surface, downTime, MotionEvent.ACTION_UP, new float[][]{end[0]});
            running = false;
        }, 640);
        setResultCode(0);
        setResultData("Dispatched " + gesture + " to game touch surface");
    }

    private static float[][] points(String gesture,float cx,float cy,float gap,float f) {
        if ("pan".equals(gesture))
            return new float[][]{{cx-gap+f*gap*2,cy+f*gap},{cx+gap+f*gap*2,cy+f*gap}};
        if ("pinch".equals(gesture))
            return new float[][]{{cx-gap*(1+f),cy},{cx+gap*(1+f),cy}};
        return new float[][]{{cx-gap,cy},{cx+gap,cy}};
    }

    private static void emit(View surface,long downTime,int action,float[][] points) {
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[points.length];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[points.length];
        for (int i=0;i<points.length;++i) {
            props[i]=new MotionEvent.PointerProperties();
            props[i].id=i;
            props[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
            coords[i]=new MotionEvent.PointerCoords();
            coords[i].x=points[i][0];
            coords[i].y=points[i][1];
            coords[i].pressure=1;
            coords[i].size=0.1f;
        }
        MotionEvent event=MotionEvent.obtain(downTime,SystemClock.uptimeMillis(),action,
            points.length,props,coords,0,0,1,1,0,0,InputDevice.SOURCE_TOUCHSCREEN,0);
        surface.dispatchTouchEvent(event);
        event.recycle();
    }
}
