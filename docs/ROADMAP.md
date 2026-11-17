# Plan de desarrollo de Majora's Mask 3D para Android

## 0. Preparación

- [x] Definir Majora's Mask 3D de Nintendo 3DS como juego de origen y Android como destino.
- [x] Documentar el alcance y el estado inicial.
- [x] Excluir juegos de 3DS, recursos locales y resultados de compilación del control de versiones.
- [x] Preparar la guía de Windows y las tareas de VS Code para comprobar el teléfono por ADB.

## 1. Evaluación de la base nativa

- [x] Revisar MM3D-PC, TriAevum y su base experimental para Android.
- [x] Registrar fuentes y límites de la evaluación en [TECHNICAL_BASE.md](TECHNICAL_BASE.md).
- [x] Confirmar la base que se integrará, sus licencias y las atribuciones necesarias.
- [x] Fijar la revisión exacta y las dependencias de la base adoptada.
- [x] Obtener modelo, versión de Android, arquitectura y capacidades gráficas del teléfono de prueba.
- [x] Compilar los servicios, soporte ARM64 y sondas de la base desde WSL2.
- [x] Ejecutar las siete pruebas de servicios/cargador en el moto g24 power.
- [x] Comprobar la incompatibilidad de Vulkan/NRI upstream y validar dibujo/compute sintéticos en GLES 3.2.
- [x] Validar el dialecto GLES inicial con shaders sintéticos, texturas, uniforms, profundidad y recorte en Mesa y en la Mali-G52 del moto.
- [x] Compilar/enlazar las seis variantes base de fragmentos PICA generadas por upstream en GLES y verificar un dibujo TEV en el moto.
- [x] Implementar y validar las 16 operaciones lógicas RGBA8 mediante framebuffer fetch, con máscaras de escritura, en Mesa y en el moto.
- [x] Conectar las operaciones lógicas al fragment shader PICA generado y validar descarte por alfa y profundidad explícita en el moto.
- [x] Implementar superficies GLES RGBA8/depth24/stencil8 y validar reutilización, cambio de tamaño, composición y ejecución PICA en el moto.
- [x] Implementar un consumidor inicial de `PicaDrawView`: geometría, índices, UBO, caché de shaders y estados de rasterizado, validado en Mesa y en el moto.
- [x] Cargar texturas tiled con el decodificador upstream y validar un dibujo indexado con textura y fragment shader TEV generado en el moto.
- [x] Implementar el adaptador inicial `PicaRenderBackend` con superficies por dirección física, borrados, fills y finalizaciones; compilar ARM64 y verificarlo en Mesa.
- [x] Validar ese adaptador en el moto, incluidos superficies, fills y finalizaciones (informe `device-20261117T204855650869Z.json`).
- [x] Implementar transferencias de imagen RGBA8 y presentación GLES con rotación, escalado, bordes y transparencia; verificar píxeles en Mesa y en el moto, y compilar ARM64.
- [ ] Adaptar PICA a GLES 3.2 y compilar el runtime Android completo para este teléfono.
- [ ] Verificar la presentación del renderizador PICA en el teléfono.

Criterio de cierre: base seleccionada con compilación reproducible y evidencia de funcionamiento de sus componentes en el dispositivo de prueba.

## 2. Preparación local del juego

- [x] Fijar la región y revisión de Majora's Mask 3D admitidas.
- [x] Verificar la copia local mediante los identificadores y hashes del extractor elegido.
- [x] Documentar extracción local de ExeFS/RomFS.
- [x] Generar el código recompilado del juego: 259 archivos C++ privados, 18 751 funciones y auditoría estructural sin bloques sin identificar.
- [x] Compilar el módulo privado del juego para Android ARM64 con NDK r29 y alineación LOAD de 16 KiB.
- [x] Cargar el módulo real mediante el cargador upstream en el moto y validar ABI V2 y 241 681 entradas, sin ejecutar el juego.
- [x] Ejecutar el inicio real desde `0x100000` hasta el primer SVC en el moto, con presupuesto por bloques y comparación de estado tras reanudar.
- [x] Mantener ROM, recursos y código derivado del juego fuera de Git.

Criterio de cierre: módulo de Majora's Mask 3D y recursos locales compatibles con la misma revisión del runtime.

## 3. Primer arranque en Android

