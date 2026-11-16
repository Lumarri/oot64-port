# Base técnica para Majora's Mask 3D en Android

Revisión documental y de configuración realizada el 16 de noviembre de 2026. Esta evaluación no incluye compilación ni pruebas de juego propias.

Actualización de la auditoría: [ROM_VERIFICATION.md](ROM_VERIFICATION.md) confirma los tres hashes oficiales de la ROM europea y evalúa TriAevum `672d13f` como alternativa directa al host MM3D-PC. Esa revisión ya contiene flavor Android `mc`, selector de título `mm3d` y Forge Android específico, aunque su lanzamiento sigue pendiente de validación en dispositivo.

## Base seleccionada y restricción del dispositivo

La implementación inicial adopta TriAevum/Moonfall Curse `672d13f15cb336bc7651c861d23cf45924a9f936` directamente, según [config/android-base.json](../config/android-base.json). El usuario exige compatibilidad con el moto g24 power. Su informe Vulkan real anuncia 1.1.177 y carece de synchronization2; el selector upstream exige 1.2. Se abre una adaptación gráfica GLES 3.2, todavía sin integración PICA completa. La preparación, las sondas y los resultados actuales están en [ANDROID_NATIVE.md](ANDROID_NATIVE.md).

## Evaluación anterior: MM3D-PC / TriAevum

