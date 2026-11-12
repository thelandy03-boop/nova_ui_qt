cmake_minimum_required(VERSION 3.20)
# WDL (el motor EEL2 de los JSFX) es C puro, por eso el proyecto tambien
# declara el lenguaje C. El resto de la app sigue siendo C++20.
project(nova_ui_qt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android-specific settings
if(ANDROID)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Compatibilidad con subproyectos como Oboe" FORCE)
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
if(NOT CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "C:/QtAndroid/6.8.0/android_arm64_v8a")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Qml Quick QuickControls2 Network)

# El moc de Qt. Qt6 ya fija CMAKE_AUTOMOC_EXECUTABLE en su propio paquete
# (que es el que vale en cada plataforma: el de Qt para Android en Android, el
# del escritorio en Linux).
#
# Antes se fijaba a pelo "/usr/lib/qt6/libexec/moc", que es la ruta del Qt de
# escritorio de esta maquina. En un runner de CI (o en cualquier equipo sin
# ese Qt de escritorio) la ruta no existe y el build falla con:
#   "No rule to make target '/usr/lib/qt6/libexec/moc', needed by
#    nova_ui_qt_autogen/timestamp"
# Por eso la ruta fija solo se aplica si de verdad existe; si no, manda la que
# dio Qt6.
if(EXISTS "/usr/lib/qt6/libexec/moc")
    set(CMAKE_AUTOMOC_EXECUTABLE "/usr/lib/qt6/libexec/moc" CACHE FILEPATH "Qt6 moc executable")
endif()
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

# ---------------------------------------------------------------------------
# WDL EEL2: motor real de los scripts JSFX
# ---------------------------------------------------------------------------
# Codigo vendorizado sin modificar de Cockos WDL (ver third_party/wdl/README.md).
# Se compila como libreria estatica C aparte:
#   - EEL_TARGET_PORTABLE evita generar codigo ejecutable en runtime, que es
#     lo que Android moderno no permite de forma fiable.
#   - WDL_FFT_REALSIZE=8 hace que la FFT use el mismo double que EEL2.
add_library(nova_wdl_eel2 STATIC
    third_party/wdl/eel2/nseel-caltab.c
    third_party/wdl/eel2/nseel-compiler.c
    third_party/wdl/eel2/nseel-eval.c
    third_party/wdl/eel2/nseel-lextab.c
    third_party/wdl/eel2/nseel-ram.c
    third_party/wdl/eel2/nseel-yylex.c
    third_party/wdl/eel2/nseel-cfunc.c
    third_party/wdl/fft.c
)
target_include_directories(nova_wdl_eel2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/wdl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/wdl/eel2
)
target_compile_definitions(nova_wdl_eel2 PUBLIC
    EEL_TARGET_PORTABLE
    WDL_FFT_REALSIZE=8
)
target_compile_definitions(nova_wdl_eel2 PRIVATE
    _FILE_OFFSET_BITS=64
)
# WDL es C puro: no necesita moc ni recursos. Qt activa AUTOMOC en todo el
# proyecto, asi que se desactiva solo para esta libreria.
set_target_properties(nova_wdl_eel2 PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# En ARM `char` es sin signo por defecto. El lexer de WDL espera el `char`
# con signo de x86 y sin este flag cambia la interpretacion de literales.
# `wdltypes.h` lo comprueba con un typedef que falla si `char` es sin signo,
# asi que el flag tambien hace falta en la unidad C++ que incluye sus
# cabeceras (el adaptador). No se aplica a toda la app: solo a esa unidad.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND
   (ANDROID_ABI MATCHES "^arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|armv7|arm)"))
    target_compile_options(nova_wdl_eel2 PRIVATE -fsigned-char)
    set_source_files_properties(
        src/audio/dsp/jsfx/NovaJsfxWdlEngine.cpp
        PROPERTIES COMPILE_OPTIONS "-fsigned-char"
    )
endif()

if(NOT WIN32)
    target_link_libraries(nova_wdl_eel2 PUBLIC m)
endif()

