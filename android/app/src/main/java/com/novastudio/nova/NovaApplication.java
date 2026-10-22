package com.novastudio.nova;

import android.app.Application;
import android.util.Log;

public class NovaApplication extends Application {
    private static final String TAG = "NovaApplication";

    // 🔒 La libreria nativa principal NO se carga aqui. La carga QtLoader,
    // con el ABI del dispositivo: System.loadLibrary("nova_ui_qt") pedia
    // libnova_ui_qt.so, que no existe (la del APK es
    // libnova_ui_qt_<abi>.so), asi que ese bloque solo podia fallar con un
    // UnsatisfiedLinkError que se comia. El nombre que Qt usa sale del
    // meta-data "android.app.lib_name" del manifest.

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "NovaApplication created");
    }
}