[MM3D-PC](https://github.com/Volpe34/MM3D-PC) documenta un port de Majora's Mask 3D para Windows/Linux basado en el runtime de recompilación de [TriAevum](https://github.com/coccofresco/TriAevum). La preparación local extrae una copia del juego, traduce su código ARM/Thumb a C++ y compila un módulo que utiliza el runtime y el renderizador Vulkan.

Revisión de MM3D-PC adoptada como clon de trabajo: `fc4d8f6d9b85afa928b05a31147246775fab9cf8` (`v0.1.9`), en `local/MM3D-PC/` (excluido de Git). No es aún un submódulo ni una dependencia publicada de este repositorio.

Propuesta inicial, sustituida por la base directa fijada arriba:

- **Título / dump:** Moonfall Curse, cartucho europeo CTR-P-AJRP Rev 1 / 1.1.0, ya extraído.
- **Host Android:** `ports/android` de MM3D-PC en esa revisión (ABI `arm64-v8a`, `minSdk 29`).
- **Licencia al copiar código:** GPL-3.0-or-later para las contribuciones originales, más los avisos de Azahar, NRI y el resto de componentes. ROM, `code.bin`, RomFS y C++ traducido del juego no se publican.
- **Adaptación obligatoria:** MM3D-PC documenta USA v1.1 (inglés/francés/español). Hay que aceptar AJRP y el RomFS europeo (más idiomas) en el cargador.

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

La base Android describe NDK r29 (`29.0.14206865`), CMake 3.30 o posterior, Ninja y un equipo Linux de compilación. Se instalaron NDK r29, CMake 4.2.3 y Ninja 1.13.2 en WSL2 Ubuntu. ADB permanece en Windows para el USB; las fuentes de compilación y objetos se mantienen en el sistema de archivos Linux.

## Copia local verificada (Europa Rev 1)

Comprobación del 16 de noviembre de 2026 sobre el `.3ds` en `roms/` (excluido de Git). Forge y Moonfall leen el contenido, no el nombre del archivo.

| Campo | Valor observado | Moonfall Curse |
| --- | --- | --- |
| Producto | `CTR-P-AJRP` | Edición europea, único cartucho admitido |
| Program ID | `0004000000125600` | `0004000000125600` |
| NCCH | versión 2, nombre ExHeader `Joker`, código comprimido | Igual |
| Cifrado | `NoCrypto` (flags NCCH `…04`), ExeFS y RomFS IVFC legibles | Dump descifrado |
| Región SMDH | Europa y Australia | EUR |
| Build | `joker-1_1_0` en el `code.bin` | 1.1.0 |
| SHA-256 de `code.bin` descomprimido | `6eec27b1827eab2c87744ac38066d569ffe6fe75790b1eb3300bd91f5099038d` | El hash publicado |
| CRC32 de ese `code.bin` | `ffdf1ee8` | El que dekui cita como origen del parche v1.1 |
| Text / rodata / data | `0x00100000` (`0x51CFB4`), `0x0061D000`, `0x0064B000`; BSS hasta `0x007CF000` (alineado a página) | Igual |

El cartucho USA v1.1 (`CTR-P-AJRE`, `0004000000125500`) no está en `roms/`. Moonfall indica que su `code.bin` es idéntico al europeo y que la región vive en RomFS y en el program ID. MM3D-PC y dekui piden ese dump USA; esta copia **no** cumple ese requisito de producto, aunque el código coincida.

Consecuencia para Android: esta ROM es la entrada correcta si el port sigue Moonfall Curse / el adaptador `mm3d` de TriAevum. No cumple el requisito USA documentado por MM3D-PC. TriAevum `672d13f` ya contiene una ruta Android específica para Moonfall Curse, con flavor `mc` y soporte de título `mm3d`; su documento de lanzamiento aún marca los APK como no validados. Véase la [evaluación fijada por revisión](ROM_VERIFICATION.md#evaluación-para-android).

El extractor de este repo (`tools/extract_mm3d_rom.py`) verifica identificadores y hashes del código; la auditoría independiente de [ROM_VERIFICATION.md](ROM_VERIFICATION.md) compara además ExHeader y RomFS completos con la receta oficial. Salida local existente, fuera de Git: `extracted/mm3d-eur-rev1/` (`exheader.bin`, `code.bin`, `romfs.bin`, `manifest.json`). SHA-256 de ExHeader `d14ad800ecb69d746c1135c6fd2828cca7dc5735f4036d1501567ca00ce05b11`; RomFS `dcc0d6eacc513cea7a8d8c0568ba0874ee3942579b4a54c9782856da4170e75f`.

El `ctr_rom.py` oficial de Forge produce el mismo código y el SHA-256 esperado. Su desplazamiento `+2` calcula el origen antes de decrementar el destino; el `+3` del extractor local lo calcula después. Ambas expresiones son equivalentes y no justifican descartar el descompresor oficial.

## Teléfono de prueba

Conectado por ADB el 16 de noviembre de 2026:

| Campo | Valor |
| --- | --- |
| Modelo | moto g24 power (`fogorow`) |
| Android | 14 (API 34) |
| ABI | `arm64-v8a` (también lista `armeabi-v7a`) |
| SoC | `mt6768` (Helio G85) |
| GPU | Mali-G52 MC2, OpenGL ES 3.2 |
| Vulkan | `android.hardware.vulkan.level=1`, versión 1.1 (`4198400`); `libvulkan.so` presente |

El teléfono cumple ABI y API. La auditoría directa del controlador confirmó Vulkan 1.1.177, create_renderpass2 presente y synchronization2 ausente. La política de selección del renderizador upstream exige 1.2 también en Android: este teléfono necesita la adaptación gráfica descrita en [ANDROID_NATIVE.md](ANDROID_NATIVE.md).

## Otras referencias y su alcance

| Proyecto | Lo que aporta | Relación con este objetivo |
| --- | --- | --- |
| [TriAevum / Moonfall Curse](https://github.com/coccofresco/TriAevum) | Recompilación nativa de MM3D; dump EUR CTR-P-AJRP | La copia local de Europa Rev 1 encaja con sus identificadores y hashes. Paquetes actuales de PC; Android no acreditado. |
| [Majora ReMasked / DekUI-Fork](https://github.com/OTPR26/DekUI-Fork) | Adaptación de MM3D con soporte Android y controles | Declara que utiliza un emulador 3DS adaptado. Es una alternativa si se cambia el enfoque; no es la base nativa adoptada. |
| [Project Restoration](https://github.com/leoetlino/project-restoration) | Parches y documentación de estructuras y mecánicas de MM3D | Sirve como referencia de modificaciones; no constituye por sí solo un runtime Android. |

## Primer resultado verificable

Reproducir una compilación ARM64 de los componentes del runtime, ejecutar sus pruebas en el teléfono y comprobar el cargador. Después, integrar el módulo específico de MM3D y demostrar arranque con imagen y sonido. No se atribuirá compatibilidad o una tasa de imágenes a este proyecto hasta medirla en el dispositivo.