# YSFX vendorizado (adopcion en curso, no enlazado todavia). Ver third_party/ysfx/NOVA.md.
set(YSFX_PLUGIN OFF CACHE BOOL "" FORCE)
set(YSFX_GFX ON CACHE BOOL "" FORCE)
set(YSFX_PORTABLE ON CACHE BOOL "" FORCE)
set(YSFX_TESTS OFF CACHE BOOL "" FORCE)
set(YSFX_TOOLS OFF CACHE BOOL "" FORCE)
set(YSFX_DEEP_STRIP OFF CACHE BOOL "" FORCE)
set(YSFX_SKIP_CHECKSUM ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/ysfx ${CMAKE_CURRENT_BINARY_DIR}/ysfx
                 EXCLUDE_FROM_ALL)

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
    src/core/NovaUpdateChecker.h
    src/core/NovaUpdateChecker.cpp
    src/core/NovaPatchManager.h
    src/core/NovaPatchManager.cpp
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
    src/views/NovaJsfxHighlighter.h
    src/views/NovaJsfxHighlighter.cpp
    src/views/NovaJsfxGfxItem.h
    src/views/NovaJsfxGfxItem.cpp
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
    src/audio/NovaFlacDecoder.h
    src/audio/NovaFlacDecoder.cpp
    src/audio/NovaOggDecoder.h
    src/audio/NovaOggDecoder.cpp
    src/audio/NovaAudioPlayer.h
    src/audio/NovaAudioPlayer.cpp
    src/audio/NovaResampler.h
    src/audio/NovaResampler.cpp
    src/audio/NovaSpikeLoader.h
    src/audio/NovaSpikeLoader.cpp
    src/audio/NovaTapeStop.h
    src/audio/NovaTapeStop.cpp
    src/audio/dsp/NovaMidiEvent.h
    src/audio/dsp/NovaDspEffect.h
    src/audio/dsp/NovaDspChain.h
    src/audio/dsp/NovaDspChain.cpp
    src/audio/dsp/NovaBiquadEq.h
    src/audio/dsp/NovaBiquadEq.cpp
    src/audio/dsp/NovaCompressor.h
    src/audio/dsp/NovaCompressor.cpp
    src/audio/dsp/NovaStereoDelay.h
    src/audio/dsp/NovaStereoDelay.cpp
    src/audio/dsp/NovaReverb.h
    src/audio/dsp/NovaReverb.cpp
    src/audio/dsp/jsfx/NovaJsfxVm.h
    src/audio/dsp/jsfx/NovaJsfxVm.cpp
    src/audio/dsp/jsfx/NovaJsfxEffect.h
    src/audio/dsp/jsfx/NovaJsfxEffect.cpp
    src/audio/dsp/jsfx/NovaJsfxManager.h
    src/audio/dsp/jsfx/NovaJsfxManager.cpp
    src/audio/dsp/jsfx/NovaJsfxPackageManager.h
    src/audio/dsp/jsfx/NovaJsfxPackageManager.cpp
    src/audio/dsp/jsfx/NovaJsfxYsfxEngine.h
    src/audio/dsp/jsfx/NovaJsfxYsfxEngine.cpp
    third_party/minimp3/minimp3.h
    third_party/dr_libs/dr_flac.h
)

# Common Qt libraries for all platforms
target_link_libraries(nova_ui_qt PRIVATE Qt6::OpenGL Qt6::Quick Qt6::Qml Qt6::Core Qt6::QuickControls2 Qt6::Gui Qt6::Network ysfx)
target_include_directories(nova_ui_qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ysfx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ysfx/sources
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ysfx/thirdparty/WDL/source
)


