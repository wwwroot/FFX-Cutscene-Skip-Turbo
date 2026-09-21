# Final Fantasy X HD Remaster - Cutscene Skip & Turbo Mod

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Windows x86](https://img.shields.io/badge/Platform-Windows%20x86-lightgrey.svg)]()
[![Game: FFX HD Remaster](https://img.shields.io/badge/Game-Final%20Fantasy%20X%20HD-blueviolet.svg)]()

A native C++ modification for **Final Fantasy X HD Remaster (Steam / PC)** providing opening logo bypass, automated dialogue/cutscene progression, and configurable game clock acceleration.

Designed as a standalone module for the FFX external module loader with zero runtime dependencies.

---

## Features

- **Intro & Splash Skip**:
  - Bypasses publisher and developer splash screens (Square Enix, Virtuos, CRIWARE, Dolby) on startup.
  - Boots directly into the main menu.
  - Configurable via `SkipIntro` in `ff10-cutscene-skip.ini`.

- **In-Engine Dialogue & Cutscene Acceleration**:
  - Automatically advances dialogue text boxes and in-engine event timers while preserving script state.

- **Configurable Turbo Clock (Default 8.0x)**:
  - Accelerates internal game logic clock (configurable from `1.0x` to `64.0x`).

- **Battle State Safety**:
  - Continuously reads the battle engine state and automatically suspends Turbo during active combat or scripted boss encounters to prevent script softlocks and animation desyncs.

- **Input Controls**:
  - **Keyboard**: Default `R` key.
  - **Gamepad (XInput)**: Default `Select + X`.
  - Supports both **Toggle** and **Hold** modes.

- **On-Screen Display (OSD)**:
  - Lightweight overlay indicating active Turbo multiplier in the corner of the window.
  - Non-intrusive implementation without Direct3D hook overhead.

- **Diagnostic Logging**:
  - Optional scene, opcode, and battle transition tracing written to `cutscene_skip.log`.

- **Compatibility**:
  - Compatible with FFX External File Loader, custom textures, audio mods, and Reshade.

---

## Installation

### Prerequisites
- Final Fantasy X/X-2 HD Remaster (Steam release)
- FFX External File Loader (`dinput8.dll`) installed in the game directory

### Standard Installation
1. Download `FFX_Cutscene_Skip_v1.0.0.zip`.
2. Extract the `modules` directory into your game installation folder:
   ```text
   ...\SteamLibrary\steamapps\common\FINAL FANTASY FFX&FFX-2 HD Remaster\
   ```
3. Launch the game via Steam.

### Standalone / Fresh Install
1. Download `FFX_Cutscene_Skip_AllInOne_v1.0.0.zip`.
2. Extract all contents directly into the game root directory.
3. Launch the game via Steam.

---

## Configuration

Settings can be modified in `modules/config/ff10-cutscene-skip.ini`:

```ini
[Settings]
; Speed multiplier applied when Turbo is engaged (e.g. 4.0, 8.0, 16.0).
SpeedMultiplier = 8.0

; Automatically bypass dialogue pauses and cutscene wait timers when Turbo is active.
SkipCutscenesAtSpeed = true

; Skip publisher and engine splash logos on launch and boot directly to menu.
SkipIntro = true

; Input control mode: "Toggle" or "Hold".
ControlMode = Toggle

; Keyboard hotkey (e.g. R, G, F1-F5, TAB, SPACE).
KeyboardHotkey = R

; Enable controller input monitoring (XInput).
EnableGamepad = true

; Gamepad button combination (e.g. SELECT+X, SELECT+A, L3+R3, LB+RB).
GamepadCombo = SELECT+X

; Render Turbo status indicator when active.
ShowOSD = true

; Automatically disable Turbo during combat encounters.
AutoDisableInBattle = true

; Enable diagnostic logging to cutscene_skip.log.
LogToFile = true

; Detailed event and scene tracing.
DebugMode = true
```

---

## Building from Source

### Requirements
- Windows 10 / 11
- Visual Studio 2022 (Desktop development with C++ workload)
- MSVC x86 build tools (`cl.exe`)

### Build
Run the build script from a Developer Command Prompt:
```bat
build.bat
```
The compiled DLL is output to `bin/ff10-cutscene-skip.dll`.

### Tests
Run the unit test suite:
```bat
run_tests.bat
```

### Packaging
Generate release archives:
```bat
package.bat
```
Output files are written to `dist/`.

---

## Credits & References

- **Kaldaien & "Untitled Project X" (UnX)**: Research and initial implementations of game clock manipulation in FFX HD.
- **ffgriever**: FFX Module Loader hook interface.
- **Tsuda Kageyu**: [MinHook](https://github.com/TsudaKageyu/minhook) detour library.
- **Square Enix**: Final Fantasy X.

---

## License

Distributed under the [MIT License](LICENSE).
