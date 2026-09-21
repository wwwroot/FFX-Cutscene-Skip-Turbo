# FFX Cutscene Skip & Turbo

A native C++ modification for Final Fantasy X HD Remaster (PC / Steam) that provides opening logo skip, dialogue fast-forward, and variable game clock acceleration.

## Features

- **Intro Skip**: Automatically bypasses opening publisher and engine splash screens (Square Enix, Virtuos, CRIWARE, Dolby) directly to the title screen.
- **Dialogue Fast-Forward**: Advances text boxes and dialogue delays when Turbo is engaged.
- **Turbo Speed Hack**: Configurable game clock multiplier (default 8.0x) for dialogue and field exploration.
- **Battle & Movie Safety**: Automatically disengages Turbo during active combat and pre-rendered FMV sequences to prevent script desync, audio stutter, and video playback errors.
- **Controller & Keyboard Support**: Toggle or hold mode via keyboard (`R` by default) or gamepad combo (`Select + X`).
- **Minimal OSD**: Simple on-screen indicator in the top-right corner showing active Turbo status. Automatically hides when Turbo is disengaged, in combat, during FMVs, or when the game window loses focus.

## Installation

### Method 1: Existing Module Loader
If you already use `dinput8.dll` / FFX Module Loader:
1. Copy the `modules` directory from `FFX_Cutscene_Skip_v1.0.1.zip` into your game root directory:
   ```text
   ...\SteamLibrary\steamapps\common\FINAL FANTASY FFX&FFX-2 HD Remaster\
   ```

### Method 2: Standalone Installation
Extract all files from `FFX_Cutscene_Skip_AllInOne_v1.0.1.zip` directly into your game root directory.

## Configuration

Settings can be modified in `modules/config/ff10-cutscene-skip.ini`:

```ini
[Settings]
SpeedMultiplier = 8.0
SkipCutscenesAtSpeed = true
SkipIntro = true
ControlMode = Toggle
KeyboardHotkey = R
EnableGamepad = true
GamepadCombo = SELECT+X
ShowOSD = true
AutoDisableInBattle = true
LogToFile = false
DebugMode = false
```

### Hotkey Options
- **Keyboard**: Single key names (e.g. `R`, `G`, `TAB`, `SPACE`, `F1`-`F12`).
- **Gamepad**: Key combinations in `BUTTON+BUTTON` format (e.g. `SELECT+X`, `BACK+A`, `L3+R3`, `LB+RB`).
- **ControlMode**: `Toggle` (press once to enable/disable) or `Hold` (active only while button is held).

## Building from Source

Requires Visual Studio (x86 MSVC toolchain).

```bat
build.bat
```

To run test suites:
```bat
run_tests.bat
```

To package release archives:
```bat
package.bat
```

## Credits

- **Kaldaien**: Research on FFX speed mechanics from UnX.
- **ffgriever**: FFX Module Loader interface.
- **Tsuda Kageyu**: MinHook library.

## License

MIT License. See [LICENSE](LICENSE) for details.
