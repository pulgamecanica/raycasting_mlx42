# 🚀 MLX42 Web & Native Template

This repository is a **starter template** for projects using [MLX42](https://github.com/codam-coding-college/MLX42).  
It allows you to **build and run** your MLX42 project both **natively** and in the **browser (WebAssembly/WebGL)** with **Emscripten**.

---

## ✨ Features

- ✅ Preconfigured **Makefile** for both native and web builds  
- ✅ Minimal **demo project** (`src/main.c`) with:
  - Randomized pixel rendering  
  - Keyboard movement for an image (`↑ ↓ ← →`)  
  - ESC to close window  
- ✅ **Emscripten HTML template** (`web/demo.html`) with fullscreen canvas  
- ✅ Cross-platform ready:  
  - Native build → desktop executable  
  - Web build → `demo.js` + `demo.wasm` + HTML wrapper  

---

## 📂 Project Structure

```

.
├── src/              # Your source code (.c files)
│   └── main.c
├── include/          # Your header files (.h)
├── MLX42/            # MLX42 submodule (must be cloned)
├── web/              # Web build output
│   ├── demo.html
│   └── demo.js (generated)
├── Makefile
└── README.md

````

---

## 🛠️ Requirements

- **Native build**:
  - GCC / Clang
  - CMake
  - GLFW3
- **Web build**:
  - [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (`emcc` in PATH)

---

## ⚡ Getting Started

### 1. Clone repository with submodules
```bash
git clone --recursive https://github.com/pulgamecanica/mlx42-web-template.git
cd mlx42-web-template
````

If you forgot `--recursive`, fetch MLX42 later with:

```bash
git submodule update --init --recursive
```

---

### 2. Build natively (desktop)

```bash
make
./demo
```

---

### 3. Build for the web (WASM + WebGL)

```bash
make web
```

This generates:

```
web/demo.js
web/demo.wasm
web/demo.html
```

Open `web/demo.html` in a browser (or use a local server):

```bash
python3 -m http.server -d web
```

Then visit: [http://localhost:8000/demo.html](http://localhost:8000/demo.html)

---

## 🎮 Controls

* **ESC** → Exit window (native)
* **↑ / ↓ / ← / →** → Move the generated image

---

## 🔧 Makefile Targets

| Target        | Description                         |
| ------------- | ----------------------------------- |
| `make`        | Build native executable (`./demo`)  |
| `make web`    | Build for the browser (WebAssembly) |
| `make clean`  | Remove object files                 |
| `make fclean` | Remove binary & object files        |
| `make re`     | Rebuild everything from scratch     |

---

## 📖 Notes

* Put your assets in an `assets/` folder and preload them by uncommenting the line in the **Makefile**:

  ```make
  # --preload-file assets
  ```
* `mlx_loop_hook` supports **multiple callbacks** (as shown in `main.c`).
* Native MLX42 and WebAssembly MLX42 are **compiled separately**.

---

## 💡 Inspiration

This template was made to:

* Quickly bootstrap **MLX42 graphics projects**
* Provide an **easy migration path** from **MLX (42 school)** to **MLX42**
* Run your projects directly in the **browser** 🚀

---

Happy coding 🎉 by pulgamecanica

