# Sonda GLES/PICA

Este ejecutable usa EGL pbuffer y no instala una aplicación Android.
Se compila para ARM64 con `tools/android/build_native.sh`, o para Linux con
`tools/android/test_gles_host.sh`. El segundo usa Mesa por software.

Las pruebas incluyen lectura de píxeles, profundidad, texturas, UBO, compute/SSBO,
seis programas de fragmentos generados por PICA y un dibujo TEV controlado.
También se verifican las 16 operaciones lógicas RGBA8 contra tablas de verdad
CPU, con todas las parejas de bytes y máscaras de escritura. Esta prueba exige
`GL_EXT_shader_framebuffer_fetch` coherente y usa un framebuffer RGBA8 sin MSAA.
`render_target_probe.h` comprueba el gestor de superficies propias: profundidad,
stencil, composición mediante blit, reutilización, cambio de tamaño y recreación.
Las pruebas del generador PICA se ejecutan dentro de uno de esos destinos.
`pica_draw_probe.h` entrega objetos `PicaDrawView` reales al consumidor GLES:
geometría con índices de 8/16 bits y base vertex, UBO, caché de shaders, blending,
culling, scissor, máscaras, profundidad y stencil. Comprueba rechazos de entradas
incompletas y recuperación del consumidor. Las texturas sintéticas usan el
decodificador tiled original de TriAevum: RGBA8, RGB565, RGB5A1, RGBA4, ETC1 y
ETC1A4, con repetición, espejo, clamp, filtrado y actualización del contenido.
Un caso combina geometría indexada, textura tiled y el fragment shader TEV real
con uniforms empaquetados upstream, y verifica el color resultante.
`pica_backend_probe.h` llama al nuevo backend por la interfaz virtual upstream
`PicaRenderBackend`. Comprueba identidad y conservación de superficies, aislamiento
de namespaces, borrados de color/profundidad/stencil, fills previos al primer
dibujo y su consumo único, rechazo de alias y escrituras parciales, finalizaciones
ordenadas, reset y recuperación tras dibujos rechazados. No ejecuta el arranque
del juego ni el puente de planes completo del runtime.
`pica_transfer_probe.h` verifica display transfers RGBA8: snapshot independiente,
reducción horizontal/2x2, rotación y flip, ajuste con bordes negros, overlays alfa,
aislamiento de namespaces y conservación del snapshot ante errores. Presenta en
un FBO prestado y en el framebuffer EGL por defecto (pbuffer en esta sonda).
Prueba además reset/reconfiguración y rechazo de una superficie prestada eliminada.
El host sigue siendo responsable de la ventana Android y de `eglSwapBuffers`.
`context_recreation_probe.h` destruye un contexto EGL temporal y crea otro sin
compartir objetos. Verifica que abandonar el backend del contexto destruido no
afecte a los dibujos y presentaciones del nuevo. La sonda restaura su contexto
original incluso si falla una comprobación.
`generated_pica_probe.h` usa el generador real de TriAevum con datos sintéticos;
su vertex shader sigue siendo una interfaz de prueba. No hay recursos del juego.

El generador y el traductor GL se compilan desde el checkout fijado por
`config/android-base.json`. Se conservan allí los avisos y la licencia upstream
(TriAevum, GPL-3.0-or-later; consultar sus archivos de licencia). Los archivos
propios de adaptación con cabecera SPDX usan GPL-3.0-or-later. No se incluyen
iMMERSE/MXAO ni otros efectos opcionales.

Un PASS no acredita todos los estados PICA, el backend completo ni gameplay.
Los resultados y límites se registran en `docs/ANDROID_NATIVE.md`.
El APK que presenta un patrón sintético en una ventana real se documenta en
`docs/ANDROID_WINDOW_APP.md`; comparte la biblioteca renderer con esta sonda.
