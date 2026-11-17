# Módulo privado MM3D para Android

Esta ruta prepara y compila localmente el módulo whole-AOT de **MM3D Europa Rev 1**
con TriAevum `672d13f`. El generador consume únicamente `code.bin` y ExHeader
verificados; no necesita RomFS para traducir el programa. El módulo y sus fuentes
son derivados del juego y se conservan como archivos privados, separados del
repositorio público y del APK gráfico sintético.

## Preparación y compilación

Requisitos: extracción local validada en `extracted/mm3d-eur-rev1/`, checkout
limpio y fijado en WSL, Python con Capstone (versión instalada/verificada: 5.0.9),
NDK r29 y las dependencias de la [base nativa](ANDROID_NATIVE.md). La extracción
comprueba otra vez tamaño y SHA-256 de los dos archivos contra el contrato MM3D
del checkout. No acepta los archivos de otra región/receta por su nombre.

Desde PowerShell, en la raíz:

```powershell
# Sólo IR estructural y auditoría de cobertura.
wsl -d Ubuntu -- python3 -B tools/android/prepare_title.py --phase ir

# IR auditado y fuentes C++ privadas.
wsl -d Ubuntu -- python3 -B tools/android/prepare_title.py --phase sources

# Preparar/reutilizar las fuentes y compilar módulo + comprobador del cargador.
wsl -d Ubuntu -- bash tools/android/build_title.sh

# Verificar y enviar únicamente el módulo privado y su comprobador al teléfono.
python -B tools/android/test_title.py

# Ejecutar el inicio real hasta su primer SVC, sin atender servicios CTR.
python -B tools/android/test_startup.py
```

`build_title.sh --probe-only` compila los comprobadores de carga e inicio, sin
generar ni compilar el título ni preparar archivos para el teléfono.
La compilación normal usa dos trabajos por defecto
y `-O1 -ffp-model=strict` para el código traducido. `CMAKE_BUILD_PARALLEL_LEVEL`
permite ajustar los trabajos. No se compila NRI en esta configuración aislada:
esta operación construye el módulo del título y su soporte, no el renderizador.
También prepara la prueba de inicio con el generador upstream de manifiestos
de proceso: obtiene segmentos, BSS, pila, TLS y páginas de sistema del ExHeader.
Para ese manifiesto lee el encabezado local de RomFS y su tamaño; no lo copia
al staging ni lo envía al teléfono.

Los procesos son incrementales. La caché IR identifica código, ExHeader, receta
y archivos del frontend. El generador comprueba su propia identidad, la selección
y los hashes de salida antes de reutilizar C++. La preparación tiene un bloqueo
para impedir dos generaciones simultáneas en el mismo destino. CMake verifica
los hashes del inventario C++ antes de configurar la compilación.

## Archivos locales

| Ruta | Contenido |
| --- | --- |
| `~/mm3d-android/private/mm3d-eur-672d13f/inputs/` en WSL | Copias verificadas de code.bin y ExHeader |
| `.../ir-cache/` | Programa estructural y auditoría del frontend |
| `.../translated/` | Shards C++, headers y `TITLE_SOURCE_MANIFEST.json` |
| `.../preparation.json` | Recibo de generación y hashes; no es un recibo de arranque |
| `~/mm3d-android/build/title-mm3d-672d13f/` | Objetos y biblioteca ARM64 |
| `local/android-title-stage/` | Módulo privado, comprobador, libc++ y hashes |
| `local/android-startup-stage/` | Comprobador de inicio, code.bin y manifiesto privado de memoria |
| `local/android-logs/title-build.log` | Resultado incremental de compilación |
| `local/android-logs/title-*.json` | Resultado de carga en dispositivo |
| `local/android-logs/startup-*.json` | Ejecución real hasta el primer SVC y comparación tras reanudar |

La preparación comprueba que el directorio privado también esté excluido por Git
en la copia WSL. No se publica ningún archivo de estas rutas. El empaquetador del
APK gráfico mantiene su lista permitida de bibliotecas y no incorpora este módulo.

## Qué verifica cada etapa

La extracción estructural obtuvo **18 751 funciones**, **241 681 bloques** y
**1 258 192 instrucciones únicas**, sin bloques sin identificar. La auditoría
upstream pasó contra el contrato MM3D: esto comprueba cobertura y entradas
estructurales, no que la traducción reproduzca cada escena correctamente.

El generador utiliza el contrato de 256 shards con estrategia `affinity` y escribe
el inventario de hashes que consume el CMake upstream Android. Estas fuentes son
una generación privada para nuestro prototipo; no se presentan como un artefacto
publicado o una compilación cualificada de Moonfall Curse.

