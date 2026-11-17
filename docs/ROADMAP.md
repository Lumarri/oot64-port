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
- [ ] Adaptar PICA a GLES 3.2 y compilar el runtime Android completo para este teléfono.
- [ ] Verificar la presentación del renderizador PICA en el teléfono.

Criterio de cierre: base seleccionada con compilación reproducible y evidencia de funcionamiento de sus componentes en el dispositivo de prueba.

## 2. Preparación local del juego

- [x] Fijar la región y revisión de Majora's Mask 3D admitidas.
- [x] Verificar la copia local mediante los identificadores y hashes del extractor elegido.
- [x] Documentar extracción local de ExeFS/RomFS.
- [ ] Generar el código recompilado del juego.
- [ ] Compilar el módulo del juego para Android ARM64.
- [x] Mantener ROM, recursos y código derivado del juego fuera de Git.

Criterio de cierre: módulo de Majora's Mask 3D y recursos locales compatibles con la misma revisión del runtime.

## 3. Primer arranque en Android

- [ ] Adaptar el dialecto GLSL y las operaciones PICA de la ruta OpenGL a GLES 3.2.
- [ ] Integrar el host Android con la ruta GLES, sin forzar Vulkan/NRI.
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

| Decisión | Estado |
| --- | --- |
| Juego | Majora's Mask 3D, versión Nintendo 3DS |
| Destino | Android ARM64 |
| Equipo de desarrollo | Windows con VS Code; Linux/WSL2 propuesto para compilación nativa |
| Ruta de trabajo | Recompilación nativa |
| Base de trabajo | TriAevum/Moonfall Curse `672d13f`, fijada en `config/android-base.json`; fuentes privadas en WSL e integración mediante scripts |
| Licencias al integrar | GPL-3.0-or-later (contribuciones originales) y avisos de cada componente; sin ROM ni código del juego |
| Región y revisión | Europa Rev 1, CTR-P-AJRP, build 1.1.0, descifrada; extraída en `extracted/mm3d-eur-rev1/` |
| Teléfono de prueba | moto g24 power, Android 14 (API 34), `arm64-v8a`, Mali-G52 MC2, Vulkan 1.1 nivel 1 |
| Versión mínima de Android | API 29 en el prototipo; el teléfono cumple API 34 |
| Renderizador para el dispositivo objetivo | Adaptación GLES 3.2 pendiente: el Vulkan/NRI upstream requiere 1.2 y synchronization2, ausentes en el moto |
| APK propio y pruebas de juego | Pendientes |
