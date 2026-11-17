# Sonda GLES/PICA

Este ejecutable usa EGL pbuffer y no instala una aplicación Android.
Se compila para ARM64 con `tools/android/build_native.sh`, o para Linux con
`tools/android/test_gles_host.sh`. El segundo usa Mesa por software.

Las pruebas incluyen lectura de píxeles, profundidad, texturas, UBO, compute/SSBO,
seis programas de fragmentos generados por PICA y un dibujo TEV controlado.
También se verifican las 16 operaciones lógicas RGBA8 contra tablas de verdad
CPU, con todas las parejas de bytes y máscaras de escritura. Esta prueba exige
`GL_EXT_shader_framebuffer_fetch` coherente y usa un framebuffer RGBA8 sin MSAA.
`generated_pica_probe.h` usa el generador real de TriAevum con datos sintéticos;
su vertex shader sigue siendo una interfaz de prueba. No hay recursos del juego.

El generador y el traductor GL se compilan desde el checkout fijado por
`config/android-base.json`. Se conservan allí los avisos y la licencia upstream
(TriAevum, GPL-3.0-or-later; consultar sus archivos de licencia). Los archivos
propios de adaptación con cabecera SPDX usan GPL-3.0-or-later. No se incluyen
iMMERSE/MXAO ni otros efectos opcionales.

Un PASS no acredita todos los estados PICA, el backend completo ni gameplay.
Los resultados y límites se registran en `docs/ANDROID_NATIVE.md`.