- [ ] Adaptar el dialecto GLSL y las operaciones PICA de la ruta OpenGL a GLES 3.2.
- [ ] Integrar el host Android con la ruta GLES, sin forzar Vulkan/NRI.
- [x] Crear e instalar un APK técnico NativeActivity con presentación GLES visible y datos sintéticos, probado en el moto.
- [x] Comprobar pausa/regreso y recreación de contexto al girar ese APK en el moto.
- [ ] Adaptar el arranque y el cargador Android al módulo de Majora's Mask 3D.
- [ ] Integrar selección de archivos con el selector de documentos de Android y almacenamiento de la aplicación.
- [ ] Preparar un APK de desarrollo y comprobar bibliotecas, ABI y carga del módulo.
- [ ] Validar pantalla inicial, vídeo y audio en un teléfono real.
- [ ] Documentar instalación, registros y problemas conocidos.

Criterio de cierre: arranque reproducible de Majora's Mask 3D con imagen y sonido en el dispositivo. La compilación del APK por sí sola no acredita este hito.

## 4. Experiencia de juego

- [ ] Adaptar las funciones de las dos pantallas de 3DS a la interfaz del teléfono.
- [ ] Integrar controles táctiles, cámara y mandos.
- [ ] Validar inventario, máscaras, mapa, cuaderno y ocarina.
- [ ] Comprobar guardado, carga y ciclo temporal del juego.
- [ ] Comprobar pausa, suspensión, reanudación y orientación de pantalla.
- [ ] Medir estabilidad, rendimiento y temperatura en el teléfono de prueba.

Criterio de cierre: recorrido de juego reproducible con interfaz, controles, sonido y guardado funcionales.

## 5. Compilaciones reproducibles

- [ ] Documentar SDK, NDK, CMake, dependencias y configuración exactos.
- [ ] Automatizar las comprobaciones que no necesitan archivos del juego.
- [ ] Documentar la preparación local de la aplicación con los archivos aportados por el usuario.
- [ ] Conservar licencias y atribuciones en los componentes que se distribuyan.

## Decisiones y límites actuales

### Fases de la continuación

1. **Renderizador GLES — en curso.** Ya se verifican superficies, dibujos y
   texturas con datos sintéticos usando el contrato upstream. Falta conectar el
   adaptador al despachador del runtime, ampliar los display transfers y resolver estados todavía rechazados:
   LUT de iluminación, sombras, mipmaps, formatos de destino y referencias a
   copias GPU. El siguiente paso es esa integración y su gestión de memoria.
2. **Presentación Android — en curso.** El APK técnico ya presenta una textura
   TEV animada en una superficie real y pasa pausa/reanudación y rotación.
   Faltan integración del runtime, composición de ambas pantallas y estado del
   juego durante la recreación. Cierre:
   dibujos presentados en una Activity del moto, con ciclo de vida probado.
3. **Módulo MM3D y primer arranque — en curso.** La receta EUR Rev 1 ya está
   generada y compilada para ARM64; su inicio ejecuta hasta el primer SVC en el
   moto y conserva el estado al reanudar. Falta conectar los servicios CTR, el runtime, los archivos y el
   backend GLES en el APK de desarrollo.
   Cierre: arranque reproducible con imagen y sonido del juego en este teléfono.
4. **Juego y rendimiento — pendiente.** Controles, guardado, recorrido jugable y
   mediciones de tiempo de cuadro, memoria y temperatura en el moto.

La fase gráfica tiene sondas fuera de pantalla y un APK de prueba visible; aún
no hay APK jugable. Los datos sintéticos no contienen material de la ROM.

| Decisión | Estado |
| --- | --- |
| Juego | Majora's Mask 3D, versión Nintendo 3DS |
| Destino | Android ARM64 |
| Equipo de desarrollo | Windows con VS Code y Ubuntu/WSL2 para compilación nativa |
| Ruta de trabajo | Recompilación nativa |
| Base de trabajo | TriAevum/Moonfall Curse `672d13f`, fijada en `config/android-base.json`; fuentes privadas en WSL e integración mediante scripts |
| Licencias al integrar | GPL-3.0-or-later (contribuciones originales) y avisos de cada componente; sin ROM ni código del juego |
| Región y revisión | Europa Rev 1, CTR-P-AJRP, build 1.1.0, descifrada; extraída en `extracted/mm3d-eur-rev1/` |
| Teléfono de prueba | moto g24 power, Android 14 (API 34), `arm64-v8a`, Mali-G52 MC2, Vulkan 1.1 nivel 1 |
| Versión mínima de Android | API 29 en el prototipo; el teléfono cumple API 34 |
| Renderizador para el dispositivo objetivo | Adaptación GLES 3.2 pendiente: el Vulkan/NRI upstream requiere 1.2 y synchronization2, ausentes en el moto |
| APK propio y pruebas de juego | APK gráfico sintético instalado y probado; arranque y pruebas de juego pendientes |
