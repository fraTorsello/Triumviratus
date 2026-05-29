# Triumviratus Chess Engine

Triumviratus is a strong, UCI-compliant chess engine written in C++.
Version 3.3 builds on the stable hybrid architecture, combining classical alpha-beta search enhancements with NNUE evaluation and an experimental policy network, and adds **Syzygy endgame tablebase** support for perfect endgame play.

Currently, Triumviratus 3.3 Hybrid plays at an estimated strength of **~3450+ Elo** (CCRL scale), having demonstrated balanced performance against established engines in its rating bracket.

## What's New in 3.3

* **Syzygy Tablebases:** In-search WDL probing (never misjudges a covered endgame) plus a root DTZ probe (converts wins / holds draws with perfect technique). Tables up to 5 men are supported via [Fathom](https://github.com/jdart1/Fathom).
* **Robustness:** Fixed an access violation that could occur on illegal/king-capture positions parsed from FEN; the search now guards against searching positions where the side-not-to-move is in check.
* **Search tuning:** History-based LMR reverted to the conservative ±1-ply clamp after match testing.

## Features

* **Protocol:** Fully UCI compliant.
* **Board Representation:** 64-bit Bitboards.
* **Search:** Principal Variation Search (PVS), Iterative Deepening, Aspiration Windows.
* **Pruning & Reductions:** Null Move Pruning (NMP), Late Move Reductions (LMR), Reverse Futility / Futility Pruning, Razoring, Late Move Pruning (LMP), SEE pruning, History & Continuation-History Heuristics.
* **Parallel Search:** ABDADA-style multi-threaded SMP.
* **Evaluation:** NNUE (HalfKAv2_hm architecture) with dual nets (big/small) for highly accurate static evaluation.
* **Endgames:** Syzygy tablebase probing (WDL in search, DTZ at the root).
* **Policy Network:** Experimental move-ordering policy network (can be toggled via UCI options).

## Setup & Usage

To use Triumviratus in any standard chess GUI (e.g., Cute Chess, Arena, BanksiaGUI, Lichess-bot):

1. Download the latest executable from the [Releases](../../releases) tab.
2. The engine requires specific network files to run correctly. Ensure the following files are placed in the **same directory** as the executable:
   * `nn-b1a57edbea57.nnue` (Big Net)
   * `nn-baff1ede1f90.nnue` (Small Net)
   * `triumviratus_policy.bin` (Policy Network weights)
3. *(Optional)* For perfect endgame play, place your Syzygy tablebase files in a folder and point the engine at it (see `SyzygyPath` below). A `Syzygy/` folder next to the executable is auto-detected.
4. Add the engine to your GUI using the standard UCI installation procedure.

### Recommended UCI Settings
* **Hash:** Allocate according to your available RAM (default is 64 MB, 1024 MB or more recommended for long time controls).
* **Threads:** Set to match your hardware's optimal concurrent thread count.
* **SyzygyPath:** Absolute path to the folder containing your Syzygy `.rtbw`/`.rtbz` files (leave empty to disable). Setting this explicitly is recommended when the GUI launches the engine from a different working directory.
* **UsePolicy:** `false` (default for standard testing), can be set to `true` to enable the experimental policy network.

## Compiling from Source

For maximum performance, Triumviratus should be compiled with AVX2 optimizations enabled. The project is configured for MSBuild (Visual Studio 2022, toolset v143) and the Release|x64 configuration already enables `/O2`, AVX2, intrinsics and whole-program optimization (LTCG). The output is named `Triumviratus_3.3.exe`.

### Windows (MSVC)
Using PowerShell and the MSBuild tools, navigate to the source directory and run:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "Triumviratus_3.0.vcxproj" `
  /t:Rebuild `
  /p:Configuration=Release `
  /p:Platform=x64
```

> Note: the Visual Studio project file is still named `Triumviratus_3.0.vcxproj`; only the build output is named `Triumviratus_3.3.exe` (via `<TargetName>` in the Release|x64 configuration).
