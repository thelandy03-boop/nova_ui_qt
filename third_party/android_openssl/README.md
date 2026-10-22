# OpenSSL para Android

Librerías prebuilt de **OpenSSL 3.1.8** para el backend TLS de Qt 6 en Android.
Qt no las incluye en sus binarios oficiales por motivos de exportación, así que
se empaquetan en el APK mediante `QT_ANDROID_EXTRA_LIBS` (ver `CMakeLists.txt`).

- Origen: https://github.com/KDAB/android_openssl (`ssl_3/<abi>/`)
- ABIs incluidas: `arm64-v8a` (teléfonos) y `x86_64` (Waydroid/emulador)
- Archivos: `libssl_3.so`, `libcrypto_3.so`
- Licencia: Apache License 2.0 (ver `LICENSE`)

Solo se copian las `.so`; los `.a` y las cabeceras del repositorio original no
son necesarios porque la app no enlaza OpenSSL directamente, solo lo carga el
plugin `libplugins_tls_qopensslbackend_*.so` en tiempo de ejecución.
