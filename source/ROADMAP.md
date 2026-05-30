# Triumviratus — Stato & Roadmap

Documento unico di **stato attuale**, **storico migliorie misurate** e **roadmap**.
Filosofia: ogni modifica è uno step isolato dietro un toggle, validato matematicamente
(SPRT per i cambi di ricerca, A/B NPS per i cambi di velocità). Niente si committa
"a sensazione".

Ultimo aggiornamento: 2026-05-30

---

## 1. Stato attuale

- **Versione:** Triumviratus 3.3.2 (MSBuild Release|x64, MSVC v143, AVX2).
- **Forza stimata:** ~3450–3470 Elo (3.3.1 era ~3430-3450; +~18 da improving, +~34 da
  singular-ext rispetto alla 3.3.1; node-TM ~0). Stima grossolana, da ri-ancorare.
- **Eval:** NNUE ibrida (feature transformer + affine), incrementale + dual-net lazy.
- **Ricerca:** alpha-beta PVS, ABDADA SMP (shared TT + busy-bit + depth-staggering),
  improving heuristic, singular ext avanzate (double/negative), node-based TM, Syzygy.
- **Toggle UCI diagnostici/A-B:** `EvalCache` (on), `Improving` (on), `NodeTM` (on),
  `SingularExt` (on), `UsePolicy` (off), `EvalOff` (profiling).

### Note di profiling (riferimento)
- Eval NNUE = ~60% del tempo/nodo. eval-OFF ≈ 2.3M nps, eval-ON ≈ 0.9M nps (1 thread).
- Collo di bottiglia eval = **feature transformer/accumulatore**, non i layer affine.
- Gap NPS ~3-4x vs Stockfish = **architetturale (rete)**, NON compilatore né movegen.
- Scaling ABDADA (8845HS, 8c/16t): 2t 93% · 4t 91% · 8t 67% · 16t 49% (SMT).

---

## 2. Storico migliorie (misurate)

| Data | Modifica | Tipo | Esito |
|------|----------|------|-------|
| 2026-05 | **Static-eval cache** per-thread (memoizzazione eval, key=hash^fifty) | velocità | **+3% NPS**, sicuro (gioco identico). Tenuto, def on (`EvalCache`). |
| 2026-05 | **Improving heuristic** (RFP/futility/LMR modulati dal trend eval) | ricerca | **Positiva confermata** (LOS 98%) @3+0.1, 508 partite. Stima centrale +17.8 ma con IC95% ampio ≈ [+1, +35] → la *grandezza* è ancora rumorosa (~+10/+18 realistico). Da rifinire con più partite + conferma a 8+0.08. Toggle `Improving`. |
| 2026-05 | **Node-Based Time Management** (`NodeTM`) | tempo | **Neutra** (~+3 ±14, LOS 64%). Non dannosa, reduce-only. Tenuta on. NB: prima versione aveva un bug grave (scalava il timestamp assoluto invece della durata → perdeva a tempo / depth-6); risolto scalando `soft - starttime`. |
| 2026-05 (v3.3.2) | **Singular Extensions avanzate** (Double +2 / Negative −1) (`SingularExt`) | ricerca | **+34.1 ±23.9 Elo, LOS 99.7%** @3+0.1, 235 partite (sopra node-TM). Lower bound +10 → successo netto. Il lever più grosso finora. |
| 2026-05 | **Correction history** (pawn-key bucket, `CorrHist`) | ricerca | **Neutra** → **default OFF**. Versione aggressiva (cap 48) = −12 Elo; versione gentile (cap 16, learn /512) ≈ 0 (±17). Codice tenuto dormiente per un retry con SPSA. |
| 2026-05 | Syzygy tablebases (Fathom, WDL+DTZ) | correttezza | Finali corretti. |

### Vicoli ciechi (NON riprovare)
- **VNNI / AVX-512** (`/arch:AVX512`): −7%. Zen4 double-pumpa il 512-bit; VNNI aiuta
  solo i layer affine (la parte minore).
- **clang-cl 19** (AVX2 e znver4): pareggio/leggera regressione vs MSVC. MSVC già ottimale.
- **Policy CNN in ricerca** (root ordering / interior / root-LMR): da neutro a −85 Elo.
  Disabilitata (`UsePolicy` off). Per riprovare servirebbe int8/rete più piccola.

---

## 3. Roadmap — ordinata per priorità (alto impatto + bassa difficoltà in cima)

Legenda:
- **Elo**: guadagno atteso (stima grossolana, da confermare con SPRT).
- **Diff**: difficoltà implementazione+test. ★ = facile, ★★ = media, ★★★ = alta.
- **F**: fase originale (1 ricerca · 2 architettura · 3 rete).

