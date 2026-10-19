cmake_minimum_required(VERSION 3.22.1)
project(nova_ui_qt_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Android NDK settings
set(CMAKE_ANDROID_NDK "/home/landy/Android/Sdk/ndk/25.2.9519653")
set(CMAKE_ANDROID_API 24)
set(CMAKE_ANDROID_STL_TYPE c++_shared)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)

# Find Qt6
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Quick Qml QuickControls2)
set(CMAKE_PREFIX_PATH "/usr/local/Qt-6.6.0-android/lib/cmake/Qt6")

# Oboe for audio
find_package(oboe 1.7 REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

# Source files
set(SOURCES
    ../src/main.cpp
    ../src/NovaAudioEngine.cpp
    ../src/core/NovaAudioEngine.cpp
    ../src/core/NovaLogging.cpp
    ../src/core/NovaSessionManager.cpp
    ../src/core/NovaProjectManager.cpp
    ../src/core/NovaTransportController.cpp
    ../src/core/NovaRecordManager.cpp
    ../src/core/NovaAudioDeviceManager.cpp
    ../src/core/NovaHardwareSanitizer.cpp
    ../src/core/NovaTimeUtils.cpp
    ../src/core/NovaAudioUtils.cpp
    ../src/core/NovaPlatformUtils.cpp
    ../src/core/platform/NovaAndroidStubs.cpp
    ../src/core/platform/NovaAndroidAudioEngine.cpp
    ../src/core/platform/NovaPlatformUtils.cpp
    ../src/models/NovaTrackListModel.cpp
    ../src/models/NovaRegionModel.cpp
    ../src/views/NovaWaveformItem.cpp
)

# Qt6 MOC and UIC
qt6_add_executable(nova_ui_qt
    ${SOURCES}
    ../src/NovaAudioEngine.h
    ../src/core/NovaAudioEngine.h
    ../src/core/NovaLogging.h
    ../src/core/NovaSessionManager.h
    ../src/core/NovaProjectManager.h
    ../src/core/NovaTransportController.h
    ../src/core/NovaRecordManager.h
    ../src/core/NovaAudioDeviceManager.h
    ../src/core/NovaHardwareSanitizer.h
    ../src/core/NovaTimeUtils.h
    ../src/core/NovaAudioUtils.h
    ../src/core/NovaPlatformUtils.h
    ../src/core/platform/NovaAndroidStubs.h
    ../src/core/platform/NovaAndroidAudioEngine.h
    ../src/core/platform/NovaPlatformUtils.h
    ../src/models/NovaTrackListModel.h
    ../src/models/NovaTrackListModel.h
    ../src/models/NovaRegionModel.h
    ../src/views/NovaWaveformItem.h
)

# Qt6 modules
target_link_libraries(nova_ui_qt PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    oboe::oboe
    log
    android
    OpenSLES
    AAudio
)

# Compile definitions
target_compile_definitions(nova_ui_qt PRIVATE
    Q_OS_ANDROID
    NOVA_ANDROID_AUDIO
    QT_NO_DEBUG_OUTPUT
)

# Compile options
target_compile_options(nova_ui_qt PRIVATE
    -std=c++20
    -fexceptions
    -frtti
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Android-specific settings
set_target_properties(nova_ui_qt PROPERTIES
    ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../android
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../android
)

# Install
install(TARGETS nova_ui_qt
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
    LIBRARY DESTINATION lib
)