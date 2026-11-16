# ROM europea y evaluación de Moonfall Curse

Auditoría local realizada el 16 de noviembre de 2026. La ROM de `roms/` es compatible con los **tres componentes** de la receta oficial `mm3d-eur-local-6eec27b1`: código descomprimido, ExHeader completo y RomFS. Sus tamaños y SHA-256 coinciden exactamente.

Referencia fijada: [TriAevum `672d13f15cb336bc7651c861d23cf45924a9f936`](https://github.com/coccofresco/TriAevum/tree/672d13f15cb336bc7651c861d23cf45924a9f936), punta de `main` consultada en esta auditoría, commit del 12 de noviembre de 2026. Es una referencia de evaluación, no una nueva dependencia integrada.

## Identidad y cifrado

| Comprobación | Resultado observado |
| --- | --- |
| Contenedor | NCSD (`.3ds`), 1.073.741.824 bytes / 1 GiB |
| Producto | `CTR-P-AJRP`, edición europea |
| Program ID | `0004000000125600` |
| NCCH de la aplicación | Versión 2; partición 0 desde `0x4000`, 668.647.424 bytes |
| ExHeader | Nombre `Joker`, campo remaster version `2` |
| Revisión del código | Marcador `joker-1_1_0`, build 1.1.0, compatible con la copia identificada como Europe Rev 1 |
| Región SMDH | Máscara `0x0C`: Europa y Australia |
| Cifrado | `NoCrypto` activado (`flags[7] & 0x04`) en las cuatro particiones presentes: 0, 1, 6 y 7 |
| Lectura sin claves | ExHeader, ExeFS y RomFS de la aplicación legibles; RomFS comienza con `IVFC` |
| Compresión | `.code` usa BLZ/LZSS; se descomprimió en memoria para calcular su identidad |

El nombre del archivo no se utiliza como prueba de región o revisión. Los identificadores, el marcador de build y los hashes sustentan la conclusión. `NCCH version 2` y el campo remaster `2` no se interpretan como la revisión comercial del cartucho. Compresión y cifrado son comprobaciones distintas.

El [documento del título](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/docs/MOONFALL_CURSE.md) identifica precisamente AJRP, ese Program ID y el código 1.1.0. El SMDH se interpretó siguiendo el [lector de pyctr](https://github.com/ihaveamac/pyctr/blob/26e302b92c0e1645515a784a971bb557891186ee/pyctr/type/smdh.py).

## Hashes admitidos y observados

Fuente de aceptación: [receta oficial de Forge](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/titles/mm3d/forge/supported_revisions.json). Todos los valores de esta tabla son iguales en la ROM local y en la receta.

| Componente | Bytes | SHA-256 | Resultado |
| --- | ---: | --- | --- |
| `code.bin` descomprimido | 5.967.872 | `6eec27b1827eab2c87744ac38066d569ffe6fe75790b1eb3300bd91f5099038d` | Coincide |
| ExHeader completo, incluido descriptor de acceso | 2.048 | `d14ad800ecb69d746c1135c6fd2828cca7dc5735f4036d1501567ca00ce05b11` | Coincide |
| RomFS, región completa indicada por NCCH | 664.428.544 | `dcc0d6eacc513cea7a8d8c0568ba0874ee3942579b4a54c9782856da4170e75f` | Coincide |

Identidades adicionales calculadas para poder reconocer este archivo:

| Alcance | SHA-256 |
| --- | --- |
| Archivo `.3ds` completo | `4b3274a70097f956df5394850f78bbfc1a688641d84fbdd7f0afe3ea38ae573d` |
| Partición 0 completa | `a26108ef34b49df421cca20eda1b4f38f596ebef9ac6b9c70a64245be055a722` |
| `.code` comprimido en ExeFS | `cc25b988602785fb58cd59db4c849cb077574f20999abe817792fc991a486566` |

La receta consultada compara los tres componentes de la primera tabla; no publica un hash de aceptación del archivo `.3ds` completo. El relleno y las otras particiones del cartucho no deben confundirse con esos componentes.

Además, coinciden los hashes internos del ExHeader, del superbloque ExeFS, del superbloque RomFS y de las tres entradas ExeFS (`.code`, `banner`, `icon`). No se verificaron firmas RSA ni todo el árbol IVFC por bloques. Sí se calculó el hash de **todo** el RomFS y coincide con el esperado por Forge.

## Método reproducible y archivos privados

Se guardaron copias locales del [lector oficial `ctr_rom.py`](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/tools/triaevum_release/ctr_rom.py) y de la receta, con la revisión anterior, en `local/rom-audit/`. El script de auditoría llama a su lector y a su descompresor BLZ, calcula los hashes por lectura y conserva únicamente metadatos en `local/rom-audit/result.json`. No necesita claves ni ejecutar el juego.

Para repetir esta auditoría en este equipo, desde la raíz:

```powershell
python -B local/rom-audit/inspect_rom.py
```

Estos archivos de auditoría son locales y quedan excluidos de Git. El extractor ya presente en `tools/extract_mm3d_rom.py` es otra implementación: verifica identificadores y el código; su opción `--verify-only` no compara los tres componentes completos. La comparación de los tres se acredita mediante esta auditoría.

El desplazamiento `+2` del descompresor oficial es equivalente al `+3` del extractor local porque el primero calcula la dirección de origen **antes** de decrementar el índice de destino y el segundo la calcula **después**. El lector oficial produjo el SHA-256 esperado. No hay evidencia de una incompatibilidad de Forge por esa diferencia de expresión.

Protección comprobada:

- `.gitignore` excluye `/roms/`, `/local/`, `/extracted/`, `/private/` y `/generated/`, además de los formatos 3DS y archivos habituales de extracción.
- `git check-ignore -v` confirma que la ROM y la auditoría están excluidas.
- El índice de Git no contiene archivos de esas carpetas.
- Los objetos alcanzables desde las referencias locales de Git contienen únicamente los pequeños archivos de configuración y documentación del historial; no contienen la ROM ni recursos del juego.

La auditoría no efectuó cargas ni publicaciones. Esta comprobación cubre el índice y el historial local disponible; `.gitignore` puede eludirse con `git add -f` y no acredita el contenido de otras copias o ramas remotas no descargadas.

## Evaluación para Android

**Moonfall Curse es una base pertinente para un prototipo Android con esta ROM.** El árbol upstream actual ya incluye una ruta específica para MM3D; resulta preferible evaluarla directamente antes de adaptar el host de MM3D-PC y sus requisitos USA. Esta es una recomendación técnica basada en el código y la documentación consultados.

| Área | Evidencia en la revisión fijada | Implicación |
| --- | --- | --- |
| Título MM3D | `titles/mm3d/android.json`, flavor `mc`, aplicación `org.triaevum.moonfallcurse` | Existe configuración Android del título europeo |
| Módulo nativo | `AndroidTitle.cmake` admite `mm3d` y valida el título del manifiesto | Requiere el módulo recompilado para ARM64 y fuentes traducidas de la revisión correspondiente |
| Runtime | Pipeline whole-AOT, servicios CTR y renderizador PICA/Vulkan/NRI compartidos | Base reutilizable; debe compilarse y probarse en el dispositivo |
| Importación | Forge Rust para Android con feature `title-mm3d` y selector de documentos | Preparación local del dump, incluidas recetas de DekUI/Project Restoration |
| Herramientas | NDK r29 `29.0.14206865`, CMake >= 3.30, Ninja, Java 17; build nativo en Linux | WSL2 es la ruta de trabajo propuesta desde Windows |
| Plataforma | `arm64-v8a`, `minSdk 29`, `compileSdk/targetSdk 35`, bibliotecas alineadas a 16 KiB | Android 10 es el mínimo de configuración; no demuestra compatibilidad de GPU |
| Estado de lanzamiento | Documento Android: en preparación, sin APK de lanzamiento compilado o probado en dispositivo | La existencia del flavor no acredita un port jugable de MM3D |

Fuentes: [base Android](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/ports/android/README.md), [Gradle](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/ports/android/app/build.gradle.kts), [selector del módulo](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/ports/android/native/AndroidTitle.cmake), [hechos de MM3D](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/titles/mm3d/android.json) y [estado y proceso de lanzamiento](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/docs/TRIAEVUM_ANDROID_RELEASE.md).

El protocolo Android upstream todavía deja pendientes importación, arranque, controles, ciclo de vida y rendimiento de Moonfall Curse. Las mediciones de introducción del APK de desarrollo sobre Adreno 830 no validan MM3D en nuestro teléfono. Para el moto g24 power/Mali-G52/Vulkan 1.1 registrado en `TECHNICAL_BASE.md`, el siguiente criterio útil es ejecutar las sondas de capacidades y presentación real; la coincidencia de hashes no resuelve la compatibilidad gráfica ni el rendimiento.

Al integrar código, conservar las licencias y atribuciones: las contribuciones originales son GPL-3.0-or-later, con avisos separados por componente. El [alcance de licencia upstream](https://github.com/coccofresco/TriAevum/blob/672d13f15cb336bc7651c861d23cf45924a9f936/LICENSE_SCOPE.md) establece que el permiso del módulo iMMERSE/MXAO es exclusivo de TriAevum y no pasa a forks: esta integración debe omitirlo y usar `THREE_DS_RECOMP_ENABLE_IMMERSE=OFF` salvo permiso propio. Los módulos y recursos derivados de nuestra ROM permanecen en las carpetas privadas del proyecto.

La auditoría valida la entrada del juego y documenta una base candidata. Quedan por reproducir la compilación ARM64, la preparación específica de MM3D y el arranque con imagen, audio y controles en el dispositivo.
