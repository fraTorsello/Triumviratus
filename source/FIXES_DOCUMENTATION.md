# Triumviratus Chess Engine - Multi-threading Fixes

> **Architecture note.** Triumviratus' parallel search is **ABDADA** (Alpha-Beta
> Distributed Avoiding Duplicate Analysis): all threads share a single
> transposition table and mark in-progress nodes as *busy* so that helpers avoid
> re-searching nodes another thread is already on, with light depth-staggering
> across helpers (`start_depth = 1 + thread_id % 2`) purely for hash diversity.
> It is **not** Lazy SMP. Where this document refers to "Lazy SMP" it is either for
> contrast (explaining what ABDADA improves on) or historical context — several of
> the fixes below predate the move to ABDADA. The relevant code lives in a single
> `threads.cpp` / `tt.h` (the `*_new` filenames mentioned in the Integration Guide
> are historical).

## Summary of Issues Found and Fixed

### 1. **Race Conditions in Transposition Table (CRITICAL)**

**Problem:** The original `tt` structure and access functions have no synchronization. Multiple threads reading and writing simultaneously causes:
- Torn reads (reading partial writes from another thread)
- Data corruption
- Incorrect hash entries leading to wrong evaluations

**Solution:** Implemented lockless hash table using XOR verification technique:
```cpp
// Store: entry->key = hash_key ^ data
// Read: verify (stored_key ^ data) == hash_key
```

This is the same technique used by Stockfish. If a torn read occurs, the XOR verification will fail and we reject the entry.

**Files changed:** `tt_new.h`, `tt_new.cpp`

---

### 2. **Missing Fifty-Move Counter in FEN Parsing**

**Problem:** `parse_fen()` doesn't parse the halfmove clock (fifth field in FEN), so `fifty` is always 0 after loading a position. This causes:
- Fifty-move rule never triggers
- Games that should be draws continue playing

**Solution:** Updated `parse_fen()` to properly parse the halfmove clock:
```cpp
// After parsing en passant square, continue to parse fifty-move counter
if (*fen == ' ') {
    fen++;
    if (*fen >= '0' && *fen <= '9') {
        fifty = atoi(fen);
        while (*fen >= '0' && *fen <= '9') fen++;
    }
}
```

**Files changed:** `io_new.cpp`

---

### 3. **Dual Stop Mechanism Confusion**

**Problem:** The code uses both:
- `stopped` global variable (set by time management)
- `stop_threads` atomic (set for thread coordination)

Different parts of the code check different variables, causing threads to not stop properly.

**Solution:** Unified the stop mechanism:
- `stop_threads` atomic is the single source of truth for search termination
- Time management sets `stop_threads` when time expires
- All threads check `stop_threads` in a consistent manner

**Files changed:** `threads_new.cpp`

---

### 4. **Thread Result Aggregation Missing**

**Problem:** Under ABDADA, helper threads search the same position (with light depth-staggering) and share hash table entries through the common TT. However, the original code:
- Only uses main thread's result
- Never checks if helper threads found a better move

**Solution:** After search completes, compare results from all threads and use the best one:
```cpp
for (int i = 1; i < num_threads; i++) {
    if (thread_data[i].completed_depth >= best_depth && 
        thread_data[i].best_score > best_score) {
        best_move = thread_data[i].best_move;
        best_score = thread_data[i].best_score;
    }
}
```

**Files changed:** `threads_new.cpp`

---

### 5. **Aspiration Window Retry Bug**

**Problem:** When aspiration fails:
```cpp
if ((score <= alpha) || (score >= beta)) {
    alpha = -infinity;
    beta = infinity;
    current_depth--;  // Bug: loop also does current_depth++
    continue;
}
```
This works but is confusing. More importantly, after re-searching with full window, the code might output incomplete info.

**Solution:** Clearer logic that ensures we don't output info until a depth is fully completed with valid bounds.

**Files changed:** `threads_new.cpp`

---

### 6. **Helper Thread Depth Distribution**

**Problem:** All threads searching the same depth provides less diversity. SMP search (ABDADA included) benefits from threads searching at slightly different depths.

**Solution:** Helper threads start at different depths:
```cpp
int start_depth = 1 + (thread_id % 3);
```

This creates more hash table entries at various depths, which helps all threads.

**Files changed:** `threads_new.cpp`

---

### 7. **History Table Overflow**

