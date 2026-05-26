# Mag Engine

A software-rendered 2D/3D game engine written from scratch in C, targeting Win32.
No OpenGL, no DirectX, no SDL — every pixel is drawn by hand.

Inspired by the philosophy of John Carmack and Casey Muratori: understand the
machine, own the stack, write code that lasts.

## Current Features

**3D Renderer**
- Vector-based raycasting (Doom-style, no BSP)
- Perspective-correct wall texture mapping
- Floor and ceiling casting
- Billboard sprite rendering with z-buffer
- Point lights with attenuation
- Distance fog
- Camera shake

**2D Mode**
- Top-down world rendering (Yuppie Psycho style)
- Tile-based map with texture support
- Flashlight / dynamic lighting
- Entity system (NPC, item, monster, trigger, hiding spot)
- Dialog system with portrait
- Inventory
- Health / damage system

**Physics & Feel**
- Spring physics (weapon bob, camera sway)
- Particle system (blood, smoke, sparks)
- Fade in / out transitions
- Timer system
- Sprite animation state machine

**Engine Tools**
- Procedural texture generation
- Text-file level loader (.lvl format)
- Bitmap font renderer
- Line-of-sight raycast
- Shoot raycast

## Building

Requires Windows + MSVC (Visual Studio Build Tools).

Open a Developer Command Prompt and run:

```
build.bat
```

Output goes to `build/` and is copied to `D:\main\`.

## Controls (demo)

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Left Click | Shoot |
| Space | Jump |
| C | Crouch |
| R | Look up |
| ESC | Quit |

## Project Structure

```
engine.h        public API — the only file a game programmer needs to include
engine.c        full engine implementation (~2000 lines)
main.c          demo game (should stay under 400 lines)
stb_image.h     single-header PNG loader (Sean Barrett, MIT)
build.bat       MSVC build script
```

## Roadmap

**Renderer**
- [ ] DDA grid traversal raycast (replace current Cramer intersection)
- [ ] Proper sprite occlusion and clipping
- [ ] Normal-mapped wall shading
- [ ] Sector-based height variation (stairs, ramps)
- [ ] Sky texture

**Gameplay Systems**
- [ ] Audio mixer (Win32 WASAPI, no SDL_mixer)
- [ ] Save / load system
- [ ] Pathfinding for enemy AI (A* on tile grid)
- [ ] Cutscene / dialogue scripting

**Tools**
- [ ] Level editor (in-engine, no external tool dependency)
- [ ] Asset pipeline (sprite sheet packer)
- [ ] Hot-reload for textures

**Architecture**
- [ ] Arena allocator (replace malloc/free)
- [ ] Separate game.dll from engine for hot-reloading game code
- [ ] Full 64-bit Linux port

## Philosophy

The goal is not to build another Unity wrapper.
The goal is to understand every byte that hits the screen.

> "I want to know what every line of code does, and why."