El comprobador usa **el cargador real upstream** (`ConfigureTitlePlugin`), no una
implementación alternativa. Verifica tabla ABI V2, observación de escrituras,
capacidad de salida reanudable, lista ordenada/sin duplicados de entradas,
presencia de la entrada del juego, revalidación de la misma ruta y rechazo de
una selección diferente después de consultar la ABI.
No llama a las funciones del juego. Un PASS de carga no equivale a arrancar MM3D.

## Resultado del 17 de noviembre de 2026

**Compilación y carga correctas en el moto g24 power** (salida 0). Se generaron
259 archivos privados con 18 751 funciones; el módulo ARM64 ocupa 141 695 960
bytes. Los tres ELF preparados tienen segmentos LOAD alineados a 16 KiB. Se
comprobaron las dos funciones de consulta ABI exportadas antes de enviarlos al
dispositivo. Una ejecución incremental reutilizó las fuentes y objetos.

La comprobación en el teléfono confirmó ABI V2, observación de escrituras,
capacidad de solicitar salida, 241 681 entradas ordenadas y únicas, entrada
`0x100000`, revalidación de la misma biblioteca y rechazo de una selección
diferente tras consultarla. La prueba negativa del módulo vacío también fue
rechazada. El módulo se carga como proceso del usuario shell de ADB; no se ha
incorporado al APK gráfico ni se han ejecutado funciones del juego.

Evidencia privada: `local/android-logs/title-20261117T215135776549Z.json` y
`local/android-logs/title-loader-negative.json`. SHA-256 de la biblioteca probada:
`2ac59c45990a824585e6af29836bbd2a3c603a0fe84c8222c90bc2d8b4beabf9`.
El inventario de fuentes y los hashes del comprobador/libc++ están en el informe
y en `local/android-title-stage/manifest.json`.

## Ejecución inicial real en el moto

**PASS el 17 de noviembre de 2026**, salida 0. `mm3d_startup_probe` carga la misma
biblioteca whole-AOT con el cargador upstream e inicializa nueve regiones de
memoria con el layout generado: texto y rodata protegidos, datos/BSS, pila, TLS
y regiones fijas CTR. Ejecuta desde `0x00100000` hasta el primer SVC y se detiene
antes de atenderlo. No usa handlers de servicios, recursos RomFS ni GPU.

Resultado en el moto g24 power:

- **292 449 bloques** ejecutados y **292 447 escrituras** registradas.
- Primer SVC: `0x21` (`CreateAddressArbiter` en el host upstream), PC `0x0010FDCC`.
  Se comprueba que la instrucción ARM de la imagen verificada coincide con el SVC.
- Cero salidas por fallo de memoria, instrucciones no admitidas o llamadas externas.
- La ejecución con un primer presupuesto de un bloque y reanudaciones de hasta
  4096 bloques llega al mismo SVC tras 73 llamadas. Coinciden todos los registros
  arquitectónicos y la huella del contenido de la memoria con la ejecución continua.
- Límite total de dos millones de bloques y timeout remoto de 60 segundos.
  Los cinco archivos transferidos se verifican también por SHA-256 en el teléfono.

Evidencia privada: `local/android-logs/startup-20261117T220115943912Z.json`.
El ejecutable probado tiene SHA-256
`0950e969a254062119a9a326b26d2ea467e58d0c45173e4696e199aacb02a2a5`.
El módulo del juego mantiene el hash indicado en la prueba ABI anterior.
El manifiesto y code.bin también son privados. El APK gráfico no cambia.

Esta ejecución cubre solamente el inicio anterior al primer servicio CTR.
Todavía no crea ese objeto de sincronización, ejecuta el planificador del
runtime, instala los observadores y parches MM3D, ni presenta una escena.

Los dos comprobadores comparten ahora la biblioteca estática del cargador.
La compilación ARM64, la prueba de inicio y la repetición aislada de
`test_title.py` de esta versión pasaron en el moto. La comprobación ABI confirma
241 681 entradas, observación de escrituras, capacidad de salida, revalidación
de la misma ruta y rechazo de cambiar de biblioteca después de consultarla.
Evidencia privada de la repetición: `local/android-logs/title-20261117T223444175994Z.json`,
salida 0, ejecutable SHA-256
`66fbb896a091809b326e12afe237e87dbf2426518293f3978c93c83dd9080092`.

Falta conectar este módulo y los recursos locales al host, servicios CTR, puente
de planes PICA y backend GLES; también siguen pendientes audio, controles,
guardado, selector de archivos y configuración. La ROM y sus recursos no cambian
de licencia por haber sido traducidos.
