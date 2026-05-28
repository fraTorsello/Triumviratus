# Triumviratus Chess Engine

Triumviratus is a strong, UCI-compliant chess engine written in C++. 
Version 3.2 introduces a stable hybrid architecture, combining classical alpha-beta search enhancements with NNUE evaluation and an experimental policy network.

Currently, Triumviratus 3.2 Hybrid plays at an estimated strength of **~3450+ Elo** (CCRL scale), having demonstrated balanced performance against established engines in its rating bracket.

## Features

* **Protocol:** Fully UCI compliant.
* **Board Representation:** 64-bit Bitboards.
* **Search:** Principal Variation Search (PVS), Iterative Deepening, Aspiration Windows.
* **Pruning & Reductions:** Null Move Pruning (NMP), Late Move Reductions (LMR), Futility Pruning, History Heuristics.
* **Evaluation:** NNUE (HalfKAv2 architecture) for highly accurate static evaluation.
* **Policy Network:** Experimental move-ordering policy network (can be toggled via UCI options).

## Setup & Usage

To use Triumviratus in any standard chess GUI (e.g., Cute Chess, Arena, BanksiaGUI, Lichess-bot):

1. Download the latest executable from the [Releases](../../releases) tab.
2. The engine requires specific network files to run correctly. Ensure the following files are placed in the **same directory** as the executable:
   * `nn-b1a57edbea57.nnue` (Big Net)
   * `nn-baff1ede1f90.nnue` (Small Net)
   * `triumviratus_policy.bin` (Policy Network weights)
3. Add the engine to your GUI using the standard UCI installation procedure.

### Recommended UCI Settings
* **Hash:** Allocate according to your available RAM (default is 64 MB, 1024 MB or more recommended for long time controls).
* **Threads:** Set to match your hardware's optimal concurrent thread count.
* **UsePolicy:** `false` (default for standard testing), can be set to `true` to enable the experimental policy network.

## Compiling from Source

For maximum performance, Triumviratus should be compiled with AVX2 optimizations enabled. The project is configured for MSBuild (Visual Studio).

### Windows (MSVC)
Using PowerShell and the MSBuild tools, navigate to the source directory and run:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "Triumviratus_3.0.vcxproj" `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:TargetName="Triumviratus_3.2_Hybrid_AVX2" `
  /p:CL_AdditionalOptions="/O2 /GL /arch:AVX2 /fp:fast /Oy /Qpar" `
  /p:Link_AdditionalOptions="/LTCG /OPT:REF /OPT:ICF"
