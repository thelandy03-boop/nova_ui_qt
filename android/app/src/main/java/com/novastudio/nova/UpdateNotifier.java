package com.novastudio.nova;

import android.Manifest;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;

/**
 * Notificacion del sistema que avisa de una nueva version de NOVA Studio.
 *
 * Se llama desde C++ (NovaUpdateChecker) cuando la release de GitHub es mas
 * nueva que la version instalada. La notificacion lleva a la app, donde el
 * overlay de actualizacion ofrece la descarga y la instalacion integradas.
 *
 * 🔒 En Android 13+ (API 33) las notificaciones necesitan el permiso
 * POST_NOTIFICATIONS. No se pide al arrancar: se pide solo cuando de verdad
 * hay una actualizacion que avisar. Si el usuario lo concede, la notificacion
 * pendiente se publica en ese momento sin esperar a la siguiente comprobacion.
 */
public final class UpdateNotifier {
    private static final String TAG = "UpdateNotifier";

    private static final String CHANNEL_ID = "nova_updates";
    private static final int NOTIFICATION_ID = 2001;

    /** Codigo propio para no chocar con el 1001 de MainActivity. */
    public static final int PERMISSION_REQUEST_CODE = 1002;

    private static final String PREFS = "nova_update_notifications";
    private static final String KEY_PENDING_VERSION = "pending_version";
    private static final String KEY_PENDING_CURRENT = "pending_current_version";
    private static final String KEY_LAST_NOTIFIED = "last_notified_version";
    private static final String KEY_PERMISSION_ASKED_VERSION = "permission_asked_version";

    private UpdateNotifier() {
        // Clase de utilidades: no se instancia.
    }

    /**
     * Publica la notificacion de actualizacion o, si Android aun no lo
     * permite, pide el permiso POST_NOTIFICATIONS y deja la version pendiente.
     *
     * @param context        Activity/contexto de la app (puede ser null).
     * @param version        Version nueva publicada (por ejemplo "1.0.18").
     * @param currentVersion Version instalada actualmente (puede ir vacia).
     */
    public static void showOrRequest(Context context, String version, String currentVersion) {
        if (context == null || version == null || version.isEmpty()) {
            return;
        }

        if (!areNotificationsEnabled(context)) {
            // Si es un problema de permiso (Android 13+), se puede pedir.
            // Si el usuario desactivo las notificaciones en Ajustes, no hay
            // dialogo que mostrar: el overlay interno sigue avisando.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                    && ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS)
                       != PackageManager.PERMISSION_GRANTED) {
                SharedPreferences prefs =
                    context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
                String askedVersion = prefs.getString(KEY_PERMISSION_ASKED_VERSION, "");
                if (version.equals(askedVersion)) {
                    // Ya se pidio para esta version y el usuario no lo
                    // concedio: no repetir el dialogo en cada vuelta a la app.
                    Log.i(TAG, "Permiso de notificaciones ya solicitado para " + version);
                    return;
                }
                prefs.edit()
                    .putString(KEY_PERMISSION_ASKED_VERSION, version)
                    .apply();
                rememberPending(context, version, currentVersion);
                requestPermission(context);
            } else {
                Log.i(TAG, "Notificaciones desactivadas por el usuario: se omite el aviso");
            }
            return;
        }

