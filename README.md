# Ocarina of Time — Android Port

Repositorio inicial para desarrollar un port de **The Legend of Zelda: Ocarina of Time** de Nintendo 64 a **Android**.

## Estado

Planificación y preparación del proyecto. Todavía no hay motor integrado, aplicación Android ni APK jugable.

## Objetivo

Ejecutar el juego mediante un port nativo en Android, con controles táctiles, soporte para mandos y partidas guardadas en el dispositivo. La primera arquitectura propuesta es ARM64 (`arm64-v8a`).

## Siguiente paso

Elegir una base técnica mantenida y revisar su soporte para Android, sus licencias y las versiones de ROM compatibles. Antes de fijar versiones del SDK/NDK, API mínima o backend gráfico, comprobaremos los requisitos de esa base y del dispositivo de prueba.

Referencias para evaluar:

- [Ship of Harkinian / Shipwright](https://github.com/HarbourMasters/Shipwright): proyecto de port existente.
- [Shipwright-Android](https://github.com/Waterdish/Shipwright-Android): adaptación para Android que estamos evaluando.
- [zeldaret/oot](https://github.com/zeldaret/oot): proyecto de decompilación de Ocarina of Time.

Estas referencias todavía no se han incorporado como dependencias. El código y las licencias de terceros se conservarán cuando se seleccione e integre la base.

## ROM y recursos

El repositorio contiene documentación y, posteriormente, código y herramientas del proyecto. La ROM aportada por el usuario y los recursos extraídos se mantienen localmente, fuera del control de versiones.

Las carpetas `local/`, `roms/` y `extracted/`, así como los formatos habituales de ROM de N64 y los paquetes de recursos, están excluidos mediante `.gitignore`. No hace falta aportar una ROM para preparar el repositorio. La identificación y extracción se definirán después de elegir la base técnica.

## Plan de trabajo

Consulta [docs/ROADMAP.md](docs/ROADMAP.md) para los hitos y sus criterios de validación.

## Desarrollo

Para trabajar desde Windows, sigue la [guía de VS Code y conexión del teléfono por ADB](docs/WINDOWS_VSCODE.md). Incluye las herramientas iniciales, el trabajo local con Codex y tareas para consultar el modelo, la versión de Android y las arquitecturas del dispositivo.

Todavía no hay un comando de compilación del juego. Las instrucciones reproducibles se añadirán junto con la primera integración del motor.
