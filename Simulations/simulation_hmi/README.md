# VLB HMI Software Simulation

This is a standalone PC-based simulation of the Vertical Lift Bridge dashboard. It uses the actual firmware data structures and LVGL graphics to emulate the bridge behavior on your Windows desktop.

## Prerequisites

1.  **VS Code + PlatformIO** (Already installed).
2.  **SDL2 Library for Windows**:
    *   Download `SDL2-devel-2.x.x-mingw.zip` from the [SDL2 Releases page](https://github.com/libsdl-org/SDL/releases).
    *   Extract it to a folder (e.g., `C:\SDL2`).
    *   Add the `bin` folder (e.g., `C:\SDL2\x86_64-w64-mingw32\bin`) to your **Windows Environment PATH**.
    *   Copy `SDL2.dll` from the `bin` folder into this `simulation_hmi` directory.

## How to Run

1.  Open VS Code.
2.  Go to the **File > Open Folder...** and select this `simulation_hmi` directory.
3.  Wait for PlatformIO to initialize the project.
4.  Click the **PlatformIO: Build** button (Checkmark icon) in the bottom status bar.
5.  Click the **PlatformIO: Run** button (Arrow icon) to launch the simulation window.

## Simulation Features

- **Real-time Bridge Logic**: Clicking "RAISE" in the OPS tab will simulate the motor winding cables, updating the height bar and current meter.
- **Modern Industrial Design**: Dark-mode UI with high-contrast status indicators.
- **Shared Data Model**: Uses the exact `SharedStatus_t` struct from the main project.
- **Hardware Mocks**: Simulated ADC noise and position tracking.

## Layout

- **HOME**: Visual representation of the deck height and road traffic lights.
- **OPS**: Manual controls (Raise/Lower/Hold) and motor current gauge.
- **VISION**: (Stub) Visualizes camera detection events.
- **DIAG**: (Stub) System health and fault register.
- **SET**: (Stub) HMI preferences.