        post(context, version, currentVersion);
    }

    /** Retira la notificacion cuando ya no hay actualizacion pendiente. */
    public static void cancel(Context context) {
        if (context == null) {
            return;
        }
        try {
            NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            if (manager != null) {
                manager.cancel(NOTIFICATION_ID);
            }
            SharedPreferences prefs =
                context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            prefs.edit()
                .remove(KEY_PENDING_VERSION)
                .remove(KEY_PENDING_CURRENT)
                .apply();
        } catch (Exception e) {
            Log.w(TAG, "No se pudo retirar la notificacion de actualizacion", e);
        }
    }

    /**
     * Procesa el resultado del dialogo de permiso. Si se concedio, publica la
     * notificacion que quedo pendiente.
     */
    public static void onPermissionResult(Context context, int requestCode,
                                          String[] permissions, int[] grantResults) {
        if (requestCode != PERMISSION_REQUEST_CODE) {
            return;
        }

        boolean granted = grantResults.length > 0
            && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        if (!granted) {
            Log.i(TAG, "El usuario no concedio el permiso de notificaciones");
            if (context != null) {
                context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .edit()
                    .remove(KEY_PENDING_VERSION)
                    .remove(KEY_PENDING_CURRENT)
                    .apply();
            }
            return;
        }

        if (context == null) {
            context = MainActivity.getInstance();
        }
        if (context == null) {
            return;
        }

        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String pendingVersion = prefs.getString(KEY_PENDING_VERSION, "");
        String pendingCurrent = prefs.getString(KEY_PENDING_CURRENT, "");
        if (!pendingVersion.isEmpty()) {
            post(context, pendingVersion, pendingCurrent);
        }
    }

    /** true si el usuario permite notificaciones de la app. */
    public static boolean areNotificationsEnabled(Context context) {
        if (context == null) {
            return false;
        }
        try {
            NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            if (manager == null || !manager.areNotificationsEnabled()) {
                return false;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                return ContextCompat.checkSelfPermission(
                           context, Manifest.permission.POST_NOTIFICATIONS)
                       == PackageManager.PERMISSION_GRANTED;
            }
            return true;
        } catch (Exception e) {
            Log.w(TAG, "No se pudo comprobar el estado de las notificaciones", e);
            return false;
        }
    }

    private static void rememberPending(Context context, String version, String currentVersion) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY_PENDING_VERSION, version)
            .putString(KEY_PENDING_CURRENT, currentVersion == null ? "" : currentVersion)
            .apply();
    }

    private static void requestPermission(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return;
        }
        Activity activity = (context instanceof Activity)
            ? (Activity) context
            : MainActivity.getInstance();
        if (activity == null) {
            Log.w(TAG, "No hay Activity para pedir el permiso de notificaciones");
            return;
        }
        ActivityCompat.requestPermissions(
            activity,
            new String[]{Manifest.permission.POST_NOTIFICATIONS},
            PERMISSION_REQUEST_CODE);
    }

    private static void post(Context context, String version, String currentVersion) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String lastNotified = prefs.getString(KEY_LAST_NOTIFIED, "");
        if (version.equals(lastNotified)) {
            // Ya se aviso de esta version: no repetir la notificacion en cada
            // comprobacion (cada arranque o cada vuelta a la app).
            return;
        }

        NotificationManager manager =
            (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) {
            return;
        }

        createChannel(context, manager);

        Intent intent = new Intent(context, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent contentIntent = PendingIntent.getActivity(
            context,
            0,
            intent,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                ? PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT
                : PendingIntent.FLAG_UPDATE_CURRENT);

        String text;
        if (currentVersion == null || currentVersion.isEmpty()) {
            text = "La versión " + version
                + " ya está disponible. Toca para actualizar NOVA Studio.";
        } else {
            text = "Ya está disponible la versión " + version
                + " (tienes la " + currentVersion
                + "). Toca para actualizar NOVA Studio.";
        }

        Notification notification = new NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("Nueva actualización de NOVA Studio")
            .setContentText(text)
            .setStyle(new NotificationCompat.BigTextStyle().bigText(text))
            .setContentIntent(contentIntent)
            .setAutoCancel(true)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setCategory(NotificationCompat.CATEGORY_RECOMMENDATION)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .build();

        try {
            manager.notify(NOTIFICATION_ID, notification);
            prefs.edit()
                .putString(KEY_LAST_NOTIFIED, version)
                .remove(KEY_PENDING_VERSION)
                .remove(KEY_PENDING_CURRENT)
                .apply();
            Log.i(TAG, "Notificacion de actualizacion publicada para la version " + version);
        } catch (Exception e) {
            Log.w(TAG, "No se pudo publicar la notificacion de actualizacion", e);
        }
    }

    private static void createChannel(Context context, NotificationManager manager) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        NotificationChannel channel = new NotificationChannel(
            CHANNEL_ID,
            "NOVA Studio Actualizaciones",
            NotificationManager.IMPORTANCE_HIGH);
        channel.setDescription("Avisos de nuevas versiones disponibles de NOVA Studio");
        channel.enableVibration(true);
        channel.setShowBadge(true);
        manager.createNotificationChannel(channel);
    }
}
