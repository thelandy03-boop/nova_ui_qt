package com.novastudio.nova;

import android.app.Application;
import android.content.Context;
import android.util.Log;

public class NovaApplication extends Application {
    private static final String TAG = "NovaApplication";
    
    static {
        try {
            System.loadLibrary("nova_ui_qt");
            Log.i(TAG, "Native library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library", e);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "NovaApplication created");
    }
}