# Base técnica para Majora's Mask 3D en Android

Revisión documental y de configuración realizada el 16 de noviembre de 2026. Esta evaluación no incluye compilación ni pruebas de juego propias.

## Candidata principal: MM3D-PC / TriAevum

[MM3D-PC](https://github.com/Volpe34/MM3D-PC) documenta un port de Majora's Mask 3D para Windows/Linux basado en el runtime de recompilación de [TriAevum](https://github.com/coccofresco/TriAevum). La preparación local extrae una copia del juego, traduce su código ARM/Thumb a C++ y compila un módulo que utiliza el runtime y el renderizador Vulkan.

Revisión de MM3D-PC consultada: `fc4d8f6d9b85afa928b05a31147246775fab9cf8`. Se registra como referencia de investigación; todavía no es una dependencia del repositorio.

Fuentes consultadas en esa revisión:

- [README y requisitos de MM3D-PC](https://github.com/Volpe34/MM3D-PC/blob/fc4d8f6d9b85afa928b05a31147246775fab9cf8/README.md): Windows/Linux x64, Vulkan 1.2 y copia descifrada de Majora's Mask 3D USA v1.1 en `.3ds` o `.cci`. El autor señala problemas de rendimiento y mantenimiento futuro incierto.
- [Base Android](https://github.com/Volpe34/MM3D-PC/blob/fc4d8f6d9b85afa928b05a31147246775fab9cf8/ports/android/README.md): trabajo experimental para ARM64, Vulkan/NRI y host Android; no se presenta como una versión Android soportada.
- [Configuración de la aplicación](https://github.com/Volpe34/MM3D-PC/blob/fc4d8f6d9b85afa928b05a31147246775fab9cf8/ports/android/app/build.gradle.kts): ABI `arm64-v8a`, `minSdk 29`, `compileSdk 35`, `targetSdk 35` y Java 17.
- [Pruebas de arranque heredadas](https://github.com/Volpe34/MM3D-PC/blob/fc4d8f6d9b85afa928b05a31147246775fab9cf8/ports/android/INTRO.md): resultados experimentales del título/runtime de TriAevum. No acreditan por sí solos un port Android jugable de Majora's Mask 3D.
- [Alcance de licencias](https://github.com/Volpe34/MM3D-PC/blob/fc4d8f6d9b85afa928b05a31147246775fab9cf8/LICENSE_SCOPE.md): contribuciones originales GPL-3.0-or-later y avisos propios de cada componente. La revisión de dependencias y atribuciones precederá a su integración.

## Qué falta comprobar

1. Que el módulo recompilado de **Majora's Mask 3D** se pueda generar y enlazar para Android ARM64 con el cargador de esa revisión.
2. Qué partes del host Android heredado necesitan adaptación al título, sus archivos y su interfaz.
3. Que Vulkan/NRI funcione en el teléfono concreto, incluyendo creación de la superficie y presentación real de imágenes.
4. Importación local, acceso a archivos, audio, controles, guardado y ciclo de vida de Android.
5. Rendimiento sostenido y estabilidad durante juego real.

La base Android describe NDK r29 (`29.0.14206865`), CMake 3.30 o posterior, Ninja y un equipo Linux de compilación. En Windows se propone WSL2 para esa fase, manteniendo ADB en Windows para el teléfono USB. Estas versiones son referencias del prototipo existente; se fijarán para este proyecto al reproducir la compilación.

## Otras referencias y su alcance

| Proyecto | Lo que aporta | Relación con este objetivo |
| --- | --- | --- |
| [TriAevum / Moonfall Curse](https://github.com/coccofresco/TriAevum) | Otra recompilación nativa de MM3D sobre el mismo motor | Su documentación consultada exige la edición europea CTR-P-AJRP y anuncia versiones de PC; sus requisitos no se deben mezclar con los de MM3D-PC. |
| [Majora ReMasked / DekUI-Fork](https://github.com/OTPR26/DekUI-Fork) | Adaptación de MM3D con soporte Android y controles | Declara que utiliza un emulador 3DS adaptado. Es una alternativa si se cambia el enfoque; no es la base nativa adoptada. |
| [Project Restoration](https://github.com/leoetlino/project-restoration) | Parches y documentación de estructuras y mecánicas de MM3D | Sirve como referencia de modificaciones; no constituye por sí solo un runtime Android. |

## Primer resultado verificable

Reproducir una compilación ARM64 de los componentes del runtime, ejecutar sus pruebas en el teléfono y comprobar el cargador. Después, integrar el módulo específico de MM3D y demostrar arranque con imagen y sonido. No se atribuirá compatibilidad o una tasa de imágenes a este proyecto hasta medirla en el dispositivo.
