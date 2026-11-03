package com.novastudio.nova;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.Manifest;
import android.net.Uri;
import android.provider.Settings;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

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

        // Soporte para pantalla completa y recorte de pantalla (notch/cámara frontal)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        
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
    /**
     * Comprueba si el usuario ha concedido el permiso de instalacion de APKs.
     */
    public static boolean canInstallApks() {
        try {
            Activity activity = getInstance();
            if (activity == null) return false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                return activity.getPackageManager().canRequestPackageInstalls();
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "canInstallApks: failed", e);
            return false;
        }
    }

    /**
     * Abre Ajustes para que el usuario conceda el permiso de instalacion.
     */
    public static void openInstallPermissionSettings() {
        try {
            Activity activity = getInstance();
            if (activity == null) return;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES);
                intent.setData(android.net.Uri.parse("package:" + activity.getPackageName()));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                activity.startActivity(intent);
            }
        } catch (Exception e) {
            Log.e(TAG, "openInstallPermissionSettings: failed", e);
        }
    }

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

            try {
                java.util.List<android.content.pm.ResolveInfo> resInfoList = activity.getPackageManager()
                    .queryIntentActivities(installIntent, PackageManager.MATCH_DEFAULT_ONLY);
                for (android.content.pm.ResolveInfo resolveInfo : resInfoList) {
                    String packageName = resolveInfo.activityInfo.packageName;
                    activity.grantUriPermission(packageName, apkUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                }
            } catch (Exception ex) {
                Log.w(TAG, "grantUriPermission warning", ex);
            }

            activity.startActivity(installIntent);
            Log.i(TAG, "installApk: launched installer for " + apkPath);
        } catch (Exception e) {
            Log.e(TAG, "installApk: failed", e);
        }
    }

    /**
     * Feedback háptico nativo de pulsación táctil (click sutil).
     */
    public void vibrateTick() {
        try {
            Vibrator vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            if (vibrator != null && vibrator.hasVibrator()) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    vibrator.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_CLICK));
                } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    vibrator.vibrate(VibrationEffect.createOneShot(20, VibrationEffect.DEFAULT_AMPLITUDE));
                } else {
                    vibrator.vibrate(20);
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "vibrateTick error", e);
        }
    }

    /**
     * Comprueba si el permiso de grabación de audio (micrófono) está concedido.
     */
    public static boolean hasRecordAudioPermission() {
        Activity act = getInstance();
        if (act == null) return false;
        return ContextCompat.checkSelfPermission(act, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Solicita el permiso de micrófono al usuario en tiempo de ejecución.
     */
    public static void requestRecordAudioPermission() {
        Activity act = getInstance();
        if (act == null) return;
        ActivityCompat.requestPermissions(act,
                new String[]{ Manifest.permission.RECORD_AUDIO },
                PERMISSION_REQUEST_CODE);
    }

    /**
     * Comprueba si el permiso de lectura de archivos de audio está concedido.
     */
    public static boolean hasStorageAudioPermission() {
        Activity act = getInstance();
        if (act == null) return true;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return ContextCompat.checkSelfPermission(act, Manifest.permission.READ_MEDIA_AUDIO)
                    == PackageManager.PERMISSION_GRANTED;
        } else {
            return ContextCompat.checkSelfPermission(act, Manifest.permission.READ_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
        }
    }

    /**
     * Solicita permiso para acceder y leer archivos de audio del almacenamiento.
     */
    public static void requestStorageAudioPermission() {
        Activity act = getInstance();
        if (act == null) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            ActivityCompat.requestPermissions(act,
                    new String[]{ Manifest.permission.READ_MEDIA_AUDIO },
                    1002);
        } else {
            ActivityCompat.requestPermissions(act,
                    new String[]{ Manifest.permission.READ_EXTERNAL_STORAGE },
                    1002);
        }
    }

    /**
     * Abre los Ajustes de la aplicación para permitir al usuario activar permisos manualmente.
     */
    public static void openAppSettings() {
        try {
            Activity act = getInstance();
            if (act == null) return;
            Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            intent.setData(Uri.parse("package:" + act.getPackageName()));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            act.startActivity(intent);
        } catch (Exception e) {
            Log.e(TAG, "openAppSettings falló", e);
        }
    }

    /**
     * Copia de forma segura un URI con esquema content:// (del selector de archivos
     * o Storage Access Framework de Android) a un archivo local en la caché de la app,
     * utilizando ContentResolver. Retorna la ruta absoluta del archivo local o null si falla.
     */
    public static String copyContentUriToCache(String contentUriStr) {
        if (contentUriStr == null || contentUriStr.isEmpty()) {
            return null;
        }
        try {
            MainActivity activity = getInstance();
            if (activity == null) {
                Log.e(TAG, "copyContentUriToCache: instancia de MainActivity es null");
                return null;
            }

            // Normalización y extracción limpia de la URI content://
            String cleanUriStr = contentUriStr.trim();
            int contentIdx = cleanUriStr.indexOf("content:");
            if (contentIdx != -1) {
                cleanUriStr = cleanUriStr.substring(contentIdx);
                if (cleanUriStr.startsWith("content:/") && !cleanUriStr.startsWith("content://")) {
                    cleanUriStr = "content://" + cleanUriStr.substring(9);
                }
            }

            Uri uri = Uri.parse(cleanUriStr);
            android.content.ContentResolver resolver = activity.getContentResolver();
            if (resolver == null) {
                Log.e(TAG, "copyContentUriToCache: ContentResolver es null");
                return null;
            }

            // Intentar tomar persistencia de lectura si el proveedor lo admite
            try {
                resolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception ignored) {
                // Proveedores que no soportan persistencia continúan normalmente
            }

            // Deducir el nombre real del archivo mediante OpenableColumns
            String displayName = null;
            try (android.database.Cursor cursor = resolver.query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int nameIndex = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (nameIndex >= 0) {
                        displayName = cursor.getString(nameIndex);
                    }
                }
            } catch (Exception ex) {
                Log.w(TAG, "No se pudo leer DISPLAY_NAME para: " + cleanUriStr, ex);
            }

            if (displayName == null || displayName.isEmpty()) {
                displayName = "audio_" + System.currentTimeMillis() + ".wav";
            }

            // Reemplazar caracteres ilegales en el nombre de archivo
            displayName = displayName.replace('/', '_').replace('\\', '_').replace(':', '_');
            String uniqueName = System.currentTimeMillis() + "_" + displayName;

            File cacheDir = new File(activity.getCacheDir(), "imported_media");
            if (!cacheDir.exists()) {
                cacheDir.mkdirs();
            }
            File outFile = new File(cacheDir, uniqueName);

            Log.i(TAG, "Copiando SAF content URI: " + cleanUriStr + " -> " + outFile.getAbsolutePath());

            try (InputStream in = resolver.openInputStream(uri);
                 OutputStream out = new FileOutputStream(outFile)) {
                if (in == null) {
                    Log.e(TAG, "openInputStream devolvió null para: " + cleanUriStr);
                    return null;
                }
                byte[] buf = new byte[65536];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                out.flush();
            }

            Log.i(TAG, "✅ Copia SAF completada exitosamente (" + outFile.length() + " bytes): " + outFile.getAbsolutePath());
            return outFile.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "copyContentUriToCache error: " + e.getMessage(), e);
            return null;
        }
    }

    /**
     * Copia un archivo desde almacenamiento compartido (/storage/emulated/0/...)
     * al almacenamiento de caché privado de la aplicación si el acceso C++ directo falla.
     */
    public static String copyStoragePathToCache(String localFilePath) {
        if (localFilePath == null || localFilePath.isEmpty()) return null;
        try {
            MainActivity activity = getInstance();
            if (activity == null) return null;

            File srcFile = new File(localFilePath);
            if (!srcFile.exists() || !srcFile.canRead()) {
                Log.w(TAG, "copyStoragePathToCache: archivo de origen no legible directamente: " + localFilePath);
                return null;
            }

            File cacheDir = new File(activity.getCacheDir(), "imported_media");
            if (!cacheDir.exists()) cacheDir.mkdirs();

            String uniqueName = System.currentTimeMillis() + "_" + srcFile.getName();
            File outFile = new File(cacheDir, uniqueName);

            try (InputStream in = new FileInputStream(srcFile);
                 OutputStream out = new FileOutputStream(outFile)) {
                byte[] buf = new byte[65536];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                out.flush();
            }

            Log.i(TAG, "✅ Copia de archivo local completada: " + outFile.getAbsolutePath());
            return outFile.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "copyStoragePathToCache error: " + e.getMessage(), e);
            return null;
        }
    }
}