# 🎯 AZ Tank Game - A* & Pathfinding Sandbox Module

This folder contains the pathfinding module for the AZ Tank Game. It includes a native C++ A* implementation, a sandbox for running isolated pathfinding benchmarks, and a file-based Python bridge to plug in custom python-based pathfinding algorithms.

---

## 🛠️ Folder Structure

*   [astar_bot.h](astar_bot.h) / [astar_bot.cpp](astar_bot.cpp): The C++ bot wrapper class that supports both native C++ pathfinding and Python-bridge command routing.
*   [bridge_main.cpp](bridge_main.cpp): The entry point for the isolated game window (`AZgameBridge`) used for sandbox/benchmark testing.
*   [python/](python/):
    *   [pathfind_controller.py](python/pathfind_controller.py): The main Python loop that reads the game state, runs the pathfinder, and writes controls/waypoints.
    *   [pathfind_planner.py](python/pathfind_planner.py): Core implementation of python search algorithms.
    *   [pathfind_env.py](python/pathfind_env.py): Handles process management and reading/writing files.
    *   [pathfind_analyzer.py](python/pathfind_analyzer.py): Performance analysis and automated benchmark runs.

---

## 🚀 Ways to Run the A* / Python Bot

### 1. Python-Bridged Main Game (Playable Window)
To control a tank in the **main game window** (`AZgame.exe`) using your Python bot and render your Python-generated waypoints on the map:

1.  **Launch the main game**:
    ```powershell
    ./build/AZgame.exe
    ```
2.  **Activate A\* Bot**: Click the **Settings (⚙️)** button in-game. Change **Player 2 (P2)** to **Bot** and select **A\***.
3.  **Start your Python script**:
    In another terminal at the repo root, run:
    ```powershell
    $env:AZ_NO_LAUNCH="1"
    python Astar/python/pathfind_controller.py
    ```

> [!NOTE]
> Setting `$env:AZ_NO_LAUNCH="1"` tells the Python environment to hook into the running main game instead of spawning the standalone sandbox executable.

---

### 2. Standalone Sandbox / Pathfinding Isolated (`AZgameBridge`)
To run a standalone simulation dedicated to benchmarking/visualizing the pathfinder script:

*   **Standard Launch**:
    ```powershell
    python Astar/python/pathfind_controller.py
    ```
    *(This script automatically launches `AZgameBridge.exe` in the background and controls Player 0 against Player 1.)*
*   **Run via CMake Custom Target**:
    ```powershell
    ninja run_astar
    ```

---

### 3. Automated Benchmark & Analyzer
To run pathfinding algorithms across a sequence of generated maps and analyze path lengths, planning durations, and success rates:

```powershell
python Astar/python/pathfind_analyzer.py
```
This script runs benchmarks and outputs performance results directly to a CSV file (e.g., `benchmark_results.csv`).

---

## ⚙️ Configuration & Algorithm Swapping

You can customize the search algorithm, heuristics, and logging options via **Environment Variables** without modifying any C++ or Python source code.

### 🧩 Available Settings

| Variable | Description | Options | Mapped To |
| :--- | :--- | :--- | :--- |
| `AZ_ALGO` | Selects the pathfinding algorithm | `astar`, `theta_star`, `dfs`, `bfs`, `jps` | [pathfind_planner.py](python/pathfind_planner.py) |
| `AZ_HEURISTIC` | Selects the heuristic metric (A* only) | `manhattan`, `euclidean`, `diagonal`, `chebyshev` | [pathfind_planner.py](python/pathfind_planner.py) |
| `AZ_NO_LAUNCH` | Disables background launch of `AZgameBridge.exe` | `1` (Hook into main game) / `0` (Default) | [pathfind_env.py](python/pathfind_env.py) |
| `AZ_LOG` | Specifies filepath to log performance metrics | e.g. `pathfind.log` | [pathfind_controller.py](python/pathfind_controller.py) |

### 💡 Examples

*   **Run Standalone Sandbox with Jump Point Search (JPS)**:
    ```powershell
    $env:AZ_ALGO="jps"
    python Astar/python/pathfind_controller.py
    ```
*   **Run Standalone Sandbox with A\* (Euclidean Distance)**:
    ```powershell
    $env:AZ_ALGO="astar"
    $env:AZ_HEURISTIC="euclidean"
    python Astar/python/pathfind_controller.py
    ```
*   **Run Standalone Sandbox with Depth First Search (DFS)**:
    ```powershell
    $env:AZ_ALGO="dfs"
    python Astar/python/pathfind_controller.py
    ```
