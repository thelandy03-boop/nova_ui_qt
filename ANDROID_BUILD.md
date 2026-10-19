# NOVA Studio Android Build Guide

## Prerequisites

### 1. Install Android SDK/NDK
```bash
# Install Android Studio, then:
sdkmanager "platforms;android-34" "build-tools;34.0.0" "ndk;25.2.9519653" "cmake;3.22.1"
```

### 2. Install Qt6 for Android
```bash
# Download Qt 6.6+ for Android from qt.io
# Install to /usr/local/Qt-6.6.0-android
```

### 3. Install Oboe
```bash
# Using FetchContent in CMake (automatic) or manually:
git clone https://github.com/google/oboe.git
cd oboe && git checkout 1.7.0
```

## Build Commands

### Option 1: Using Gradle (Recommended for Android)
```bash
cd ~/Descargas/NOVA-STUDIO/nova_ui_qt/android
./gradlew assembleDebug
# Output: app/build/outputs/apk/debug/app-debug.apk
```

### Option 2: Using CMake directly (for testing)
```bash
cd ~/Descargas/NOVA-STUDIO/nova_ui_qt/android/app
mkdir -p build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-24 \
      -DCMAKE_BUILD_TYPE=Debug \
      -DQt6_DIR=/usr/local/Qt-6.6.0-android/lib/cmake/Qt6 \
      ..
cmake --build . -j$(nproc)
```

### Option 3: Using Qt Creator (Best for development)
1. Open `android/app/src/main/cpp/CMakeLists.txt` in Qt Creator
2. Configure kit: **Qt 6.6.0 Android arm64-v8a**
3. Build & Run on device

## Project Structure
```
android/
├── app/
│   ├── build.gradle              # App module
│   ├── proguard-rules.pro        # ProGuard rules
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/novastudio/nova/
│       │   ├── NovaApplication.java
│       │   ├── MainActivity.java
│       │   └── AudioForegroundService.java
│       ├── res/
│       │   ├── values/strings.xml, colors.xml, themes.xml
│       │   ├── xml/audio_policy.xml, usb_device_filter.xml
│       │   ├── mipmap-*/ic_launcher.xml
│       │   └── drawable/ic_notification.xml
│       └── cpp/CMakeLists.txt    # CMake for NDK build
├── build.gradle                   # Project level
├── settings.gradle
├── gradle.properties
└── gradle/wrapper/
```

## Building APK

### Debug Build
```bash
cd android
./gradlew assembleDebug
# Output: app/build/outputs/apk/debug/app-debug.apk
```

### Release Build (for Play Store)
```bash
# Configure signing in build.gradle first
./gradlew assembleRelease
# Output: app/build/outputs/apk/release/app-release.aab
```

## Running on Device

### Install Debug APK
```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.novastudio.nova/.MainActivity
```

### View Logs
```bash
adb logcat -s NovaApplication:D MainActivity:D AudioForegroundService:D NovaAndroidAudio:D
```

## Required Device Permissions
- **Audio Recording**: `RECORD_AUDIO` (mandatory)
- **Storage**: `READ_EXTERNAL_STORAGE`, `WRITE_EXTERNAL_STORAGE`, `MANAGE_EXTERNAL_STORAGE`
- **Foreground Service**: `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MICROPHONE`
- **Wake Lock**: `WAKE_LOCK` (prevents sleep during recording)
- **MIDI USB**: `USB_PERMISSION` + device filter XML

## Troubleshooting

### Oboe not found
```bash
# Ensure FetchContent downloaded it
cd android/app/build/oboe
ls -la
```

### Qt6 not found
```bash
export Qt6_DIR=/usr/local/Qt-6.6.0-android/lib/cmake/Qt6
```

### NDK path issues
```bash
export ANDROID_NDK_HOME=/home/landy/Android/Sdk/ndk/25.2.9519653
export ANDROID_SDK_ROOT=/home/landy/Android/Sdk
```

### Audio not working
- Check `adb logcat -s NovaAndroidAudio:D`
- Verify `RECORD_AUDIO` permission granted in Settings
- Check `AudioForegroundService` is running

## Architecture Notes

### Audio Engine
- **Oboe** → Low-latency audio I/O (AAudio/OpenSL ES fallback)
- **RingBuffer** → Lock-free SPSC per track (4 bursts capacity)
- **DiskWriterThread** → Background thread writes to disk
- **NovaAndroidAudioEngine** → Qt wrapper exposing C++ API to QML

### QML Integration
- `AudioEngine` singleton exposed to QML
- `NovaAndroidAudioEngine` used on Android instead of Ardour backend
- Same API surface as desktop (`NovaAudioEngine`)

### Foreground Service
- `AudioForegroundService` keeps audio alive in background
- Notification shows recording/playback status
- Audio focus management for interruptions

## Next Development Steps

1. **Implement disk writing** in `NovaAndroidAudioEngine::writeTrackToDisk()`
2. **Multi-track mixing** in `OutputStreamCallback`
3. **Latency compensation** using `getFramesPerBurst()` + `getBufferSizeInFrames()`
4. **MIDI support** via `AMidi` API
5. **Settings persistence** with DataStore/Room