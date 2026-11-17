# Majora's Mask 3D — Android Port

Proyecto para desarrollar un port nativo de **The Legend of Zelda: Majora's Mask 3D**, la versión de **Nintendo 3DS**, a **Android**.

El repositorio se aloja actualmente en [`Lumarri/oot64-port`](https://github.com/Lumarri/oot64-port).

## Estado

La copia europea Rev 1 está verificada y extraída en local. La base nativa seleccionada es **TriAevum/Moonfall Curse `672d13f`**, fijada en [config/android-base.json](config/android-base.json). El objetivo obligatorio es **moto g24 power** (Android 14, ARM64, Mali-G52). Hay herramientas de compilación WSL, sondas ARM64 y una prueba GLES 3.2; todavía no hay APK jugable.

Primer hito ejecutado en el teléfono: **7 pruebas de servicios/cargador y la prueba gráfica GLES 3.2 pasaron**. Vulkan/NRI falló por requerir Vulkan 1.2. Los resultados y límites están en [ANDROID_NATIVE.md](docs/ANDROID_NATIVE.md#resultado-del-16-de-noviembre-de-2026).

Avance del **17 de noviembre de 2026**, validado también en la Mali-G52 del moto:

- Seis variantes base de fragment shaders PICA generadas por TriAevum compilan y enlazan en GLES 3.2.
- Un dibujo TEV generado devuelve los píxeles esperados.
- Las 16 operaciones lógicas RGBA8 pasan pruebas de lectura del destino y máscaras de escritura.
- La combinación de TEV y operaciones lógicas conserva el descarte por alfa y la profundidad explícita.

Son pruebas controladas sin datos del juego. El backend completo, el módulo MM3D y el APK siguen pendientes.

## Objetivo

Ejecutar Majora's Mask 3D en Android ARM64 (`arm64-v8a`), con controles táctiles, soporte para mandos, una interfaz adaptada al teléfono y partidas guardadas en el dispositivo.

La ruta de trabajo es la recompilación nativa: traducir localmente el código del juego y adaptarlo a un runtime Android. Los recursos del juego se obtienen de la copia local del usuario.

## Siguiente paso

Conectar la adaptación GLES al procesamiento de dibujos del runtime y cubrir los formatos y estados PICA pendientes. Su Vulkan 1.1.177 no cumple los requisitos del renderizador actual de TriAevum; la ruta en desarrollo usa OpenGL ES 3.2.

El dump europeo CTR-P-AJRP coincide con la receta de Moonfall Curse. La [guía de compilación y pruebas Android](docs/ANDROID_NATIVE.md) documenta la base, comandos, resultados y límites del primer hito.

Las copias de upstream se mantienen fuera de GitHub. Al integrar código, se conservarán GPL-3.0-or-later y las atribuciones de cada componente; iMMERSE/MXAO se excluirá por el alcance de su permiso upstream.

## ROM y recursos

La copia local verificada es **Majora's Mask 3D Europa Rev 1** (CTR-P-AJRP, programa `0004000000125600`, build 1.1.0), descifrada, en `.3ds`. Coincide con los identificadores y el SHA-256 de `code.bin` que documenta [Moonfall Curse / TriAevum](https://github.com/coccofresco/TriAevum). MM3D-PC documenta en cambio **USA v1.1** (CTR-P-AJRE): no mezclar esos requisitos.

La [auditoría de la ROM y evaluación Android](docs/ROM_VERIFICATION.md) confirma también los hashes completos de ExHeader y RomFS contra la receta oficial de TriAevum `672d13f`. Esa base ya incluye una ruta Android específica para Moonfall Curse, todavía pendiente de validación en dispositivo.

Para volver a extraer ExHeader, `code.bin` y RomFS (fuera de Git):

```powershell
python tools/extract_mm3d_rom.py
```

La ROM, los recursos extraídos, el clon de MM3D-PC y el código generado a partir del juego se mantienen localmente, fuera de GitHub. `.gitignore` excluye `local/`, `roms/`, `extracted/`, `private/` y `generated/`, los formatos de 3DS y los archivos de extracción habituales.

## Plan de trabajo

Consulta [docs/ROADMAP.md](docs/ROADMAP.md) para los hitos y sus criterios de validación.

## Desarrollo

Para trabajar desde Windows, sigue la [guía de VS Code y conexión del teléfono por ADB](docs/WINDOWS_VSCODE.md). Incluye las herramientas iniciales, el trabajo local con Codex y tareas para consultar el modelo, la versión de Android y las arquitecturas del dispositivo.

Para compilar las sondas y probarlas desde PowerShell:

```powershell
wsl -d Ubuntu -- bash tools/android/build_native.sh
python -B tools/android/test_device.py
```

La [guía Android](docs/ANDROID_NATIVE.md) incluye la preparación del NDK. Estas sondas no son un APK ni ejecutan el juego.
