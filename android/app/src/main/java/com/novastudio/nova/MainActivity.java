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
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.net.Uri;
import android.provider.Settings;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import android.content.ContentUris;
import android.database.Cursor;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.provider.MediaStore;
import java.nio.ByteBuffer;

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
        if (instance == this) {
            instance = null;
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        // Si el usuario acaba de conceder POST_NOTIFICATIONS, publicar el
        // aviso de actualizacion que quedo pendiente.
        UpdateNotifier.onPermissionResult(this, requestCode, permissions, grantResults);
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

    /**
     * Publica la notificacion del sistema que avisa de una nueva version.
     * Llamado desde C++ (NovaUpdateChecker) via JNI cuando la release de
     * GitHub es mas nueva que la version instalada.
     */
    public static void showUpdateNotification(String version, String currentVersion) {
        UpdateNotifier.showOrRequest(getInstance(), version, currentVersion);
    }

    /**
     * Retira la notificacion de actualizacion cuando ya no queda nada que
     * instalar (por ejemplo, tras actualizar la app).
     */
    public static void cancelUpdateNotification() {
        UpdateNotifier.cancel(getInstance());
    }

    public static void installApk(String apkPath) {
        try {
            Activity activity = getInstance();
            if (activity == null) {
                Log.e(TAG, "installApk: no activity available");
                return;
            }
            File sourceFile = new File(apkPath);
            if (!sourceFile.exists()) {
                Log.e(TAG, "installApk: file not found: " + apkPath);
                return;
            }

            // Preparar archivo en externalCacheDir si está disponible para máxima compatibilidad con instaladores del sistema
            File apkFile = sourceFile;
            try {
                File extCache = activity.getExternalCacheDir();
                if (extCache != null) {
                    File extApk = new File(extCache, "nova_update.apk");
                    if (!extApk.getAbsolutePath().equals(sourceFile.getAbsolutePath())) {
                        copyFile(sourceFile, extApk);
                        apkFile = extApk;
                    }
                }
            } catch (Exception copyEx) {
                Log.w(TAG, "No se pudo copiar APK a external cache, usando origen", copyEx);
                apkFile = sourceFile;
            }

            apkFile.setReadable(true, false);

            Uri apkUri;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                apkUri = androidx.core.content.FileProvider.getUriForFile(
                    activity,
                    activity.getPackageName() + ".fileprovider",
                    apkFile
                );
            } else {
                apkUri = Uri.fromFile(apkFile);
            }

            Intent installIntent = new Intent(Intent.ACTION_VIEW);
            installIntent.setDataAndType(apkUri, "application/vnd.android.package-archive");
            installIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            installIntent.addFlags(Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
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

            // Conceder también a instaladores conocidos por si PackageManager filtra la lista por visibilidad de paquetes
            String[] knownInstallers = new String[] {
                "com.google.android.packageinstaller",
                "com.android.packageinstaller",
                "com.samsung.android.packageinstaller",
                "com.miui.packageinstaller"
            };
            for (String installerPkg : knownInstallers) {
                try {
                    activity.grantUriPermission(installerPkg, apkUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                } catch (Exception ignored) {}
            }

            activity.startActivity(installIntent);
            Log.i(TAG, "installApk: launched installer for " + apkFile.getAbsolutePath());
        } catch (Exception e) {
            Log.e(TAG, "installApk: failed", e);
        }
    }

    private static void copyFile(File src, File dst) throws java.io.IOException {
        try (InputStream in = new FileInputStream(src);
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[65536];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            out.flush();
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
     * utilizando ContentResolver. Si el formato es AAC/M4A/OPUS u otro no decodificado
     * directamente por el C++ nativo, lo transcodifica a WAV PCM estándar con MediaCodec.
     * Retorna la ruta absoluta del archivo local o null si falla.
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
            try (Cursor cursor = resolver.query(uri, null, null, null, null)) {
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

            File cacheDir = new File(activity.getCacheDir(), "imported_media");
            if (!cacheDir.exists()) {
                cacheDir.mkdirs();
            }

            String lowerName = displayName.toLowerCase();
            boolean isDirectAudio = lowerName.endsWith(".wav") || lowerName.endsWith(".mp3")
                    || lowerName.endsWith(".flac") || lowerName.endsWith(".ogg");

            // Si es un formato no soportado por C++ nativo (.m4a, .aac, .opus, notas de voz, etc.),
            // decodificarlo directamente a WAV PCM 16-bit con MediaCodec de Android.
            if (!isDirectAudio) {
                String baseName = displayName;
                int dotIdx = baseName.lastIndexOf('.');
                if (dotIdx > 0) baseName = baseName.substring(0, dotIdx);
                String wavName = System.currentTimeMillis() + "_" + baseName + ".wav";
                File outWavFile = new File(cacheDir, wavName);
                if (decodeAudioToWav(activity, uri, outWavFile)) {
                    Log.i(TAG, "✅ Audio SAF transcodificado a WAV PCM vía MediaCodec: " + outWavFile.getAbsolutePath());
                    return outWavFile.getAbsolutePath();
                }
                Log.w(TAG, "MediaCodec no pudo decodificar el formato; continuando con copia directa como respaldo");
            }

            String uniqueName = System.currentTimeMillis() + "_" + displayName;
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
     * Soporta Scoped Storage en Android 11+ resolviendo vía MediaStore si File.canRead() da false.
     */
    public static String copyStoragePathToCache(String localFilePath) {
        if (localFilePath == null || localFilePath.isEmpty()) return null;
        try {
            MainActivity activity = getInstance();
            if (activity == null) return null;

            File srcFile = new File(localFilePath);
            if (srcFile.exists() && srcFile.canRead()) {
                File cacheDir = new File(activity.getCacheDir(), "imported_media");
                if (!cacheDir.exists()) cacheDir.mkdirs();

                String lowerName = srcFile.getName().toLowerCase();
                boolean isDirectAudio = lowerName.endsWith(".wav") || lowerName.endsWith(".mp3")
                        || lowerName.endsWith(".flac") || lowerName.endsWith(".ogg");

                if (!isDirectAudio) {
                    String baseName = srcFile.getName();
                    int dotIdx = baseName.lastIndexOf('.');
                    if (dotIdx > 0) baseName = baseName.substring(0, dotIdx);
                    String wavName = System.currentTimeMillis() + "_" + baseName + ".wav";
                    File outWavFile = new File(cacheDir, wavName);
                    Uri fileUri = Uri.fromFile(srcFile);
                    if (decodeAudioToWav(activity, fileUri, outWavFile)) {
                        Log.i(TAG, "✅ Audio local transcodificado a WAV PCM: " + outWavFile.getAbsolutePath());
                        return outWavFile.getAbsolutePath();
                    }
                }

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
            }

            // Si srcFile.canRead() da false debido a Scoped Storage (Android 11+),
            // intentar resolver la URI en MediaStore mediante ContentResolver
            Uri mediaUri = findMediaUriForPath(activity, localFilePath);
            if (mediaUri != null) {
                Log.i(TAG, "Resolviendo ruta Scoped Storage via MediaStore: " + localFilePath + " -> " + mediaUri);
                return copyContentUriToCache(mediaUri.toString());
            }

            Log.w(TAG, "copyStoragePathToCache: no se pudo leer directamente ni por MediaStore: " + localFilePath);
            return null;
        } catch (Exception e) {
            Log.e(TAG, "copyStoragePathToCache error: " + e.getMessage(), e);
            return null;
        }
    }

    /**
     * Resuelve un URI de MediaStore correspondiente a una ruta absoluta de archivo (/storage/emulated/0/...).
     */
    private static Uri findMediaUriForPath(Context context, String path) {
        if (context == null || path == null) return null;
        try {
            android.content.ContentResolver cr = context.getContentResolver();
            Uri[] baseUris = new Uri[]{
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                MediaStore.Files.getContentUri("external")
            };
            for (Uri baseUri : baseUris) {
                String[] projection = new String[]{ MediaStore.MediaColumns._ID };
                String selection = MediaStore.MediaColumns.DATA + " = ?";
                String[] selectionArgs = new String[]{ path };
                try (Cursor cursor = cr.query(baseUri, projection, selection, selectionArgs, null)) {
                    if (cursor != null && cursor.moveToFirst()) {
                        long id = cursor.getLong(cursor.getColumnIndexOrThrow(MediaStore.MediaColumns._ID));
                        return ContentUris.withAppendedId(baseUri, id);
                    }
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "findMediaUriForPath error para " + path, e);
        }
        return null;
    }

    /**
     * Decodifica cualquier flujo de audio soportado por el framework de Android
     * (AAC, M4A, OPUS, 3GP, AMR, WMA, etc.) a un archivo estándar WAV PCM 16-bit.
     */
    private static boolean decodeAudioToWav(Context context, Uri uri, File outFile) {
        MediaExtractor extractor = new MediaExtractor();
        MediaCodec codec = null;
        File tempPcm = null;
        try {
            extractor.setDataSource(context, uri, null);
            int trackIndex = -1;
            MediaFormat format = null;
            String mime = null;
            for (int i = 0; i < extractor.getTrackCount(); i++) {
                MediaFormat f = extractor.getTrackFormat(i);
                String m = f.getString(MediaFormat.KEY_MIME);
                if (m != null && m.startsWith("audio/")) {
                    trackIndex = i;
                    format = f;
                    mime = m;
                    break;
                }
            }
            if (trackIndex < 0 || format == null || mime == null) {
                return false;
            }
            extractor.selectTrack(trackIndex);
            codec = MediaCodec.createDecoderByType(mime);
            codec.configure(format, null, null, 0);
            codec.start();

            int sampleRate = format.containsKey(MediaFormat.KEY_SAMPLE_RATE)
                    ? format.getInteger(MediaFormat.KEY_SAMPLE_RATE) : 44100;
            int channelCount = format.containsKey(MediaFormat.KEY_CHANNEL_COUNT)
                    ? format.getInteger(MediaFormat.KEY_CHANNEL_COUNT) : 2;

            tempPcm = new File(outFile.getAbsolutePath() + ".raw");
            long totalPcmBytes = 0;
            try (FileOutputStream pcmOut = new FileOutputStream(tempPcm)) {
                MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                boolean isEOS = false;
                final long kTimeoutUs = 5000;

                while (!Thread.interrupted()) {
                    if (!isEOS) {
                        int inIndex = codec.dequeueInputBuffer(kTimeoutUs);
                        if (inIndex >= 0) {
                            ByteBuffer inBuf = codec.getInputBuffer(inIndex);
                            int sampleSize = extractor.readSampleData(inBuf, 0);
                            if (sampleSize < 0) {
                                codec.queueInputBuffer(inIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                                isEOS = true;
                            } else {
                                codec.queueInputBuffer(inIndex, 0, sampleSize, extractor.getSampleTime(), 0);
                                extractor.advance();
                            }
                        }
                    }

                    int outIndex = codec.dequeueOutputBuffer(info, kTimeoutUs);
                    if (outIndex >= 0) {
                        ByteBuffer outBuf = codec.getOutputBuffer(outIndex);
                        byte[] chunk = new byte[info.size];
                        outBuf.position(info.offset);
                        outBuf.limit(info.offset + info.size);
                        outBuf.get(chunk);
                        pcmOut.write(chunk);
                        totalPcmBytes += info.size;
                        codec.releaseOutputBuffer(outIndex, false);

                        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                            break;
                        }
                    } else if (outIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                        MediaFormat newFormat = codec.getOutputFormat();
                        if (newFormat.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
                            sampleRate = newFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                        }
                        if (newFormat.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) {
                            channelCount = newFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                        }
                    } else if (outIndex == MediaCodec.INFO_TRY_AGAIN_LATER && isEOS) {
                        break;
                    }
                }
            }

            writeWavFile(tempPcm, outFile, sampleRate, channelCount, totalPcmBytes);
            tempPcm.delete();
            return outFile.exists() && outFile.length() > 44;
        } catch (Exception e) {
            Log.e(TAG, "decodeAudioToWav error: " + e.getMessage(), e);
            if (tempPcm != null && tempPcm.exists()) tempPcm.delete();
            if (outFile.exists()) outFile.delete();
            return false;
        } finally {
            if (codec != null) {
                try { codec.stop(); codec.release(); } catch (Exception ignored) {}
            }
            try { extractor.release(); } catch (Exception ignored) {}
        }
    }

    /**
     * Escribe un encabezado estándar RIFF WAVE de 44 bytes para datos PCM 16-bit.
     */
    private static void writeWavFile(File pcmFile, File wavFile, int sampleRate, int channels, long pcmDataLength) throws java.io.IOException {
        long totalDataLen = pcmDataLength + 36;
        long byteRate = (long) sampleRate * channels * 2; // 16-bit PCM = 2 bytes por muestra
        int blockAlign = channels * 2;

        try (FileOutputStream out = new FileOutputStream(wavFile);
             FileInputStream in = new FileInputStream(pcmFile)) {
            byte[] header = new byte[44];
            header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
            header[4] = (byte) (totalDataLen & 0xff);
            header[5] = (byte) ((totalDataLen >> 8) & 0xff);
            header[6] = (byte) ((totalDataLen >> 16) & 0xff);
            header[7] = (byte) ((totalDataLen >> 24) & 0xff);
            header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
            header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
            header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; // Subchunk1Size (16 para PCM)
            header[20] = 1; header[21] = 0; // AudioFormat (1 = PCM)
            header[22] = (byte) channels; header[23] = 0;
            header[24] = (byte) (sampleRate & 0xff);
            header[25] = (byte) ((sampleRate >> 8) & 0xff);
            header[26] = (byte) ((sampleRate >> 16) & 0xff);
            header[27] = (byte) ((sampleRate >> 24) & 0xff);
            header[28] = (byte) (byteRate & 0xff);
            header[29] = (byte) ((byteRate >> 8) & 0xff);
            header[30] = (byte) ((byteRate >> 16) & 0xff);
            header[31] = (byte) ((byteRate >> 24) & 0xff);
            header[32] = (byte) blockAlign; header[33] = 0;
            header[34] = 16; header[35] = 0; // BitsPerSample = 16
            header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
            header[40] = (byte) (pcmDataLength & 0xff);
            header[41] = (byte) ((pcmDataLength >> 8) & 0xff);
            header[42] = (byte) ((pcmDataLength >> 16) & 0xff);
            header[43] = (byte) ((pcmDataLength >> 24) & 0xff);

            out.write(header, 0, 44);

            byte[] buf = new byte[65536];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            out.flush();
        }
    }
}