if(ANDROID)
    set_property(TARGET nova_ui_qt PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")

    # Qt para Android no incluye OpenSSL: sin estas libs el backend TLS no
    # arranca y cualquier HTTPS (p. ej. NovaUpdateChecker) falla.
    set(NOVA_OPENSSL_ABI_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/android_openssl/ssl_3/${ANDROID_ABI}")
    if(EXISTS "${NOVA_OPENSSL_ABI_DIR}/libssl_3.so"
       AND EXISTS "${NOVA_OPENSSL_ABI_DIR}/libcrypto_3.so")
        set_property(TARGET nova_ui_qt APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
            "${NOVA_OPENSSL_ABI_DIR}/libssl_3.so"
            "${NOVA_OPENSSL_ABI_DIR}/libcrypto_3.so")
    else()
        message(WARNING
            "NOVA: faltan libssl_3/libcrypto_3 para ${ANDROID_ABI}; HTTPS no funcionara en el APK")
    endif()

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
nova_collect_resources(NOVA_QML_RESOURCES)

qt_add_qml_module(nova_ui_qt
    URI NovaStudio
    VERSION 1.0
    QML_FILES
        ${NOVA_QML_FILES}
    RESOURCES
        ${NOVA_QML_RESOURCES}
)

# Bundle de CAs Mozilla: garantiza verificacion TLS tambien en Android, donde
# el backend OpenSSL no lee el almacen del sistema por si solo.
qt_add_resources(nova_ui_qt "nova_ca_bundle"
    PREFIX "/certs"
    FILES third_party/cacert.pem
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

    # El decodificador de audio (WAV propio + MP3 con minimp3 + FLAC + OGG). Usa QString,
    # por eso enlaza Qt6::Core.
    add_executable(test_audio_decoder
        tests/test_audio_decoder.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
        src/audio/NovaFlacDecoder.cpp
        src/audio/NovaOggDecoder.cpp
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
        src/audio/NovaFlacDecoder.cpp
        src/audio/NovaOggDecoder.cpp
        src/audio/NovaTapeStop.cpp
        src/audio/dsp/NovaDspChain.cpp
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
        src/audio/NovaFlacDecoder.cpp
        src/audio/NovaOggDecoder.cpp
        src/core/NovaSimdPeaks.cpp
    )
    target_link_libraries(test_audio_peaks PRIVATE Qt6::Core)
    target_include_directories(test_audio_peaks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_peaks PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_peaks COMMAND test_audio_peaks)

    # El cargador del spike: decodifica el fichero y le calcula los picos en
    # un hilo aparte (nunca en el hilo de la interfaz) y el fichero se
    # decodifica una sola vez.
    add_executable(test_audio_loader
        tests/test_audio_loader.cpp
        src/audio/NovaSpikeLoader.cpp
        src/audio/NovaAudioPeaks.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
        src/audio/NovaFlacDecoder.cpp
        src/audio/NovaOggDecoder.cpp
        src/audio/NovaAudioPlayer.cpp
        src/audio/NovaResampler.cpp
        src/audio/dsp/NovaDspChain.cpp
        src/core/NovaSimdPeaks.cpp
    )
    target_link_libraries(test_audio_loader PRIVATE Qt6::Core)
    target_include_directories(test_audio_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_loader PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_loader COMMAND test_audio_loader)

    # Los procesadores de efectos DSP (ACE EQ, Compresor, Delay, Reverb)
    add_executable(test_audio_dsp
        tests/test_audio_dsp.cpp
        src/audio/dsp/NovaDspChain.cpp
        src/audio/dsp/NovaBiquadEq.cpp
        src/audio/dsp/NovaCompressor.cpp
        src/audio/dsp/NovaStereoDelay.cpp
        src/audio/dsp/NovaReverb.cpp
    )
    target_link_libraries(test_audio_dsp PRIVATE Qt6::Core)
    target_include_directories(test_audio_dsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME audio_dsp COMMAND test_audio_dsp)

    # El host y evaluador de scripts JSFX de Cockos REAPER
    add_executable(test_audio_jsfx
        tests/test_audio_jsfx.cpp
        src/audio/dsp/jsfx/NovaJsfxVm.cpp
        src/audio/dsp/jsfx/NovaJsfxYsfxEngine.cpp
        src/audio/dsp/jsfx/NovaJsfxEffect.cpp
        src/audio/dsp/jsfx/NovaJsfxManager.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
        src/audio/NovaFlacDecoder.cpp
        src/audio/NovaOggDecoder.cpp
    )
    target_link_libraries(test_audio_jsfx PRIVATE Qt6::Core ysfx)
    target_include_directories(test_audio_jsfx PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ysfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ysfx/sources
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ysfx/thirdparty/WDL/source
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minimp3
    )
    # Las plantillas JSFX viven en QML; el test las lee del árbol fuente para
    # comprobar que siguen compilando con WDL.
    target_compile_definitions(test_audio_jsfx PRIVATE
        NOVA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
    add_test(NAME audio_jsfx COMMAND test_audio_jsfx)

    # El gestor de parches de colaboración local (.novapatch)
    add_executable(test_patch_manager
        tests/test_patch_manager.cpp
        src/core/NovaPatchManager.cpp
        src/core/NovaLogging.cpp
    )
    target_link_libraries(test_patch_manager PRIVATE Qt6::Core)
    target_include_directories(test_patch_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME patch_manager COMMAND test_patch_manager)

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

        # El contrato del camino del spike de Android (el motor, el controlador
        # y el cargador solo se compilan en Android: sus behaviours criticos se
        # comprueban leyendo el codigo).
        add_test(NAME spike_contract
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_spike_contract.py)

        # El contrato del pipeline de build de Android: CMake compila el
        # nativo y Gradle solo empaqueta. Si alguien mezcla las dos etapas
        # (un bloque externalNativeBuild, un find_package(oboe) imposible, el
        # CMakeLists obsoleto de android/app/src/main/cpp/), el APK deja de
        # construirse. Ver AGENTS.md.
        add_test(NAME android_pipeline
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_android_pipeline.py)

        # El contrato de novedades de release: la version actual en Gradle debe
        # tener sus novedades registradas en qml/dialogs/whats_new.json antes de
        # compilar o publicar cualquier version.
        add_test(NAME whats_new_contract
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_whats_new.py)
    endif()
endif()
