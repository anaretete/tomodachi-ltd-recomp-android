// ==============================================================================
// android/app/src/main/java/com/recomp/tomodachiltd/GameActivity.java
// NativeActivity Host for tomodachi-ltd-recomp Engine & Vulkan Surface
// ==============================================================================

package com.recomp.tomodachiltd;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

public class GameActivity extends NativeActivity {
    private static final String TAG = "TomodachiGameActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "GameActivity starting native recomp engine (libtomodachi_app.so)...");
    }

    @Override
    public void onBackPressed() {
        Log.i(TAG, "Back pressed in GameActivity, terminating native session and returning to Launcher.");
        super.onBackPressed();
        finish();
    }
}
