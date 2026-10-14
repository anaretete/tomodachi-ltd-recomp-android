package com.recomp.tomodachiltd;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.Log;

public class TomodachiActivity extends NativeActivity {
    private static final String TAG = "TomodachiActivity";
    private static TomodachiActivity sInstance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sInstance = this;
        super.onCreate(savedInstanceState);
        Log.i(TAG, "TomodachiActivity created successfully.");
    }

    @Override
    protected void onDestroy() {
        if (sInstance == this) {
            sInstance = null;
        }
        super.onDestroy();
    }

    public static void showModalDialog(final String title, final String message) {
        if (sInstance == null) {
            Log.e(TAG, "Cannot show dialog: activity instance is null.");
            return;
        }

        sInstance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    new AlertDialog.Builder(sInstance)
                        .setTitle(title)
                        .setMessage(message)
                        .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                            }
                        })
                        .setCancelable(false)
                        .show();
                } catch (Exception e) {
                    Log.e(TAG, "Error displaying alert dialog", e);
                }
            }
        });
    }
}
