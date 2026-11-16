# Plan de desarrollo de Majora's Mask 3D para Android

## 0. Preparación

- [x] Definir Majora's Mask 3D de Nintendo 3DS como juego de origen y Android como destino.
- [x] Documentar el alcance y el estado inicial.
- [x] Excluir juegos de 3DS, recursos locales y resultados de compilación del control de versiones.
- [x] Preparar la guía de Windows y las tareas de VS Code para comprobar el teléfono por ADB.

## 1. Evaluación de la base nativa

- [x] Revisar MM3D-PC, TriAevum y su base experimental para Android.
- [x] Registrar fuentes y límites de la evaluación en [TECHNICAL_BASE.md](TECHNICAL_BASE.md).
- [ ] Confirmar la base que se integrará, sus licencias y las atribuciones necesarias.
- [ ] Fijar la revisión exacta y las dependencias de la base adoptada.
- [ ] Obtener modelo, versión de Android, arquitectura y capacidades gráficas del teléfono de prueba.
- [ ] Reproducir la compilación del runtime para ARM64 desde Linux/WSL2.
- [ ] Verificar en el teléfono las pruebas del runtime y el renderizador.

Criterio de cierre: base seleccionada con compilación reproducible y evidencia de funcionamiento de sus componentes en el dispositivo de prueba.

## 2. Preparación local del juego

- [ ] Fijar la región y revisión de Majora's Mask 3D admitidas.
- [ ] Verificar la copia local mediante los identificadores y hashes del extractor elegido.
- [ ] Documentar extracción local de ExeFS/RomFS y generación del código recompilado.
- [ ] Compilar el módulo del juego para Android ARM64.
- [ ] Mantener ROM, recursos y código derivado del juego fuera de Git.

Criterio de cierre: módulo de Majora's Mask 3D y recursos locales compatibles con la misma revisión del runtime.

## 3. Primer arranque en Android

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
| Base candidata | MM3D-PC / TriAevum; sin integrar |
| Región y revisión | MM3D-PC documenta USA v1.1; confirmar al adoptar la base |
| Versión mínima de Android | Pendiente de validación; la base experimental usa API 29 |
| Renderizador candidato | Vulkan/NRI; compatibilidad del teléfono pendiente |
| APK propio y pruebas de juego | Pendientes |
