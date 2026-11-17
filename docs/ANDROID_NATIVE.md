# Primer hito Android: base ARM64 y compatibilidad del moto g24 power

El dispositivo objetivo es **moto g24 power**, Android 14 / API 34, ARM64, GPU Mali-G52 MC2. El port debe funcionar en este teléfono. El juego se mantiene como recompilación nativa; la adaptación del renderizador es una tarea separada.

## Base y organización

[config/android-base.json](../config/android-base.json) fija TriAevum/Moonfall Curse a `672d13f15cb336bc7651c861d23cf45924a9f936`, título `mm3d`, receta europea `mm3d-eur-local-6eec27b1`, ABI `arm64-v8a` y API 29. Sustituye la propuesta de mezclar el host MM3D-PC USA con el dump EUR.

El repositorio contiene los scripts de integración y una sonda GLES propia. El código upstream se obtiene en una copia privada, independiente y fijada por commit. Las modificaciones posteriores al runtime deberán conservarse como parches revisables; el script rechaza una copia upstream con modificaciones versionadas para no dar por reproducible una compilación desconocida.

| Ubicación | Contenido |
| --- | --- |
| `config/android-base.json` | Identidad del upstream, NDK y objetivo |
| `tools/android/` | Preparación, compilación, preparación de sondas y pruebas ADB |
| `ports/android/compat_probe/` | Prueba sintética EGL/OpenGL ES 3.2, sin material del juego |
| `$HOME/mm3d-android/source/TriAevum` en WSL | Fuentes upstream para compilar en el sistema de archivos Linux |
| `$HOME/mm3d-android/tools` en WSL | NDK r29 |
| `$HOME/mm3d-android/build/native-672d13f` en WSL | Compilación incremental ARM64 |
| `local/android-native-stage/` | Sondas y bibliotecas, con manifiesto de hashes; fuera de Git |
| `local/android-logs/` | Registros de compilación y pruebas; fuera de Git |
| `roms/`, `extracted/`, `private/`, `generated/` | ROM y derivados, fuera de Git |

La variable Linux `MM3D_ANDROID_WORK` permite cambiar la raíz de trabajo. Se usan dos compiladores simultáneos por defecto para los aproximadamente 4 GiB de RAM asignados a este WSL. `CMAKE_BUILD_PARALLEL_LEVEL` permite ajustar ese límite.

## Preparar las herramientas

Desde PowerShell, en la raíz del repositorio:

```powershell
wsl -d Ubuntu -u root -- apt-get update
wsl -d Ubuntu -u root -- apt-get install -y --no-install-recommends git build-essential python3 curl ca-certificates cmake ninja-build unzip pkg-config
wsl -d Ubuntu -- bash tools/android/setup_ndk.sh
```

