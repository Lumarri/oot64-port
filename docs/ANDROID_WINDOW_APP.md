# APK técnico con ventana GLES

El prototipo **MM3D · Prueba gráfica** (`org.lumarri.mm3d.prototype`) muestra una
textura original animada mediante el generador TEV de TriAevum y el backend GLES
del proyecto. No carga la ROM, recursos ni código del juego. No es todavía el
menú de inicio del port ni un arranque de Majora's Mask 3D.

## Compilar e instalar

Herramientas verificadas: Windows/Python, WSL Ubuntu con NDK r29, SDK Android
plataforma 34, Build Tools **36.0.0** y JDK **21.0.10**. El SDK y el JDK deben estar
en `ANDROID_HOME` y `JAVA_HOME`; se pueden pasar `--sdk` y `--jdk` al script.
El checkout upstream limpio y fijado se prepara como indica
[ANDROID_NATIVE.md](ANDROID_NATIVE.md). No hace falta descargar Gradle ni añadir
dependencias de interfaz: el componente Android es NativeActivity.

Desde PowerShell, en la raíz del repositorio:

```powershell
python -B tools/android/build_window_apk.py
python -B tools/android/test_window_app.py --install
```

Hay tareas equivalentes `MM3D:` en VS Code. El primer script compila en WSL con
`MM3D_BUILD_WINDOW_APP=ON`, verifica ELF64/AArch64 y alineación LOAD de 16 KiB,
prepara el manifiesto con `aapt2`, empaqueta solamente las dos bibliotecas
permitidas y los avisos, alinea las bibliotecas a 16 KiB y firma con una clave
debug local. Verifica después firma, alineación, entradas ZIP y hashes.
`--skip-native` permite empaquetar bibliotecas ya preparadas y verificadas;
usarlo sólo cuando no se han cambiado sus fuentes.

El resultado es `local/android-apk/mm3d-window-debug.apk`. Su SHA-256, alcance y
hashes nativos quedan en `local/android-apk/manifest.json`. La clave está en
`local/android-app-signing/`; no es una clave de publicación. Todos estos
archivos están fuera de Git. ROM y derivados siguen fuera tanto del APK como
de GitHub. No se piden permisos de almacenamiento ni red.

El segundo script verifica el APK, lo instala/actualiza por ADB y abre su Activity.
Prueba imagen visible, salida al inicio del teléfono y regreso, y giro a horizontal.
Restaura la configuración de rotación previa incluso si falla una comprobación.
Las capturas se guardan sólo cuando la ventana del prototipo tiene el foco.
Los registros privados contienen únicamente la etiqueta `MM3DWindow` del intervalo
de la prueba, sin limpiar el logcat del dispositivo ni guardar su número de serie.
La aplicación queda instalada y puede abrirse desde su icono.

## Resultado en el moto g24 power

**PASS el 17 de noviembre de 2026**, Android 14, Mali-G52 MC2, GLES 3.2:

- Ventana EGL real, dibujo TEV, lectura de píxel y `eglSwapBuffers` correctos.
- Al ir al inicio del teléfono se pausa el bucle y se libera el contexto.
- Al regresar se crean superficie/contexto nuevos y vuelve la imagen.
- El giro de pantalla recrea la Activity y el contexto y mantiene el dibujo.
- Capturas vertical y horizontal revisadas: textura visible, proporción y bordes.

Evidencia privada: `local/android-logs/window-20261117T211340246938Z/`, incluidos
`report.json`, `app.log`, `portrait.png` y `landscape.png`. Los cuatro checks del
informe son `true`. Las superficies registradas son 720x1612 y 1535x720; el tamaño
útil puede variar por las barras del sistema.
SHA-256 del APK instalado y probado:
`6437db12b09bdca9efa7f0e1f27913c2ec0482d818dbd343197db3e99b5f2d05`.

## Responsabilidades y límites

`ports/android/window_app/native_main.cpp` posee EGL y usa `native_app_glue` del
NDK para recibir los eventos de ventana, pausa, reanudación y foco. Atiende el
bucle mediante `ALooper_pollOnce`; sólo dibuja con ventana, foco y Activity activa.
El contenido de prueba se recrea después de cerrar la superficie. No conserva
estado del juego porque todavía no existe un módulo cargado en este host.

El renderer se compila ahora como biblioteca estática común a la sonda y al APK;
no hay una segunda implementación de PICA para la ventana. En caso de contexto
perdido se abandonan sus nombres GL sin borrarlos en otro contexto. La sonda
adicional `context_recreation_probe.h` destruye un contexto, crea otro independiente
y comprueba que descartar el backend anterior no afecta a los dibujos nuevos.
La pérdida EGL_CONTEXT_LOST de un contexto vivo no se ha provocado en el teléfono:
ese error cierra la sesión con diagnóstico, en vez de prometer recuperación del juego.

La animación está limitada a unas 30 actualizaciones por segundo; esto no mide el
rendimiento del juego. El backend aún crea una imagen de transferencia por envío
y conserva los límites de estados y coherencia documentados. Faltan el puente
del runtime completo, el módulo MM3D, ambas pantallas, controles, audio, archivos,
guardado y menú/configuración. El APK contiene licencias en `assets/licenses/`;
no compila iMMERSE/MXAO ni DLSS. El host SDL upstream queda intacto.

Referencias oficiales: [NativeActivity](https://developer.android.com/reference/android/app/NativeActivity),
[native activity del NDK](https://developer.android.com/ndk/reference/group/native-activity),
[zipalign](https://developer.android.com/tools/zipalign) y
[apksigner](https://developer.android.com/tools/apksigner).
