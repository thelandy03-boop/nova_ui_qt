cmake_minimum_required(VERSION 3.20)
project(nova_ui_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android-specific settings
if(ANDROID)
    set(CMAKE_ANDROID_NDK_VERSION "26.1.10909125" CACHE STRING "NDK version")
    set(CMAKE_ANDROID_API 24)  # Android 7.0 minimum for Oboe
    set(CMAKE_ANDROID_STL_TYPE c++_shared)

    # Oboe - use FetchContent
    include(FetchContent)
    FetchContent_Declare(
        oboe
        GIT_REPOSITORY https://github.com/google/oboe.git
        GIT_TAG        1.7.0
    )
    FetchContent_MakeAvailable(oboe)

    # Audio libraries
    find_library(AAUDIO aaudio)
    find_library(OPENSLES OpenSLES)
    find_library(LOGCAT log)
    find_library(ANDROID_SYS_LIB android)
endif()

# Rutas del motor Ardour. Por defecto, el repositorio padre del checkout
# actual; se pueden sobreescribir con -DARDOUR_BUILD_DIR=... y
# -DARDOUR_SRC_DIR=... para compilar la app fuera de ese arbol (worktrees,
# builds alternos). Sin esos flags el comportamiento es el de siempre.
if(NOT ARDOUR_BUILD_DIR)
    get_filename_component(ARDOUR_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build" ABSOLUTE)
endif()
if(NOT ARDOUR_SRC_DIR)
    get_filename_component(ARDOUR_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# Set Qt6 prefix path to help find_package
set(CMAKE_PREFIX_PATH "/usr/lib/qt6")

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Qml Quick QuickControls2)

# Force Qt6 moc path
set(CMAKE_AUTOMOC_EXECUTABLE "/usr/lib/qt6/libexec/moc" CACHE FILEPATH "Qt6 moc executable")
set(CMAKE_AUTOMOC_MOC_OPTIONS "-DQT_VERSION=0x060800")

# Solo Linux Desktop enlaza las librerías compartidas de Ardour Core
if(NOT ANDROID AND NOT WIN32)
    set(CMAKE_INSTALL_RPATH
        "${ARDOUR_BUILD_DIR}/libs/ardour"
        "${ARDOUR_BUILD_DIR}/libs/pbd"
        "${ARDOUR_BUILD_DIR}/libs/temporal"
        "${ARDOUR_BUILD_DIR}/libs/evoral"
        "${ARDOUR_BUILD_DIR}/libs/midi++2"
        "${ARDOUR_BUILD_DIR}/libs/fst"
        "${ARDOUR_BUILD_DIR}/libs/tk/suil"
        "${ARDOUR_BUILD_DIR}/libs/audiographer"
        "${ARDOUR_BUILD_DIR}/libs/backends/alsa"
        "${ARDOUR_BUILD_DIR}/libs/backends/dummy"
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SIGCPP REQUIRED IMPORTED_TARGET sigc++-2.0)
    pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.4)
    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
    pkg_check_modules(GIOMM REQUIRED IMPORTED_TARGET giomm-2.4)
    pkg_check_modules(LIBXML REQUIRED IMPORTED_TARGET libxml-2.0)
    pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
endif()

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

include(cmake/NovaQml.cmake)

# En Windows, el flag WIN32 evita que se abra la ventana negra de consola CMD
# En Android, no usamos WIN32
if(WIN32 AND NOT ANDROID)
    set(WINDOWS_FLAG WIN32)
else()
    set(WINDOWS_FLAG)
endif()

qt_add_executable(nova_ui_qt ${WINDOWS_FLAG}
    main.cpp
    src/NovaAudioEngine.h
    src/NovaAudioEngine.cpp
    src/core/NovaLogging.h
    src/core/NovaLogging.cpp
    src/core/NovaSessionManager.h
    src/core/NovaSessionManager.cpp
    src/core/NovaProjectManager.h
    src/core/NovaProjectManager.cpp
    src/core/NovaProjectPolicy.h
    src/core/NovaProjectPolicy.cpp
    src/core/NovaTransportController.h
    src/core/NovaTransportController.cpp
    src/core/NovaRecordManager.h
    src/core/NovaRecordManager.cpp
    src/core/NovaAudioDeviceManager.h
    src/core/NovaAudioDeviceManager.cpp
    src/core/NovaHardwareSanitizer.h
    src/core/NovaHardwareSanitizer.cpp
    src/core/NovaTimeUtils.h
    src/core/NovaTimeUtils.cpp
    src/core/NovaAudioUtils.h
    src/core/platform/NovaPlatformUtils.h
    src/core/platform/NovaPlatformUtils.cpp
    src/core/platform/NovaAndroidStubs.h
    src/core/platform/NovaAndroidAudioEngine.h
    src/core/platform/NovaAndroidAudioEngine.cpp
    src/models/NovaTrackListModel.h
    src/models/NovaTrackListModel.cpp
    src/models/NovaRegionModel.h
    src/models/NovaRegionModel.cpp
    src/views/NovaWaveformItem.h
    src/views/NovaWaveformItem.cpp
    src/views/NovaWaveformRenderer.cpp
    src/views/NovaWaveformRenderer.h
    src/core/NovaSimdPeaks.cpp
    src/audio/NovaDecodedAudio.h
    src/audio/NovaAudioDecoder.h
    src/audio/NovaAudioDecoder.cpp
    src/audio/NovaAudioPeaks.h
    src/audio/NovaAudioPeaks.cpp
    src/audio/NovaWavReader.h
    src/audio/NovaWavReader.cpp
    src/audio/NovaMp3Decoder.h
    src/audio/NovaMp3Decoder.cpp
    src/audio/NovaAudioPlayer.h
    src/audio/NovaAudioPlayer.cpp
    src/audio/NovaResampler.h
    src/audio/NovaResampler.cpp
    third_party/minimp3/minimp3.h
)

# Common Qt libraries for all platforms
target_link_libraries(nova_ui_qt PRIVATE Qt6::OpenGL Qt6::Quick Qt6::Qml Qt6::Core Qt6::QuickControls2 Qt6::Gui)

if(ANDROID)
    set_property(TARGET nova_ui_qt PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")

    # El controlador del spike (motor de Oboe + forma de onda) solo se
    # compila en Android: en el escritorio la app es la de siempre.
    target_sources(nova_ui_qt PRIVATE
        src/core/platform/NovaSpikeController.h
        src/core/platform/NovaSpikeController.cpp
    )

    target_link_libraries(nova_ui_qt PRIVATE oboe ${AAUDIO} ${OPENSLES} ${LOGCAT} ${ANDROID_SYS_LIB})

    target_compile_definitions(nova_ui_qt PRIVATE Q_OS_ANDROID NOVA_ANDROID_AUDIO)
endif()

# Solo Linux Desktop enlaza las librerías compartidas de Ardour Core
if(NOT ANDROID AND NOT WIN32)
    target_include_directories(nova_ui_qt PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ARDOUR_SRC_DIR}
        ${ARDOUR_SRC_DIR}/libs
        ${ARDOUR_SRC_DIR}/libs/ardour
        ${ARDOUR_SRC_DIR}/libs/pbd
        ${ARDOUR_SRC_DIR}/libs/temporal
        ${ARDOUR_SRC_DIR}/libs/evoral
        ${ARDOUR_SRC_DIR}/libs/midi++2
        ${ARDOUR_SRC_DIR}/libs/libltc
        ${ARDOUR_SRC_DIR}/libs/libltc/ltc
        ${ARDOUR_SRC_DIR}/libs/lua
        ${ARDOUR_SRC_DIR}/libs/zita-resampler
        ${ARDOUR_SRC_DIR}/libs/zita-convolver
        ${ARDOUR_SRC_DIR}/libs/qm-dsp
        ${ARDOUR_SRC_DIR}/libs/audiographer
        ${ARDOUR_SRC_DIR}/libs/ptformat
        ${ARDOUR_BUILD_DIR}
        ${ARDOUR_BUILD_DIR}/libs
        ${ARDOUR_BUILD_DIR}/libs/ardour
        ${ARDOUR_BUILD_DIR}/libs/pbd
    )

    target_compile_definitions(nova_ui_qt PRIVATE
        COMPILER_INT128_SUPPORT
        WAF_BUILD
        PROGRAM_NAME="Ardour"
        PROGRAM_VERSION="v9"
    )

    find_library(FST_LIB fst HINTS "${ARDOUR_BUILD_DIR}/libs/fst")

    set(EXTRA_ARDOUR_LIBS "")
    if(EXISTS "${ARDOUR_BUILD_DIR}/libs/tk/suil/libsuil.so")
        list(APPEND EXTRA_ARDOUR_LIBS "${ARDOUR_BUILD_DIR}/libs/tk/suil/libsuil.so")
    endif()
    if(FST_LIB)
        list(APPEND EXTRA_ARDOUR_LIBS "${FST_LIB}")
    elseif(EXISTS "${ARDOUR_BUILD_DIR}/libs/fst/libfst.so")
        list(APPEND EXTRA_ARDOUR_LIBS "${ARDOUR_BUILD_DIR}/libs/fst/libfst.so")
    endif()

    target_link_libraries(nova_ui_qt PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::Core
        Qt6::QuickControls2
        PkgConfig::SIGCPP
        PkgConfig::GLIBMM
        PkgConfig::GLIB
        PkgConfig::GIOMM
        PkgConfig::LIBXML
        PkgConfig::SNDFILE
        PkgConfig::ALSA
        ${ARDOUR_BUILD_DIR}/libs/ardour/libardour.so
        ${ARDOUR_BUILD_DIR}/libs/pbd/libpbd.so
        ${ARDOUR_BUILD_DIR}/libs/temporal/libtemporal.so
        ${ARDOUR_BUILD_DIR}/libs/evoral/libevoral.so
        ${ARDOUR_BUILD_DIR}/libs/midi++2/libmidipp.so
        ${EXTRA_ARDOUR_LIBS}
    )
else()
    # Windows or Android: no Ardour libraries needed (stubs or not used)
    target_include_directories(nova_ui_qt PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(nova_ui_qt PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::Core
        Qt6::QuickControls2
    )
endif()

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

include(cmake/NovaQml.cmake)

nova_mark_singletons()
nova_collect_qml(NOVA_QML_FILES)

qt_add_qml_module(nova_ui_qt
    URI NovaStudio
    VERSION 1.0
    QML_FILES
        ${NOVA_QML_FILES}
    RESOURCES
        qml/skins/bandlab/topbar/icons/svg/IconLoop.svg
        qml/skins/bandlab/topbar/icons/svg/IconLoopActive.svg
        qml/skins/logic/topbar/icons/svg/LogicIconChevron.svg
        qml/skins/logic/topbar/icons/svg/LogicIconPencil.svg
        qml/skins/logic/topbar/icons/svg/LogicIconPointer.svg
        qml/skins/logic/topbar/icons/svg/LogicIconScissors.svg
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
# Tests unitarios sin dependencias externas (asserts simples + Qt6Core).
# Ejecutar con:  ctest --test-dir build --output-on-failure
# o directamente: ./tests/run_tests.sh
include(CTest)

if(BUILD_TESTING AND NOT ANDROID AND NOT WIN32)
    add_executable(test_project_policy
        tests/test_project_policy.cpp
        src/core/NovaProjectPolicy.cpp
    )
    target_link_libraries(test_project_policy PRIVATE Qt6::Core)
    target_include_directories(test_project_policy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME project_policy COMMAND test_project_policy)

    add_executable(test_simd_peaks
        tests/test_simd_peaks.cpp
        src/core/NovaSimdPeaks.cpp
    )
    target_include_directories(test_simd_peaks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME simd_peaks COMMAND test_simd_peaks)

    # El decodificador de audio (WAV propio + MP3 con minimp3). Usa QString,
    # por eso enlaza Qt6::Core.
    add_executable(test_audio_decoder
        tests/test_audio_decoder.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
    )
    target_link_libraries(test_audio_decoder PRIVATE Qt6::Core)
    target_include_directories(test_audio_decoder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_decoder PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_decoder COMMAND test_audio_decoder)

    # El reproductor con remuestreo. Usa QString (el error de load()) y el
    # decodificador (el test carga el WAV del fixture), por eso enlaza
    # Qt6::Core y compila tambien las fuentes del decodificador.
    add_executable(test_audio_player
        tests/test_audio_player.cpp
        src/audio/NovaAudioPlayer.cpp
        src/audio/NovaResampler.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
    )
    target_link_libraries(test_audio_player PRIVATE Qt6::Core)
    target_include_directories(test_audio_player PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_player PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_player COMMAND test_audio_player)

    # Los picos de la forma de onda, calculados del PCM ya decodificado. El
    # test carga el WAV del fixture, por eso necesita el decodificador y
    # Qt6::Core (el decodificador trabaja con QString).
    add_executable(test_audio_peaks
        tests/test_audio_peaks.cpp
        src/audio/NovaAudioPeaks.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
        src/core/NovaSimdPeaks.cpp
    )
    target_link_libraries(test_audio_peaks PRIVATE Qt6::Core)
    target_include_directories(test_audio_peaks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_peaks PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_peaks COMMAND test_audio_peaks)

    # El contrato QML <-> C++ se comprueba con un script: detecta los botones
    # que llaman a metodos inexistentes (fallan en runtime con TypeError).
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME qml_contract
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_qml_contract.py)

        # El contrato del script de build de Android: debe compilar para el ABI
        # que se le pida (arm64-v8a para telefonos, x86_64 para Waydroid).
        add_test(NAME android_build_script
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_android_build_script.py)
    endif()
endif()
