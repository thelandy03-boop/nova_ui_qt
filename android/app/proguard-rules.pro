# ProGuard rules for NovaStudio Android

# Keep Qt classes
-keep class org.qtproject.qt.** { *; }
-keep class org.qtproject.qt6.** { *; }

# Keep NovaStudio classes
-keep class com.novastudio.nova.** { *; }

# Keep Oboe classes
-keep class com.google.oboe.** { *; }

# Keep JNI native methods
-keepclasseswithmembers class * {
    native <methods>;
}

# Keep Qt meta-object system
-keepclassmembers class * {
    public *** qt_metacast(**);
    public *** qt_metacall(***);
}

# Keep QML types
-keep class org.qtproject.qt.qml.** { *; }
-keep class org.qtproject.qt.jni.** { *; }

# Keep Oboe audio classes
-keep class oboe.** { *; }

# Don't obfuscate native library names
-keep class java.lang.System {
    static void loadLibrary(java.lang.String);
}

# Keep Android-specific classes
-keep class android.** { *; }
-keep class androidx.** { *; }
-keep class com.google.** { *; }

# Keep Kotlin coroutines
-keep class kotlinx.coroutines.** { *; }

# Keep Room database
-keep class androidx.room.** { *; }

# Keep DataStore
-keep class androidx.datastore.** { *; }

# Keep WorkManager
-keep class androidx.work.** { *; }

# Don't warn about Qt
-dontwarn org.qtproject.**
-dontwarn com.novastudio.nova.**

# Keep line numbers for debugging
-keepattributes SourceFile,LineNumberTable

# Keep annotations
-keepattributes *Annotation*