| # | Voce | Elo | Diff | F | Note |
|---|------|-----|------|---|------|
| 1 | **Improving su LMP + null-move** | +2..8 | ★ | 1 | RIMANDATA: implementata poi tolta (troppo piccola da isolare). Da riprovare singola. |
| 2 | ~~Node-Based Time Management~~ | +5..15 | ★ | 1 | **FATTO** (v3.3.2), neutra ~+3. Toggle `NodeTM`. |
| 3 | **Aspiration window tuning** | +2..8 | ★ | 1 | Larghezza iniziale + crescita. Solo costanti da SPRT. |
| 4 | **SPSA tuning dei margini** (RFP/futility/razor/LMR) | +5..15 | ★★ | 1 | Setup iniziale, poi sforna Elo "gratis" su parametri esistenti. Abilita tutto il resto. |
| 5 | ~~Singular Extensions avanzate~~ (Double + Negative) | +10..20 | ★★ | 1 | **FATTO** (v3.3.2): +34 Elo, LOS 99.7%. Toggle `SingularExt`. |
| 6 | **Correction History** | +10..20 | ★★ | 1 | Corregge le strutture che la rete sovra/sotto-stima sistematicamente. Alto impatto. |
| 7 | **ProbCut** | +5..12 | ★★ | 1 | Taglio con ricerca shallow sopra beta + margine. Classico +Elo AB. |
| 8 | **History gravity/aging + continuation nel pruning** | +5..15 | ★★ | 1 | Raffina riduzioni/potature con storia 1-2 ply. |
| 9 | **Address Sanitizer (`/fsanitize=address`)** | ~0 | ★ | 2 | Non dà Elo ma stana il crash ~1/400 della GUI. Alto valore per release/tornei. Fallo presto. |
| 10 | **TT bucketizzata (4-way) + aging migliore** | +3..10 | ★★ | 2 | Meno collisioni/thrashing. |
| 11 | **Static eval nella TT vera** | +0..3 | ★★ | 1 | Estende eval/improving cross-thread. Poco Elo (cache per-thread già prende il grosso). |
| 12 | **Pulizia codice morto** (policy, unused) | 0 | ★ | 2 | Manutenibilità, non Elo. |
| 13 | **Ottimizzazione movegen** | +NPS | ★★★ | 2 | Limare cicli. Guadagno piccolo (movegen non è il collo di bottiglia). |
| 14 | **Lazy SMP** (rimuovere ABDADA) | +? alto core | ★★★ | 2 | Paga ad alto core count; a ≤8 core ABDADA già scala bene → misurare il guadagno reale PRIMA. Grosso refactor. |
| 15 | **Self-play data gen** | 0 (enabler) | ★★ | 3 | Logging FEN+score. Prerequisito per la rete propria. |
| 16 | **Quantizzazione int8 / rete più piccola** | +NPS | ★★★ | 3 | Abbatte il vero collo di bottiglia (eval ~60%). Può far rivivere la policy. |
| 17 | **Training rete custom** (bullet/PyTorch) | −poi+ | ★★★ | 3 | Calo iniziale fisiologico, poi indipendenza totale. Progetto lungo. |
| 18 | **Sperimentare architettura rete** (bucket/dimensione) | ±? | ★★★ | 3 | Forza vs velocità. Dopo aver consolidato il pipeline di training. |

**Suggerimento di esecuzione:** punti 1→3 sono "quick win" da fare subito (facili, infra
pronta). Poi 4 (SPSA) perché rende automatici i tuning di tutti gli altri. Poi i due grossi
lever di forza, 5 (singular avanzate) e 6 (correction history). Il 9 (ASAN) infilalo appena
hai un'oretta: non è Elo ma toglie il crash che imbarazza in torneo.

---

## 4. Metodo di test (promemoria)

- **Cambi di ricerca → SPRT** in cutechess, stesso exe con toggle UCI:
  `option.Improving=true` vs `false`. Parametri tipici `elo0=0 elo1=5 alpha=0.05 beta=0.05`.
- **Cambi di velocità → A/B NPS** interlacciato (`eval_ab.ps1`): mediana cache/feature
  OFF vs ON nello stesso run (immune al rumore ~10% run-to-run dei singoli go).
- **TC:** 3+0.1 per volume veloce; **riconfermare a 8+0.08** prima di committare i
  cambi di pruning (a TC corto possono ingannare).
- **Anchor Elo:** ancorare i tornei a ≥3 avversari con rating noto affidabile; diffidare
  degli outlier (es. prune sotto-performa il suo anchor).
