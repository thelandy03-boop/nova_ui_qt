package com.novastudio.nova;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import org.qtproject.qt.android.QtActivityBase;

public class MainActivity extends QtActivityBase {
    private static final String TAG = "MainActivity";
    private static final int PERMISSION_REQUEST_CODE = 1001;
    private static MainActivity instance = null;

    public static MainActivity getInstance() { return instance; }

    // 🔒 No se piden permisos al arrancar. Cuando haga falta uno (el microfono
    // para grabar), se pide en ese momento: pedirlo al abrir la app mete un
    // dialogo encima y, sin el permiso concedido, el servicio en primer plano
    // de tipo "microphone" revienta en Android 14+.

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;
        Log.i(TAG, "MainActivity onCreate");
        
        // Keep screen on during audio operations
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        
        // Hide system bars for immersive experience
        hideSystemBars();

        // 🔒 NO se pide NINGÚN permiso al arrancar.
        //
        // 1) MANAGE_EXTERNAL_STORAGE ("acceso a todos los archivos") abria la
        //    pantalla de Ajustes del SISTEMA al arrancar: la app salia de
        //    golpe y el usuario se encontraba en los Ajustes sin saber por
        //    qué. Y no hace falta: el spike lee de la carpeta propia de la
        //    app (/sdcard/Android/data/com.novastudio.nova/files/), que no
        //    necesita ese permiso.
        //
        // 2) RECORD_AUDIO (microfono) abria un dialogo encima de la app y,
        //    ademas, hasta que el usuario lo concede no se puede arrancar un
        //    servicio en primer plano de tipo "microphone": en Android 14+
        //    eso es un SecurityException y mata el proceso entero. El micro
        //    se pedira cuando haya grabacion, no al abrir la app.
        //
        // El servicio en primer plano se arranca igualmente: sin microfono
        // usa solo el tipo "mediaPlayback", que no necesita permiso.
        startAudioForegroundService();
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume");
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy");
        stopAudioForegroundService();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            boolean allGranted = true;
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }
            if (!allGranted) {
                Log.w(TAG, "Some permissions denied");
            }
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        Log.i(TAG, "onNewIntent: " + intent.getAction());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }

    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            );
        }
    }

    private void startAudioForegroundService() {
        // 🔒 Con try/catch: si el sistema rechaza el servicio (permisos, tipo
        // no permitido, politica de bateria), la app NO debe morir por ello.
        // El spike sigue funcionando sin el servicio en primer plano.
        try {
            Intent serviceIntent = new Intent(this, AudioForegroundService.class);
            serviceIntent.setAction(AudioForegroundService.ACTION_START);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                startForegroundService(serviceIntent);
            } else {
                startService(serviceIntent);
            }
        } catch (Exception e) {
            Log.w(TAG, "No se pudo arrancar el servicio en primer plano", e);
        }
    }

    private void stopAudioForegroundService() {
        Intent serviceIntent = new Intent(this, AudioForegroundService.class);
        serviceIntent.setAction(AudioForegroundService.ACTION_STOP);
        stopService(serviceIntent);
    }

    /**
     * Lanza el instalador de Android para un APK descargado.
     * Llamado desde C++ (NovaUpdateChecker) via JNI.
     */
    public static void installApk(String apkPath) {
        try {
            Activity activity = getInstance();
            if (activity == null) {
                Log.e(TAG, "installApk: no activity available");
                return;
            }
            java.io.File apkFile = new java.io.File(apkPath);
            if (!apkFile.exists()) {
                Log.e(TAG, "installApk: file not found: " + apkPath);
                return;
            }
            android.net.Uri apkUri;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                apkUri = androidx.core.content.FileProvider.getUriForFile(
                    activity,
                    activity.getPackageName() + ".fileprovider",
                    apkFile
                );
            } else {
                apkUri = android.net.Uri.fromFile(apkFile);
            }
            Intent installIntent = new Intent(Intent.ACTION_VIEW);
            installIntent.setDataAndType(apkUri, "application/vnd.android.package-archive");
            installIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            installIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(installIntent);
            Log.i(TAG, "installApk: launched installer for " + apkPath);
        } catch (Exception e) {
            Log.e(TAG, "installApk: failed", e);
        }
    }
}