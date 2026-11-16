# Majora's Mask 3D — Android Port

Proyecto para desarrollar un port nativo de **The Legend of Zelda: Majora's Mask 3D**, la versión de **Nintendo 3DS**, a **Android**.

El repositorio se aloja actualmente en [`Lumarri/oot64-port`](https://github.com/Lumarri/oot64-port).

## Estado

Evaluación técnica y preparación del entorno. Hay una guía para Windows/VS Code y tareas de ADB. Todavía no hay runtime integrado, compilación Android propia ni APK jugable.

## Objetivo

Ejecutar Majora's Mask 3D en Android ARM64 (`arm64-v8a`), con controles táctiles, soporte para mandos, una interfaz adaptada al teléfono y partidas guardadas en el dispositivo.

La ruta de trabajo es la recompilación nativa: traducir localmente el código del juego y adaptarlo a un runtime Android. Los recursos del juego se obtienen de la copia local del usuario.

## Siguiente paso

Evaluar [MM3D-PC](https://github.com/Volpe34/MM3D-PC), basado en [TriAevum](https://github.com/coccofresco/TriAevum), como punto de partida. Su versión de escritorio documenta recompilación de Majora's Mask 3D y su árbol incluye una base Android experimental que requiere adaptación y pruebas específicas para este juego.

La revisión de fuentes, los requisitos observados y las comprobaciones pendientes están en [docs/TECHNICAL_BASE.md](docs/TECHNICAL_BASE.md).

Todavía no se han incorporado dependencias de esos proyectos. Antes de integrar una base, fijaremos su revisión y conservaremos sus licencias y atribuciones.

## ROM y recursos

La base MM3D-PC revisada requiere una copia local de **Majora's Mask 3D USA v1.1**, descifrada, en formato `.3ds` o `.cci`. Otros proyectos pueden exigir otra región: la versión admitida se fijará junto con la base adoptada y se verificará por contenido.

La ROM, los recursos extraídos y el código generado a partir del juego se mantienen localmente, fuera del control de versiones. `.gitignore` excluye las carpetas `local/`, `roms/`, `extracted/`, `private/` y `generated/`, los formatos de juego de 3DS y los archivos de extracción habituales. Guarda estos archivos en tu PC; no hace falta subirlos para preparar el entorno.

## Plan de trabajo

Consulta [docs/ROADMAP.md](docs/ROADMAP.md) para los hitos y sus criterios de validación.

## Desarrollo

Para trabajar desde Windows, sigue la [guía de VS Code y conexión del teléfono por ADB](docs/WINDOWS_VSCODE.md). Incluye las herramientas iniciales, el trabajo local con Codex y tareas para consultar el modelo, la versión de Android y las arquitecturas del dispositivo.

Todavía no hay un comando de compilación de este port. Las instrucciones reproducibles se añadirán junto con la primera integración del runtime.
