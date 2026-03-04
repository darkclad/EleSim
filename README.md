# EleSim

An electronic circuit simulator with a visual schematic editor. Build circuits, run DC/AC/transient analysis, and visualize waveforms — all in a single desktop application.

<!-- ![EleSim Screenshot](screenshots/main.png) -->

## Features

**Schematic Editor**
- Grid-snapped component placement with drag-and-drop or click-to-place
- Smart wire routing with automatic T-junction detection
- Component rotation, property editing, and double-click quick edit
- Right-click context menu with probe assignment and component actions
- Undo/redo, save/load circuits (`.esim` JSON format), recent files

**Simulation Engine**
- Modified Nodal Analysis (MNA) solver
- DC operating point analysis
- AC frequency-domain analysis (complex impedance)
- Transient time-domain simulation with real-time streaming
- Mixed DC+AC superposition analysis
- Nonlinear device support (iterative convergence for diodes, MOSFETs)
- Background threading with cancellation support
- Solver state continuation — zooming out computes only new steps

**Oscilloscope**
- Dual-channel voltage and current probes
- Node voltage and branch current measurement
- Crosshair cursor with value readout
- Zoom (scroll wheel) and pan (click-drag), double-click to reset
- Auto-scale based on detected signal frequency
- Time axis formatting across ps/ns/µs/ms/s ranges
- Time slider for quick navigation
- Probe selection via right-click context menu on schematic
- Settings saved/restored with circuit files

**Components (21)**

| Category | Components |
|----------|-----------|
| Sources | DC Voltage, AC Voltage, Pulse, DC Current, Ground |
| Passive | Resistor, Capacitor, Inductor, Junction |
| Switches | SPST, SPDT (3-way), DPDT (4-way) |
| Semiconductors | Diode, Zener Diode, Lamp, NMOS FET, PMOS FET |
| Logic Gates | NOT, AND, OR, XOR (CMOS) |

## Building

### Prerequisites

- [MSYS2](https://www.msys2.org/) with the UCRT64 environment
- CMake 3.20+
- GCC (C++17)

Install dependencies:

```bash
pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-svg mingw-w64-ucrt-x86_64-eigen3
```

### Build & Run

```bash
# Shared (default) — debug build
build.bat debug run

# Release build
build.bat release run

# Clean rebuild
build.bat release clean run
```

### Fully Static Build

Produces a single `EleSim.exe` with zero external DLLs (only Windows system libraries).

```bash
# Install static Qt (one-time)
pacman -S mingw-w64-ucrt-x86_64-qt6-base-static mingw-w64-ucrt-x86_64-qt6-svg-static

# Build
build.bat release static run
```

### Creating an Installer

Uses the [Qt Installer Framework](https://doc.qt.io/qtinstallerframework/) to produce a setup wizard with Start Menu/Desktop shortcuts and an uninstaller.

```bash
# Install Qt IFW (one-time)
pacman -S mingw-w64-ucrt-x86_64-qt-installer-framework

# Build + create installer
build.bat release static install
```

Output: `EleSim-0.1.0-Setup.exe`

## Project Structure

```
EleSim/
├── src/
│   ├── core/               # Data model: components, circuit, wires
│   │   └── components/     # 21 component implementations
│   ├── simulation/          # MNA solver, streaming engine, state management
│   └── ui/                  # Qt widgets and schematic editor
│       └── graphics/        # Visual component renderers
├── resources/icons/         # SVG symbols for all components
├── tests/                   # Circuit-based integration tests
│   └── circuits/            # Reference .esim circuit files
└── build.bat                # Build script (debug/release, shared/static)
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [Qt 6](https://www.qt.io/) | 6.10 | GUI framework, graphics scene, SVG rendering |
| [Eigen](https://eigen.tuxfamily.org/) | 5.x | Linear algebra for the MNA solver |

## Tests

Tests verify simulation accuracy by building real circuits and comparing results against analytical solutions.

```bash
build.bat debug
cmake -B build-Debug -DELESIM_BUILD_TESTS=ON
cmake --build build-Debug
ctest --test-dir build-Debug
```

Test suites include voltage dividers, RC/RL/RLC filters, bridge rectifiers, diode clippers, zener regulators, MOSFET circuits, CMOS logic gates, and more.
