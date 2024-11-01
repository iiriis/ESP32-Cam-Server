# ESP32-Cam-Server
A TCP and UDP based ESP32 CAM driver in C with OpenGL rendering

This code was compiled on a windows machine using MSYS2.
Ensure proper packages are installed, below are the `pacman` commands are used to install.

### Install GLEW (OpenGL Extension Wrangler Library)
Install GLEW, which is needed to manage OpenGL extensions:

```bash
pacman -S mingw-w64-x86_64-glew
```
### Install OpenGL Libraries
Install the core OpenGL libraries:

```bash
pacman -S mingw-w64-x86_64-opengl
```

### Install GLFW (Graphics Library Framework)
Install GLFW, which provides a simple API for creating windows, contexts, and managing input:

```bash
pacman -S mingw-w64-x86_64-glfw
```

### Install SOIL (Simple OpenGL Image Library)
Install SOIL, which allows for easy image loading in OpenGL:

```bash
pacman -S mingw-w64-x86_64-soil
```

After all the packages are installed, run `make <tcp/udp>` to compile the desired binary. Ex **`make TCP`** will compile the *visualizer_tcp.c*.


