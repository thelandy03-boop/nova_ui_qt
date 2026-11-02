package com.novastudio.nova;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
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
    public static final String ACTION_PLAY = "com.novastudio.nova.ACTION_PLAY";
    public static final String ACTION_PAUSE = "com.novastudio.nova.ACTION_PAUSE";
    public static final String ACTION_TOGGLE_PLAY = "com.novastudio.nova.ACTION_TOGGLE_PLAY";
    
    private static final String CHANNEL_ID = "nova_audio_channel";
    private static final int NOTIFICATION_ID = 1001;

    private static AudioForegroundService instance = null;
    public static AudioForegroundService getInstance() { return instance; }

    public static native void nativePauseAudio();
    public static native void nativePlayAudio();
    public static native void nativeStopAudio();
    public static native boolean nativeIsPlaying();

    private NotificationManager notificationManager;
    private AudioManager audioManager;
    private AudioFocusRequest focusRequest;
    private boolean isPlayingState = false;
    private boolean isNoisyReceiverRegistered = false;

    private final BroadcastReceiver noisyReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction())) {
                Log.i(TAG, "Auriculares desconectados: pausando reproducción");
                try {
                    nativePauseAudio();
                } catch (Throwable t) {
                    Log.w(TAG, "nativePauseAudio no disponible", t);
                }
                updateNotification(false);
            }
        }
    };

    public static void updateServicePlaybackState(boolean isPlaying) {
        if (instance != null) {
            instance.updateNotification(isPlaying);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        instance = this;
        Log.i(TAG, "AudioForegroundService created");
        
        notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        
        createNotificationChannel();

        try {
            registerReceiver(noisyReceiver, new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
            isNoisyReceiverRegistered = true;
        } catch (Exception e) {
            Log.w(TAG, "No se pudo registrar noisyReceiver", e);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            String action = intent.getAction();
            if (ACTION_START.equals(action)) {
                startForeground();
            } else if (ACTION_PLAY.equals(action)) {
                try { nativePlayAudio(); } catch (Throwable ignored) {}
                updateNotification(true);
            } else if (ACTION_PAUSE.equals(action)) {
                try { nativePauseAudio(); } catch (Throwable ignored) {}
                updateNotification(false);
            } else if (ACTION_TOGGLE_PLAY.equals(action)) {
                boolean playing = false;
                try { playing = nativeIsPlaying(); } catch (Throwable ignored) {}
                if (playing) {
                    try { nativePauseAudio(); } catch (Throwable ignored) {}
                    updateNotification(false);
                } else {
                    try { nativePlayAudio(); } catch (Throwable ignored) {}
                    updateNotification(true);
                }
            } else if (ACTION_STOP.equals(action)) {
                try { nativeStopAudio(); } catch (Throwable ignored) {}
                stopForeground(true);
                stopSelf();
            }
        }
        return START_STICKY;
    }

    private void startForeground() {
        Notification notification = createNotification(false);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification, resolveForegroundServiceType());
        } else {
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

    public void updateNotification(boolean isPlaying) {
        isPlayingState = isPlaying;
        if (notificationManager != null) {
            notificationManager.notify(NOTIFICATION_ID, createNotification(isPlaying));
        }
    }

    private Notification createNotification(boolean isPlaying) {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        notificationIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        
        PendingIntent pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent, 
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M 
                ? PendingIntent.FLAG_IMMUTABLE 
                : 0
        );

        Intent toggleIntent = new Intent(this, AudioForegroundService.class);
        toggleIntent.setAction(ACTION_TOGGLE_PLAY);
        PendingIntent togglePendingIntent = PendingIntent.getService(
            this, 1, toggleIntent,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                ? PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT
                : PendingIntent.FLAG_UPDATE_CURRENT
        );

        Intent stopIntent = new Intent(this, AudioForegroundService.class);
        stopIntent.setAction(ACTION_STOP);
        PendingIntent stopPendingIntent = PendingIntent.getService(
            this, 2, stopIntent,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                ? PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT
                : PendingIntent.FLAG_UPDATE_CURRENT
        );

        return new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("NOVA Studio")
            .setContentText(isPlaying ? "Reproduciendo audio..." : "Reproducción en pausa")
            .setSmallIcon(R.drawable.ic_notification)
            .setContentIntent(pendingIntent)
            .setOngoing(isPlaying)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .addAction(
                isPlaying ? R.drawable.ic_pause : R.drawable.ic_play,
                isPlaying ? "Pausar" : "Reproducir",
                togglePendingIntent
            )
            .addAction(
                R.drawable.ic_stop,
                "Detener",
                stopPendingIntent
            )
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

    private void handleAudioFocusChange(int focusChange) {
        Log.i(TAG, "Audio focus change: " + focusChange);
        switch (focusChange) {
            case AudioManager.AUDIOFOCUS_LOSS:
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                try {
                    nativePauseAudio();
                } catch (Throwable t) {
                    Log.w(TAG, "nativePauseAudio no disponible", t);
                }
                updateNotification(false);
                break;
            case AudioManager.AUDIOFOCUS_GAIN:
                break;
        }
    }

    private void requestAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .setFlags(AudioAttributes.FLAG_HW_AV_SYNC)
                .build();
            
            focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(audioAttributes)
                .setAcceptsDelayedFocusGain(true)
                .setOnAudioFocusChangeListener(this::handleAudioFocusChange)
                .build();
            
            int result = audioManager.requestAudioFocus(focusRequest);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                Log.w(TAG, "Audio focus not granted: " + result);
            }
        } else {
            int result = audioManager.requestAudioFocus(
                this::handleAudioFocusChange,
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
            if (focusRequest != null) {
                audioManager.abandonAudioFocusRequest(focusRequest);
            }
        } else {
            audioManager.abandonAudioFocus(this::handleAudioFocusChange);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (isNoisyReceiverRegistered) {
            try {
                unregisterReceiver(noisyReceiver);
                isNoisyReceiverRegistered = false;
            } catch (Exception ignored) {}
        }
        abandonAudioFocus();
        instance = null;
        Log.i(TAG, "AudioForegroundService destroyed");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}