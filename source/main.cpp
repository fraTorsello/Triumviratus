/*
 * Triumviratus Chess Engine - Main Entry Point
 * UCI compliant - no debug output on startup
 */

#include "defs.h"
#include "io.h"
#include "tt.h"
#include "uci.h"
#include "threads.h"
#include "sf_bridge.h"

//Added for policy network
#include "policy_bridge.h"

#include <string>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// Resolve an NNUE net filename to a path that exists, INDEPENDENT of the current
// working directory. Match runners / GUIs (e.g. cutechess) often launch the
// engine from a different cwd than the project root; the net is loaded by
// relative path, so a wrong cwd used to make the net silently fail to load ->
// eval returns 0 -> the engine plays junk (a2a3...). We search, in order:
//   1) <exe dir>\<name>          (net shipped next to the binary)
//   2) <exe dir>\..\..\<name>    (project root, when exe sits in x64\Release)
//   3) <name>                    (current working directory; legacy fallback)
// Returns the first existing path, or an empty string if none is found.
static std::string resolve_net_path(const std::string& name)
{
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
    {
        std::string exePath(buf, n);
        size_t slash = exePath.find_last_of("\\/");
        if (slash != std::string::npos)
        {
            std::string exeDir = exePath.substr(0, slash);
            const std::string cands[] = { exeDir + "\\" + name,
                                          exeDir + "\\..\\..\\" + name };
            for (const std::string& c : cands)
            {
                std::ifstream f(c, std::ios::binary);
                if (f.good()) return c;
            }
        }
    }
#endif
    std::ifstream f(name, std::ios::binary);   // current working directory
    if (f.good()) return name;
    return std::string();
}

int main()
{
    // Initialize all bitboards and attack tables (silent)
    init_bitboards();

    // Initialize hash table with default 64 MB (silent)
    init_hash_table(64);

    // Resolve the NNUE nets relative to the executable so the engine works from
    // ANY working directory. A missing net used to cause a SILENT eval=0 (junk
    // moves under cutechess); now we fail loudly instead of playing garbage.
    const std::string bigNet   = resolve_net_path("nn-b1a57edbea57.nnue");
    const std::string smallNet = resolve_net_path("nn-baff1ede1f90.nnue");
    if (bigNet.empty() || smallNet.empty())
    {
        std::cerr << "FATAL: NNUE net not found. Place nn-b1a57edbea57.nnue and "
                     "nn-baff1ede1f90.nnue next to the executable (or in the "
                     "project root) and restart." << std::endl;
        return 1;
    }

    // Initialize the Stockfish HalfKAv2_hm NNUE probe (big + small nets).
    sf_init(bigNet.c_str(), smallNet.c_str());

    // Azzera i buffer in memoria allineati a 32-byte per l'AVX2
    init_policy();

    // Carica i pesi della policy, risolti relativi all'EXE (come le reti) cosi'
    // Lucas Chess li trova da qualunque working directory.
    std::string polPath = resolve_net_path("triumviratus_policy.bin");
    if (polPath.empty()) polPath = "triumviratus_policy.bin";
    if (!load_policy(polPath.c_str())) {
        printf("info string ATTENZIONE: Impossibile caricare triumviratus_policy.bin!\n");
    }

    // Run UCI loop
    uci_loop();

    return 0;
}