El script descarga `android-ndk-r29-linux.zip` desde Google y verifica el SHA-1 publicado `87e2bb7e9be5d6a1c6cdf5ec40dd4e0c6d07c30b` antes de extraerlo. También verifica `Pkg.Revision = 29.0.14206865`. La fuente del checksum es el [archivo oficial del NDK](https://github.com/android/ndk/wiki/Unsupported-Downloads).

Herramientas instaladas en esta sesión: NDK r29 / Clang 21.0.0, CMake 4.2.3 y Ninja 1.13.2 sobre Ubuntu 26.04 WSL2. En otras distribuciones, comprobar que CMake sea >= 3.30. ADB y Python se ejecutan desde Windows para utilizar la conexión USB existente.

## Compilar y comprobar

```powershell
wsl -d Ubuntu -- bash tools/android/build_native.sh
python -B tools/android/check_device.py
python -B tools/android/test_device.py
```

También están disponibles como tareas `MM3D:` de VS Code. La tarea de compilación es la predeterminada. El script de build compila servicios, cargador, pruebas upstream y la sonda GLES; no requiere ROM ni genera un APK.

`stage_native.py` comprueba ELF64/AArch64 y segmentos LOAD alineados a 16 KiB, copia solamente las sondas y sus bibliotecas y genera un manifiesto con SHA-256. `test_device.py` verifica esos hashes, usa el único teléfono USB (`--serial` permite elegir otro) y crea un directorio de prueba nuevo en `/data/local/tmp/`. Cada proceso remoto tiene un límite de 60 segundos. Los resultados quedan en JSON local, sin número de serie.

La comprobación de GPU devuelve código 1 cuando los requisitos conocidos del renderizador upstream no se cumplen. Eso es el resultado esperado en este moto con Vulkan 1.1, no un error de instalación del NDK. La prueba completa conserva por separado el resultado de cada servicio, NRI y GLES. No se debe convertir un fallo de NRI en una afirmación de que todo el runtime está validado.

## Límite gráfico confirmado

El informe real `adb shell cmd gpu vkjson` declara **Vulkan 1.1.177**. Dispone de `VK_KHR_create_renderpass2`, pero carece de `VK_KHR_synchronization2` y `VK_KHR_dynamic_rendering`.

El [selector de GPU upstream](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/runtime/three_ds_recomp/src/fast/extension/vulkan_adapter_policy.cpp) exige Vulkan >= 1.2. El [puente NRI](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/runtime/three_ds_recomp/src/fast/extension/nri_interop_context.cpp) necesita synchronization2. Por tanto, el renderizador actual no es compatible con este dispositivo sin adaptación; no basta con bajar la comprobación de versión.

La ruta OpenGL existente tampoco es un port GLES listo: [gfx_opengl_pica.cpp](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/runtime/three_ds_recomp/src/fast/backends/gfx_opengl_pica.cpp) desactiva PICA con `USE_OPENGLES` y requiere OpenGL 4.3. La línea de trabajo propuesta es adaptar esta ruta a GLES 3.2, preservando la lógica nativa y las operaciones PICA. Su viabilidad completa sigue pendiente de pruebas con los shaders y recursos del juego.

La sonda GLES crea un contexto EGL 3.2 y un pbuffer RGBA8/depth24, compila shaders ES 3.20, dibuja un triángulo y verifica un píxel. Después ejecuta un compute shader, escribe un SSBO y verifica su contenido desde CPU. Consulta extensiones de framebuffer fetch y border clamp para orientar la adaptación. Esto comprueba operaciones reales de GPU, pero no una ventana Android, el renderizador PICA ni el juego.

## Trabajo siguiente para este teléfono

### Validación sin teléfono

La sonda también puede compilarse para Linux y ejecutarse en Mesa/llvmpipe, con
renderizado por software. Usa el mismo traductor y los mismos casos de dibujo que
el binario Android. Desde PowerShell:

```powershell
wsl -d Ubuntu -u root -- apt-get install -y libegl-dev libgles-dev libgl1-mesa-dri
wsl -d Ubuntu -- bash tools/android/test_gles_host.sh
```

El script exige el checkout upstream limpio y fijado, usa un directorio de build
separado y guarda el resultado en `local/android-logs/gles-host.log`. También hay
una tarea de VS Code. Este resultado no acredita compatibilidad ni rendimiento
en Mali. Para repetir solamente la sonda gráfica en el teléfono conectado:

```powershell
wsl -d Ubuntu -- bash tools/android/build_native.sh
python -B tools/android/test_device.py --case gles32_draw_and_compute
```

Con `--case`, el código de salida refleja únicamente las pruebas seleccionadas;
el informe conserva la evaluación Vulkan independiente y el hash del ejecutable.
Sin ese argumento se mantiene la comprobación completa, incluido el fallo NRI.

### Pendientes de integración

1. Adaptar y validar el dialecto de shaders PICA a GLSL ES 3.20, incluidos precisión, bindings y profundidad.
2. Portar las operaciones GL de escritorio: `glClearDepth`/`glDrawBuffer`, composición, texturas, framebuffer y operaciones lógicas; resolver las diferencias semánticas con GLES mediante casos de prueba.
3. Integrar esa ruta en el host Android sin forzar Vulkan/NRI y validar presentación, recreación de superficies, controles y audio.
4. Generar/compilar el módulo MM3D ARM64 a partir de la ROM verificada, manteniendo sus fuentes traducidas y recursos privados.
5. Empaquetar el APK de desarrollo y demostrar arranque del juego en el moto g24 power.

El éxito de pruebas de servicios o de un triángulo sintético no acredita un APK jugable. Tampoco se extrapola rendimiento de Adreno 830 a Mali-G52.

## Revisión y continuación del 17 de noviembre de 2026

Se encontró otra copia del proyecto en WSL, `~/mm3d-android`,
con el commit `6053bfc`. Al revisarla, sus archivos de `ports/`, `tools/android/`,
`docs/` y `config/` coincidían byte por byte con esta copia Windows. Sus exclusiones
adicionales para NDK, `source/`, `.ssh/` y `.venv/` se incorporaron aquí. No se
encontraron archivos del juego versionados en ninguna de las dos copias revisadas.
Los cambios de esta continuación están en la copia Windows; la copia Git WSL
no se sincroniza automáticamente. El checkout upstream y los builds siguen en WSL.

La traducción inicial reutiliza el conversor GL de TriAevum y añade GLSL ES 3.20
y precisión alta para float, int, sampler2D y usampler2D. Sigue siendo un subconjunto:
rechaza variantes con `#extension` y necesita validar los shaders completos
generados por PICA antes de integrar el backend.

**PASS en Mesa llvmpipe (GLES 3.2)**: compilación/enlace, texturas RGBA8 y R32UI,
bindings 0/2/13, actualización de UBO, profundidad 0.25/0.75 con coordenada
homogénea w=2, oclusión en ambos órdenes, recorte fuera de los planos cercano y
lejano, rechazo de dialectos no admitidos, dibujo básico y compute/SSBO.
Los casos usan shaders sintéticos originales, sin datos derivados de la ROM.
La compilación Android y el staging de 12 ELF ARM64 también pasaron.

Tras conectar el teléfono, la prueba ampliada obtuvo **PASS en el moto g24 power**
(Android 14, Mali-G52 MC2, GLES 3.2), con salida 0 y sin errores en stderr.
Se verificaron los mismos casos de texturas, uniforms, profundidad, oclusión,
recorte y compute descritos arriba, ejecutando la sonda por ADB.
Evidencia privada: `local/android-logs/device-20261117T135836851508Z.json`.
SHA-256 del ejecutable probado:
`0ad5403b030871ffc596ca99b5249e5db70bf23787611f3ec8ce31f9535331dc`.
El requisito Vulkan/NRI sigue sin cumplirse; este PASS corresponde a la ruta GLES
sintética. No hay APK ni arranque del juego validado.

### Generador PICA ejecutado en GLES

La sonda incorpora ahora el generador de fragmentos del checkout upstream fijado,
compilado directamente sin copiar sus fuentes al repositorio. Genera y traduce
las seis variantes paramétricas base: iluminación desactivada, con tablas LUT o
sin tablas LUT, cada una con textura 0 normal o entera. Las seis compilan y enlazan
en Mesa y en la Mali-G52 MC2 del moto g24 power.

Además, la variante sin iluminación y con textura normal se ejecuta: un UBO en
binding 4 configura las seis etapas TEV para seleccionar el color primario.
El dibujo devuelve RGBA aproximadamente (64,128,191,255), como se esperaba.
El tamaño del UBO se consulta al controlador; no se presupone su disposición total.
El vertex shader de esta prueba es una interfaz sintética compatible, y los datos
de entrada no provienen de la ROM. La iluminación y las otras cinco variantes se
validan por compilación/enlace, no por comparación de sus resultados de dibujo.

**PASS en el teléfono**, informe privado
`local/android-logs/device-20261117T140334667938Z.json`. El informe registra el
hash del ejecutable y los campos `generated_pica_fragment_variants_linked: 6`
y `generated_pica_tev_draw_readback: true`. Se conserva también el PASS de
profundidad, recorte, texturas y compute de las pruebas anteriores.

Esto todavía no integra `gfx_opengl_pica.cpp` en Android: el backend upstream
desactiva PICA bajo `USE_OPENGLES` y usa operaciones de escritorio como
`glLogicOp`, `glDrawBuffer` y `glClearDepth`. Queda adaptar esas operaciones,
validar más estados PICA (incluidas sombras), generar vértices desde programas
PICA y conectar el host Android y el módulo MM3D.

### Operaciones lógicas RGBA8 en GLES

Se añadió `ports/android/renderer/pica_gles_logic.h`: las 16 operaciones de
`PicaLogicOperation` se calculan sobre canales de 8 bits mediante GLSL. El destino
se obtiene con `GL_EXT_shader_framebuffer_fetch` coherente; blending y dithering
deben estar desactivados. La sonda exige la extensión y falla si no está disponible.

**PASS en Mesa y en el moto g24 power**. Por operación se comparan las 65 536
parejas posibles de valores fuente/destino, en los cuatro canales de una superficie
RGBA8 de 256x256, contra tablas de verdad independientes calculadas en CPU.
Un segundo dibujo comprueba el destino actualizado y máscaras que preservan verde
y alfa. Se verifican los píxeles de ambos dibujos. No se ensayan MSAA ni formatos
RGB565/RGBA4/RGB5A1; tampoco está conectado todavía al backend completo.

Informe privado: `local/android-logs/device-20261117T141111789604Z.json`.
Los campos `rgba8_logic_operations: 16` y `logic_coherent_fetch_and_masks: true`
registran el éxito. También pasaron las pruebas anteriores del generador PICA,
profundidad, texturas y compute en ese mismo ejecutable.

### Composición de TEV y operaciones lógicas

`TranslatePicaFragmentWithRgba8Logic` conecta el fragment shader generado por
upstream con las operaciones lógicas. Convierte su salida a `inout`, conserva el
color de destino antes de ejecutar el shader y aplica la operación al resultado
TEV. Añade la extensión coherente y el uniform `mm3d_logic_operation`. Esta es
una variante de programa distinta: al integrarla, la caché del backend deberá
distinguirla de la variante normal. El GLSL canónico de entrada no se modifica.

El adaptador requiere la declaración de salida del generador fijado, rechaza los
productores de sombras y falla si falta la capacidad de framebuffer fetch. Su
contrato actual es RGBA8, con blending/dithering desactivados. No es un conversor
de GLSL arbitrario ni está conectado aún al despachador de dibujos del runtime.

**PASS en Mesa y en el moto** para las 16 operaciones aplicadas a un dibujo TEV
generado sin iluminación. Se comprueban además alpha-test NEVER (conserva el
destino al descartar el fragmento) y profundidad explícita: los uniforms producen
`gl_FragDepth=0`, y GREATER contra profundidad cero debe rechazar el dibujo aunque
la profundidad rasterizada sea 0.5. Esto verifica que la envoltura conserva ambas
semánticas del shader original.

Evidencia privada: `local/android-logs/device-20261117T141816343253Z.json`, campo
`generated_pica_logic_discard_depth: true`. Continúan pasando las pruebas previas.

### Gestión de superficies GLES

`GlesRenderTarget` posee un framebuffer RGBA8 con profundidad de 24 bits y stencil
de 8 bits. Reutilizar las mismas dimensiones conserva los objetos y su contenido.
El cambio de tamaño prepara los nuevos objetos antes de sustituir los anteriores;
los tamaños inválidos se rechazan sin perder el destino anterior. La liberación
es explícita o por destructor y exige el contexto GLES propietario activo.
El gestor conserva bindings ajenos y actualiza los bindings de lectura/escritura,
renderbuffer y unidad de textura activa que apuntaban a los objetos sustituidos.
Los consumidores en otras unidades deben volver a vincular la nueva textura.

**PASS en Mesa y en el moto g24 power**:

- Creación y reutilización de superficies, rechazo de dimensiones nulas/excesivas.
- Pruebas de profundidad y stencil verificadas mediante lectura de píxeles.
- Copia de superficies 400x240 y 320x240 a una composición 400x480, con margen negro.
- Cambio de tamaño mientras el destino está vinculado, eliminación de objetos
  anteriores, liberación y recreación.
- Ejecución de los fragment shaders PICA, TEV y operaciones lógicas dentro de una
  superficie propia del gestor, conservando las pruebas de alfa y profundidad.

Informe privado: `local/android-logs/device-20261117T144026164738Z.json`, campo
`owned_targets_depth_stencil_resize_blit: true`. La composición se verifica fuera
de pantalla: todavía no presenta las pantallas del juego en una Activity Android.
Este gestor tampoco resuelve la caché de memoria 3DS, alias entre superficies,
formatos de color empaquetados, MSAA ni pérdida/recreación del contexto Android.

### Consumidor de dibujos y texturas PICA

`ports/android/renderer/pica_gles_draw.cpp` consume directamente el tipo
`Fast::Renderer3ds::PicaDrawView` del upstream fijado. Sube geometría e índices,
vincula UBO en 0/4, traduce y almacena programas GLES, y aplica blending, culling,
scissor, máscaras, profundidad, stencil y operaciones lógicas RGBA8. La variante
lógica tiene su propia entrada de caché. Valida los atributos y uniforms activos
del programa, límites de buffers y estados admitidos antes de dibujar.

Las texturas 2D de CPU pasan por `DecodePicaTextureRgba8` del upstream, conservando
su decodificación tiled. Se suben como RGBA8 a las unidades 1/2/3 y admiten clamp,
repeat, mirrored repeat y filtros nearest/linear en nivel cero. Los bytes se
decodifican y suben en cada envío: no se presupone que un hash sin cambios implique
contenido idéntico. Esta implementación prioriza corrección; aún no tiene caché
de texturas ni de geometría.

**PASS en Mesa y en el moto g24 power**, con lectura de píxeles:

- Geometría con índices de 8/16 bits, base vertex, listas/tiras/abanicos de
  triángulos, estados de rasterizado y recuperación tras entradas rechazadas.
- Separación de programas normales y lógicos en caché, liberación y recreación.
- Texturas sintéticas RGBA8, RGB565, RGB5A1, RGBA4, ETC1 y ETC1A4; repetición,
  espejo, clamp, filtrado y actualización de bytes conservando el mismo hash.
- Dibujo indexado con fragment shader paramétrico TEV original sin iluminación,
  uniforms empaquetados upstream y textura tiled. El color verificado es
  RGBA (255,128,64,255), distinto del color primario rojo de los vértices.

Informe privado: `local/android-logs/device-20261117T173556017075Z.json`, salida 0.
Campos: `upstream_pica_draw_contract`, `pica_native_texture_upload_sampling` y
`generated_tev_textured_draw_contract`, todos `true`. La misma ejecución conserva
las comprobaciones anteriores de superficies, generador PICA, lógica y compute.
La compilación ARM64 y el staging de los 12 ELF también pasaron.

La prueba combinada detectó inicialmente una máscara TEV sin referencias a
texturas en sus datos de entrada. Se corrigió construyendo `PicaTevProgram` con
`Control[1]=1`, además de habilitar la textura en el control del fragmento.
La comprobación conserva el color esperado y ahora informa los canales reales
ante discrepancias.

Este consumidor es síncrono y todavía no está conectado al despachador completo
ni a una ventana Android. Exige destino RGBA8/depth24/stencil8, texturas 2D de
CPU de hasta 1024x1024, nivel cero y shaders con la interfaz admitida. Rechaza
LUT de iluminación, sombras, referencias a copias GPU, mipmaps, sesgo LOD,
wrapping de borde/asimétrico, geometría programable y otros estados no resueltos.
La caché inicial admite hasta 64 programas y falla al excederlos; necesita una
política adecuada para el juego. El llamador conserva el orden y borrado de
superficies; falta resolver alias y coherencia con la memoria 3DS. Los vértices y
texturas del ensayo son sintéticos. No acredita aún escenas ni rendimiento del
juego. Las fases siguientes se detallan en [ROADMAP.md](ROADMAP.md).

### Adaptador inicial de la interfaz del runtime

`PicaGlesBackend` implementa la interfaz virtual `PicaRenderBackend` del checkout
fijado. El código upstream no se modifica. Usa `PicaGlesDraw` y conserva sus
superficies por namespace, direcciones físicas de color/profundidad y dimensiones.
Un dibujo posterior sobre el mismo destino conserva color, profundidad y stencil.
Un destino nuevo empieza transparente, con profundidad 1 y stencil 0.

El adaptador implementa borrado completo, reflejo GPU de fills GX de 32 bits que
coinciden exactamente con un attachment, cola de finalizaciones y reset.
Los fills anteriores a la creación de un destino se retienen hasta el primer
dibujo válido que los consume; repetir exactamente su rango conserva el valor
más reciente. Se rechazan escrituras parciales o rangos pendientes solapados.
Los alias entre destinos y las texturas que leen memoria de un destino también
se rechazan, en lugar de dibujar con contenido GPU desactualizado. Los mismos
rangos en namespaces diferentes tienen superficies independientes.

Las finalizaciones siguen el contrato GL upstream: confirman que los spans de
entrada ya se copiaron a objetos propios. No son un fence que garantice GPU
inactiva. El llamador sigue siendo responsable de la RAM del invitado y de sus
interrupciones. Los límites iniciales son 64 destinos, 128 fills pendientes y
128 finalizaciones sin recoger. Los límites de programa y estados del consumidor
de dibujos siguen vigentes.

**PASS en Mesa/llvmpipe y compilación ARM64 correcta**, 17 de noviembre de 2026.
La sonda llama por una referencia `PicaRenderBackend&`, verifica píxeles de
color/profundidad/stencil, persistencia, aislamiento, fills pendientes consumidos
una sola vez, rollback de destinos nuevos ante un dibujo inválido, rechazos de
alias, orden de finalizaciones, reset y reutilización. El campo nuevo es
`runtime_backend_targets_fills_completions: true`.
Registro local: `local/android-logs/gles-host.log`. SHA-256 del ejecutable ARM64
preparado en el manifiesto:
`05f28b638f8f1865ed4ba1e61cfac4276ffed3a6d87dbd9b821446470c0805a1`.

**Prueba de esta versión en el moto pendiente**: `adb devices` devolvió una lista
vacía y la ejecución se detuvo en `get-state`, antes de subir o ejecutar archivos.
Los PASS anteriores en el teléfono corresponden a las versiones identificadas
en sus informes, no a este adaptador nuevo.

Falta instanciar el adaptador en el host Android y conectarlo al puente completo
de planes del runtime. La sonda usa datos sintéticos y la interfaz virtual, sin
arrancar ese host. La transferencia y el scanout añadidos después se describen
abajo; siguen pendientes la copia GPU a RAM, capturas de productores y una
ventana Android visible. Tampoco se implementan snapshots para
restaurar estado GPU al pausar; la implementación base rechaza esas operaciones.

### Transferencia y presentación GLES

`SubmitPicaDisplayTransfer` copia ahora un destino RGBA8 a una imagen propia,
identificada por namespace y dirección física de salida. La copia conserva su
contenido aunque el origen se borre o reciba más dibujos. Un fallo de validación
o presentación conserva la imagen publicada anteriormente. Se admiten el tamaño
original, reducción horizontal por dos y reducción 2x2 con filtrado lineal; las
dimensiones deben coincidir con la fuente completa y el modo declarado.

`GlesScanout` transforma el formato físico vertical de CTR a la imagen lógica
horizontal. Aplica el flip derivado de los flags de transferencia, ajusta la
relación de aspecto y centra la imagen. Replace borra el destino a negro opaco;
AlphaOverlay conserva el contenido y combina RGB por alfa fuente. El alfa de
salida se calcula como `src.a + dst.a * (1 - src.a)`.

El host presta el framebuffer y sus dimensiones mediante
`ConfigurePresentation`; el framebuffer 0 permite usar la superficie EGL del
host. `DisablePresentation` desvincula ese destino sin destruirlo. El propietario
debe reconfigurarlo al cambiar de tamaño y desvincularlo antes de destruirlo.
Reset elimina las imágenes y shaders y exige configurar otra vez la presentación.
Este código no crea una Activity ni llama a `eglSwapBuffers`.

**PASS en Mesa/llvmpipe y compilación ARM64 correcta**. La prueba comprueba:

- Cuatro colores asimétricos para detectar rotación o flip incorrectos.
- Bordes negros y presentación tanto en un FBO prestado como en el framebuffer
  EGL por defecto; este último es un pbuffer, no una ventana Android.
- Reducción horizontal y 2x2 de texeles alternados, verificando el promedio.
- Transparencias sobre rojo, preservando el contenido fuera de la imagen.
- Aislamiento de namespaces, conservación tras cambiar el origen, resize de la
  imagen de transferencia, reset y recreación.
- Rechazo de modos/formatos no implementados, dimensiones incompatibles, alias,
  presentación sin configurar y framebuffer prestado eliminado; ante errores se
  conserva el snapshot publicado anteriormente.

El campo de resultado es `display_transfer_rotation_scaling_overlay: true`.
Registro Mesa: `local/android-logs/gles-host.log`. SHA-256 ARM64 preparado:
`a3b8308211da6beb937b8b0dab7983f0a747029c2ae52ba2c84a5f9274f83887`.

**PASS también en el moto g24 power**, con el mismo ejecutable y salida 0.
Informe privado: `local/android-logs/device-20261117T204855650869Z.json`.
La ejecución confirma además `runtime_backend_targets_fills_completions: true`,
por lo que cierra la validación del adaptador que antes estaba pendiente por
desconexión USB. Continúan pasando las pruebas anteriores de shaders, texturas,
superficies, profundidad/stencil, operaciones lógicas y compute. Todo se ejecuta
fuera de pantalla mediante ADB; no presenta todavía una aplicación visible.

Esta ruta inicial admite RGBA8 a RGBA8 con salida lineal, flags de flip,
input-linear/dont-swizzle y escalado reconocidos. Otros bits se rechazan. No
materializa escrituras en la RAM 3DS ni completa interrupciones por su cuenta:
el runtime debe solicitar la finalización después del envío. Se rechazan
`MaterializeGuest`, `RenderTargetCapture`, raw texture copies, salidas tiled,
formatos empaquetados, recortes de la fuente y alias. Hay un límite inicial de
64 imágenes; se crea un reemplazo por transferencia para conservar la imagen
anterior si falla. Faltan reutilización eficiente, integración con el host,
composición de ambas pantallas y pruebas del ciclo de vida Android.

### APK técnico visible y recreación de contexto

El prototipo de [ANDROID_WINDOW_APP.md](ANDROID_WINDOW_APP.md) ya está instalado
y probado en el moto g24 power. Usa una NativeActivity con EGL 3.2, el generador
TEV real y datos sintéticos, presentados mediante `eglSwapBuffers`. Comparte
`mm3d_gles_renderer` con la sonda; no introduce una segunda ruta de dibujo.

**PASS** en ventana visible, regreso tras salir al inicio y giro de pantalla con
recreación de Activity/superficie/contexto. Se revisaron capturas vertical y
horizontal. Evidencia privada: `local/android-logs/window-20261117T211340246938Z/`.
El APK probado es `local/android-apk/mm3d-window-debug.apk`, SHA-256
`6437db12b09bdca9efa7f0e1f27913c2ec0482d818dbd343197db3e99b5f2d05`.

Se añadió `AbandonContext` a los propietarios de recursos GLES para olvidar
nombres de un contexto perdido sin intentar borrarlos en otro. Las operaciones
Release/destructor sobre objetos vacíos no hacen llamadas GL. Una prueba crea un
backend, destruye su contexto, crea otro independiente y confirma que abandonar
el backend viejo conserva los recursos y dibujos del nuevo. No equivale a
provocar el error real `EGL_CONTEXT_LOST` del controlador.

**PASS en Mesa y en el moto**, informe privado
`local/android-logs/device-20261117T211655771694Z.json`, salida 0 y campo
`egl_context_recreation_abandon: true`. Continúan pasando todos los casos previos
de dibujo, superficies, lógica, transferencias y compute en la misma ejecución.

Falta conectar el módulo del juego y su puente de planes al host: este APK
todavía no carga MM3D, ni incluye sus archivos o recursos. Tampoco tiene menú de
configuración, controles, audio o guardado. La pausa/recreación se valida aquí
con un patrón reconstruible, sin estado de juego que conservar. El host SDL
upstream queda sin modificaciones.

### Módulo real MM3D ARM64

La [ruta privada del módulo](ANDROID_TITLE_MODULE.md) ya generó 18 751 funciones
en 259 archivos y compiló la biblioteca ARM64 con NDK r29. El cargador real
upstream la aceptó en el moto g24 power: ABI V2, 241 681 entradas, entrada del
juego y capacidades de observación de escrituras/salida verificadas, con salida 0.
Informe privado: `local/android-logs/title-20261117T215135776549Z.json`.

Esta comprobación carga y consulta el módulo mediante ADB; no ejecuta funciones
del juego ni lo integra todavía con el host gráfico. La biblioteca y las fuentes
traducidas siguen excluidas de Git y del APK sintético. El siguiente trabajo es
conectar módulo, runtime, recursos locales y planes PICA al backend GLES.

### Módulo real y ejecución inicial de CPU

El [módulo privado MM3D](ANDROID_TITLE_MODULE.md) ya compila para ARM64 y pasa
la carga ABI V2 en el moto. Una prueba adicional ejecuta desde `0x100000` hasta
el primer SVC (`0x21`, `CreateAddressArbiter`), sin atenderlo: 292 449 bloques,
cero fallos de memoria e instrucciones rechazadas. Al reanudar con presupuestos
de bloques coinciden los registros y el contenido de memoria con la ejecución
continua. Evidencia privada: `local/android-logs/startup-20261117T220115943912Z.json`.

Esta ruta es independiente del APK sintético y no usa GPU ni RomFS en el
dispositivo. Falta unir los servicios y el planificador CTR, los observadores
MM3D, recursos y backend GLES para avanzar hacia el primer arranque con imagen.

## Resultado del 16 de noviembre de 2026

Compilación ARM64 completada: 62 pasos upstream y 2 de la sonda GLES propia. Se verificaron y prepararon 12 archivos ELF64/AArch64 con alineación LOAD de al menos 16 KiB. Las pruebas se ejecutaron en el moto g24 power conectado por USB, como procesos del usuario shell de ADB.

| Prueba | Resultado |
| --- | --- |
| Política de presentación | PASS |
| Distribución de memoria del runtime | PASS |
| Servicio de entrada | PASS |
| Servicio de audio | PASS |
| Servicio de archivos | PASS |
| Cargador nativo con módulo sintético | PASS |
| Rechazo del módulo de juego vacío | PASS |
| Creación de dispositivo Vulkan/NRI upstream | FAIL, salida 133: el controlador no admite Vulkan 1.2+ |
| GLES 3.2: dibujo RGBA8/depth24, lectura de píxel, compute y SSBO | PASS |

Son **8 pruebas correctas y 1 fallo gráfico confirmado**. Las pruebas de servicios usan casos sintéticos; el PASS de audio no equivale a reproducir el sonido del juego. La sonda GLES confirma ejecución real en Mali-G52 MC2 y anuncia `GL_EXT_shader_framebuffer_fetch`, `GL_ARM_shader_framebuffer_fetch` y `GL_EXT_texture_border_clamp`. La presencia de esas extensiones no valida todavía su uso para reproducir las operaciones PICA.

Evidencias locales: `local/android-logs/native-build.log`, `local/android-native-stage/manifest.json` y `local/android-logs/device-20261116T212027774375Z.json`. El código de salida global es 1 porque se conserva el fallo de Vulkan/NRI y del requisito de GPU; no se oculta al pasar GLES. El siguiente hito es la adaptación PICA/GLES, no repetir la instalación del NDK.
