package com.novastudio.nova;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;

public class AudioForegroundService extends Service {
    private static final String TAG = "AudioForegroundService";
    public static final String ACTION_START = "com.novastudio.nova.ACTION_START";
    public static final String ACTION_STOP = "com.novastudio.nova.ACTION_STOP";
    
    private static final String CHANNEL_ID = "nova_audio_channel";
    private static final int NOTIFICATION_ID = 1001;
    
    private NotificationManager notificationManager;
    private AudioManager audioManager;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "AudioForegroundService created");
        
        notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            String action = intent.getAction();
            if (ACTION_START.equals(action)) {
                startForeground();
            } else if (ACTION_STOP.equals(action)) {
                stopForeground(true);
                stopSelf();
            }
        }
        return START_STICKY;
    }

    private void startForeground() {
        Notification notification = createNotification();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // El overload con tipo (startForeground(id, notificacion, tipo))
            // existe desde Android 10 (API 29).
            startForeground(NOTIFICATION_ID, notification, resolveForegroundServiceType());
        } else {
            // Android 8 y 9: todavia no hay overload con tipo.
            startForeground(NOTIFICATION_ID, notification);
        }
        
        // Request audio focus
        requestAudioFocus();
        
        Log.i(TAG, "Foreground service started");
    }

    /**
     * 🔒 El tipo del servicio en primer plano.
     *
     * "mediaPlayback" es el tipo base: no necesita ningun permiso y es el
     * unico que declara el manifest ahora mismo. El tipo "microphone" solo se
     * puede pedir si el manifest lo declara (startForeground lanza
     * IllegalArgumentException si el tipo pedido no es un subconjunto del
     * declarado) y si RECORD_AUDIO esta concedido de verdad.
     *
     * Asi el microfono aparece solo cuando haya grabador: el dia que el
     * manifest declare "mediaPlayback|microphone" y el permiso este concedido,
     * el servicio lo pide solo, sin tocar el codigo. Y mientras tanto la app
     * no anuncia capacidad de microfono, que es lo que pide la Device and
     * Network Abuse policy de Play (el audio tiene que iniciarlo el usuario).
     */
    private int resolveForegroundServiceType() {
        int type = ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                && isMicrophoneTypeDeclared()
                && ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                   == PackageManager.PERMISSION_GRANTED) {
            type |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        }
        return type;
    }

    /** Lo que el manifest declara para este servicio (no lo que se pide). */
    @RequiresApi(Build.VERSION_CODES.Q)
    private boolean isMicrophoneTypeDeclared() {
        try {
            ServiceInfo info = getPackageManager().getServiceInfo(
                    new ComponentName(this, AudioForegroundService.class), 0);
            return (info.getForegroundServiceType()
                    & ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE) != 0;
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "No se encuentra el servicio en el manifest", e);
            return false;
        }
    }

    private Notification createNotification() {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        notificationIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        
        PendingIntent pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent, 
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M 
                ? PendingIntent.FLAG_IMMUTABLE 
                : 0
        );

        return new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("NOVA Studio")
            .setContentText("Grabación/Reproducción de audio activa")
            .setSmallIcon(R.drawable.ic_notification)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .build();
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "NOVA Studio Audio",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Servicio de audio en primer plano para NOVA Studio");
            channel.setSound(null, null);
            channel.enableVibration(false);
            channel.setShowBadge(false);
            notificationManager.createNotificationChannel(channel);
        }
    }

    private void requestAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .setFlags(AudioAttributes.FLAG_HW_AV_SYNC)
                .build();
            
            AudioFocusRequest focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(audioAttributes)
                .setAcceptsDelayedFocusGain(true)
                .setOnAudioFocusChangeListener(focusChange -> {
                    Log.i(TAG, "Audio focus change: " + focusChange);
                })
                .build();
            
            int result = audioManager.requestAudioFocus(focusRequest);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                Log.w(TAG, "Audio focus not granted: " + result);
            }
        } else {
            int result = audioManager.requestAudioFocus(
                focusChange -> Log.i(TAG, "Audio focus change: " + focusChange),
                AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN
            );
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                Log.w(TAG, "Audio focus not granted: " + result);
            }
        }
    }

    private void abandonAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Would need to keep reference to focusRequest
            audioManager.abandonAudioFocusRequest(null);
        } else {
            audioManager.abandonAudioFocus(null);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        abandonAudioFocus();
        Log.i(TAG, "AudioForegroundService destroyed");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}