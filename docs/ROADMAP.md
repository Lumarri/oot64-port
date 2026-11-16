# Plan de desarrollo para Android

## 0. Preparación

- [x] Definir Android como plataforma de destino.
- [x] Documentar el alcance y el estado inicial.
- [x] Excluir ROMs, recursos locales y resultados de compilación del control de versiones.

## 1. Selección de la base técnica

- [ ] Evaluar un port existente y su soporte actual para Android.
- [ ] Revisar licencias, atribuciones, dependencias y mantenimiento.
- [ ] Fijar la revisión exacta del proyecto que se integrará.
- [ ] Identificar el dispositivo de prueba y acordar la versión mínima de Android.
- [ ] Documentar SDK, NDK, ABI y backend gráfico requeridos.

Criterio de cierre: base elegida con requisitos y proceso de compilación documentados.

## 2. Importación local de recursos

- [ ] Documentar las revisiones de ROM admitidas por la base elegida.
- [ ] Validar formato e integridad mediante hashes de referencia verificables.
- [ ] Preparar extracción local y mensajes claros para archivos incompatibles.
- [ ] Definir selección de archivos y almacenamiento privado de la aplicación en Android.

Criterio de cierre: recursos cargables generados desde una copia compatible del usuario, sin incluirlos en Git.

## 3. Primera compilación Android

- [ ] Integrar el código y las dependencias de la base seleccionada.
- [ ] Generar una compilación de desarrollo para ARM64.
- [ ] Validar arranque, vídeo y audio en un dispositivo Android real.
- [ ] Documentar instalación, registros de diagnóstico y problemas conocidos.

Criterio de cierre: aplicación instalable que inicia el juego con los recursos locales.

## 4. Experiencia de juego

- [ ] Implementar o adaptar controles táctiles y mandos.
- [ ] Validar guardado y carga de partidas.
- [ ] Comprobar pausa, suspensión, reanudación y orientación de pantalla.
- [ ] Medir estabilidad y rendimiento en el dispositivo de prueba.

Criterio de cierre: recorrido de prueba reproducible con controles, sonido y guardado funcionales.

## 5. Compilaciones reproducibles

- [ ] Automatizar compilación y verificaciones que no necesiten la ROM.
- [ ] Documentar versiones de herramientas y dependencias.
- [ ] Preparar distribución de la aplicación sin ROM ni recursos originales del juego.

## Decisiones pendientes

| Decisión | Estado |
| --- | --- |
| Plataforma | Android |
| Arquitectura inicial | ARM64 propuesta; validar con la base y el dispositivo |
| Motor o port de partida | Pendiente de evaluación |
| Versión mínima de Android | Pendiente |
| SDK y NDK | Pendientes |
| Backend gráfico | Pendiente |
| Revisiones de ROM compatibles | Las que admita la base seleccionada |
| Licencia del código del proyecto | Definir al integrar la base |
