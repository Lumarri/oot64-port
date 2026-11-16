# Desarrollo local en Windows con VS Code

Esta guía prepara el editor y la conexión con un teléfono Android por USB. El proyecto todavía no contiene una aplicación compilable ni un APK.

## 1. Instalar las herramientas iniciales

- [Visual Studio Code](https://code.visualstudio.com/).
- [Git para Windows](https://git-scm.com/downloads/win).
- [Android SDK Platform-Tools para Windows](https://developer.android.com/tools/releases/platform-tools), que incluye `adb.exe`.

Para Platform-Tools puedes elegir una de estas opciones:

- Si tienes Android Studio, instala **Android SDK Platform-Tools** desde su SDK Manager y comprueba la ubicación del SDK. Habitualmente está en `%LOCALAPPDATA%\Android\Sdk\platform-tools`.
- Si descargaste el ZIP independiente, extráelo, por ejemplo, en `C:\Android`, de modo que el ejecutable quede en `C:\Android\platform-tools\adb.exe`.

En Windows, abre **Editar las variables de entorno de esta cuenta**, edita `Path` en las variables de usuario y añade la carpeta que contiene `adb.exe`. Conserva las entradas existentes. Cierra todas las ventanas de VS Code y vuelve a abrirlo para que las tareas reciban el nuevo `Path`.

Comprueba en PowerShell:

```powershell
git --version
adb version
```

Si `adb` no se reconoce, revisa la carpeta añadida a `Path` y abre una terminal nueva.

## 2. Abrir el repositorio

Desde la carpeta donde guardas tus proyectos, ejecuta en PowerShell:

```powershell
git clone https://github.com/Lumarri/oot64-port.git
cd oot64-port
code .
```

Si ya tienes una copia local, abre esa carpeta en VS Code. Si `code` no se reconoce, usa **Archivo → Abrir carpeta** y selecciona `oot64-port`.

## 3. Continuar con Codex en tu equipo

Instala la extensión oficial [Codex de OpenAI](https://marketplace.visualstudio.com/items?itemName=openai.chatgpt) y sigue su inicio de sesión. El repositorio también la incluye entre las extensiones recomendadas de VS Code.

Trabaja en una sesión local sobre esta carpeta. La sesión anterior en la nube y sus archivos adjuntos no se transfieren automáticamente al equipo. Para comprobar el teléfono, abre una terminal **PowerShell de Windows** dentro de VS Code.

Puedes iniciar la conversación local con este contexto:

> Continuamos el proyecto oot64-port para Android desde Windows. Revisa README.md y docs/ROADMAP.md. Comprueba la conexión del teléfono por ADB y obtén modelo, versión de Android y arquitecturas compatibles. El repositorio contiene documentación y tareas de VS Code; todavía no tiene motor integrado ni APK. Estamos evaluando Shipwright-Android como base y la compatibilidad de la ROM sigue pendiente de resolver. Mantén ROMs y recursos extraídos en las carpetas locales excluidas de Git.

La [guía oficial de la extensión](https://developers.openai.com/codex/ide/) describe su instalación y el trabajo local.

## 4. Conectar y comprobar el teléfono

1. En el teléfono, activa **Opciones de desarrollador** y **Depuración USB**. La ubicación varía según el fabricante; suele habilitarse pulsando varias veces **Número de compilación** en los ajustes del dispositivo.
2. Conecta el teléfono con un cable que permita transferencia de datos.
3. Desbloquea la pantalla y acepta el aviso de autorización de depuración USB para tu computadora.
4. Ejecuta en PowerShell:

```powershell
adb devices -l
```

El teléfono debe aparecer con estado `device`.

| Resultado | Siguiente paso |
| --- | --- |
| `device` | La conexión ADB está disponible. |
| `unauthorized` | Desbloquea el teléfono y acepta el aviso de autorización USB. |
| `offline` | Reconecta el cable y vuelve a comprobar la conexión. |
| Lista vacía | Comprueba el cable, la depuración USB y el controlador USB del fabricante en Windows. |

Para obtener los datos del único teléfono conectado por USB:

```powershell
adb -d shell getprop ro.product.model
adb -d shell getprop ro.build.version.release
adb -d shell getprop ro.product.cpu.abilist
```

Si hay varios dispositivos USB, usa `adb -s SERIAL` en lugar de `adb -d`, sustituyendo `SERIAL` por el identificador mostrado en la lista.

También puedes abrir **Terminal → Ejecutar tarea** en VS Code y elegir cualquiera de las tareas `Android:`. Estas tareas utilizan `adb` desde `Path`; las de consulta seleccionan un único dispositivo USB con `-d`.

## 5. Preparar la compilación del port

El SDK, NDK y el sistema de compilación se fijarán al integrar la base elegida. La [documentación de Shipwright-Android](https://github.com/Waterdish/Shipwright-Android#build) describe un entorno de compilación Linux y propone WSL o una máquina virtual para usuarios de Windows. Si adoptamos esa base, prepararemos WSL2 para compilar y podremos seguir usando ADB desde Windows para instalar y probar el APK.

Una terminal WSL no recibe automáticamente el teléfono conectado a Windows. Las comprobaciones USB de esta guía se ejecutan en PowerShell local. Todavía no hay una tarea de compilación o instalación porque el motor no se ha integrado.

## ROM y archivos locales

Mantén tu copia de la ROM en `roms/` o `local/` y los recursos extraídos en `extracted/`. Estas carpetas están excluidas de Git. La validación de la ROM se debe resolver antes de extraer recursos con la base seleccionada; un nombre de archivo o una cabecera reconocible no garantizan compatibilidad.
