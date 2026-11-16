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

1. Adaptar y validar el dialecto de shaders PICA a GLSL ES 3.20, incluidos precisión, bindings y profundidad.
2. Portar las operaciones GL de escritorio: `glClearDepth`/`glDrawBuffer`, composición, texturas, framebuffer y operaciones lógicas; resolver las diferencias semánticas con GLES mediante casos de prueba.
3. Integrar esa ruta en el host Android sin forzar Vulkan/NRI y validar presentación, recreación de superficies, controles y audio.
4. Generar/compilar el módulo MM3D ARM64 a partir de la ROM verificada, manteniendo sus fuentes traducidas y recursos privados.
5. Empaquetar el APK de desarrollo y demostrar arranque del juego en el moto g24 power.

El éxito de pruebas de servicios o de un triángulo sintético no acredita un APK jugable. Tampoco se extrapola rendimiento de Adreno 830 a Mali-G52.

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