**Problem:** History moves are incremented by `depth` each cutoff:
```cpp
history_moves[piece][square] += depth;
```
Over time, this can overflow or create huge disparities.

**Solution:** Use `depth * depth` with a maximum cap, and consider periodic aging.

**Files changed:** `threads_new.cpp`

---

### 8. **Null Move Reduction Too Aggressive**

**Problem:** Fixed R=2 reduction for null move pruning is not adaptive.

**Solution:** Dynamic reduction based on depth:
```cpp
int R = 2 + depth / 4;
```

**Files changed:** `threads_new.cpp`

---

### 9. **Late Move Reductions Not Aggressive Enough**

**Problem:** Original LMR only reduces by 2 plies regardless of move count or depth.

**Solution:** Progressive LMR:
```cpp
int reduction = 1;
if (moves_searched >= 6) reduction = 2;
if (moves_searched >= 12 && depth >= 6) reduction = 3;
```

**Files changed:** `threads_new.cpp`

---

## Version 3.3 Updates

### 10. **Syzygy Endgame Tablebases (NEW)**

**Added:** Endgame tablebase probing via [Fathom](https://github.com/jdart1/Fathom).
- **In-search WDL probe** so the search never misjudges a covered (≤ TB_LARGEST men) endgame.
- **Root DTZ probe** that returns the DTZ-optimal move, so the engine converts wins / holds draws with perfect technique.
- Coordinate bridge engine (a8=0) ↔ Fathom (a1=0) via vertical byte-swap and `sq ^ 56`; the root move is always re-matched against the generated move list, so an unmatched TB result is never played.
- Tables auto-load from a `Syzygy/` folder next to the executable, or from the `SyzygyPath` UCI option.

**Files changed:** `syzygy.cpp`, `syzygy.h`, `fathom/*`, `threads.cpp`, `uci_mt.cpp`, `main.cpp`

---

### 11. **Illegal-position / King-capture Crash Guard (CRITICAL)**

**Problem:** On an illegal position parsed from FEN where the side-not-to-move is in check, pseudo-legal generation can produce a king-capturing move. `td_make_move` accepts it (the mover's own king is safe), the NNUE mirror then loses the enemy king, and the next evaluation indexes a king-bucket feature for a now-kingless side → access violation (0xC0000005).

**Solution:** Guard the entry of `td_negamax_abdada` and `td_quiescence`: if the side-to-move can capture the opponent's king (i.e. the side-not-to-move is in check), return an immediate winning score instead of searching. In legal play this never fires (costs one attack probe per node).

**Files changed:** `threads.cpp`

---

### 12. **History-based LMR Reverted**

**Change:** The combined butterfly + continuation-history LMR modifier (divisor 4000) was reverted to the conservative butterfly-only `±1`-ply clamp (divisor 3500) after match testing showed the combined variant was not an Elo gain.

**Files changed:** `threads.cpp`

---

## Integration Guide

To use these fixes, replace the following files:

1. Replace `tt.h` → `tt_new.h`
2. Replace `tt.cpp` → `tt_new.cpp`
3. Replace `threads.h` → `threads_new.h`
4. Replace `threads.cpp` → `threads_new.cpp`
5. Replace `io.cpp` → `io_new.cpp`
6. Replace `see.cpp` → `see_new.cpp`
7. Replace `uci_mt.cpp` → `uci_new.cpp`

Update includes in other files as needed.

---

## Performance Expectations

With these fixes, you should see:

1. **Correct multi-threaded behavior** - No crashes or corruption
2. **Linear scaling to ~4 threads** - Diminishing returns after that
3. **~5-10% Elo gain** per doubling of threads (typical for shared-TT SMP)
4. **Correct fifty-move detection** - Draws properly detected

---

## Testing Recommendations

1. **Perft testing** - Verify move generation still works
2. **STS (Strategic Test Suite)** - Compare single vs multi-thread scores
3. **Cutechess** tournament - Play games against other engines
4. **Long time control games** - Check for crashes/hangs

---

## Remaining Improvements (Not Implemented)

1. **Shared killers across threads** - Can improve cutoffs
2. **Contempt factor** - For avoiding draws when ahead
3. **Better time management** - Pondering, sudden death handling
4. **Singular extensions** - The `excluded_move` parameter exists but is not yet exploited

> Note: ABDADA parallel search and Syzygy tablebase support (listed here historically)
> are now implemented — see "Version 3.3 Updates" above.
