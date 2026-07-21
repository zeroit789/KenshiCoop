# Base-building sync investigation (issue #13, Venaen: "base building does not sync at all")

Status: **WIP** — hypothesis A refuted by live evidence; real cause still open.
Binary under test: **Kenshi 1.0.65 Steam** (NOT 1.0.68 as the brief assumed).
Method: host-only runs, save `sync`/`squad1`, plugin log + static disassembly (capstone).

## Prior hypothesis A (from earlier read-only diagnosis)
`placeFinal_hook` reads `justBeenBuilt->getGameData()->stringID`, believed to be the
RUNTIME INSTANCE sid (unresolvable), so the peer's `placeBuildingAt` ->
`findItemTemplateImpl(gw, sid, BUILDING)` never finds it -> `mint template MISS` ->
building invisible on the peer, any transport.

## Live findings (this session) — hypothesis A is NOT supported
1. Hook installs fine: `[build] placeFinalPreviewBuilding detour installed`.
2. BUILDING template enumeration is HEALTHY: `g_getDataOfTypeFn(&gw->gamedata,BUILDING)`
   returns **n=604** templates (log `[build] tmpl-scan BUILDING n=604`). The
   "empty enumeration" theory is dead.
3. `findBuildTemplate` returned 0 only because it name-searches English strings
   ("camp bed", "storage") while this install is SPANISH ("Armario para Ballesta",
   "Granja de Algodon"...). NOT a networking bug — a probe fragility. A fallback
   (use first enumerated template) was added so the probe works on localized installs.
4. **`createBuilding(tmpl)` PRESERVES the template sid**: a building minted via the
   factory has `getGameData()->stringID == tmpl.stringID`. Live proof:
   `SCENARIO BUILDPLACE sid='1532622-rebirth.mod'` == `SCENARIO BUILDSITE sid='1532622-rebirth.mod'`,
   and that sid **resolves** (`findItemTemplateImpl` -> resolves=1). So a runtime
   building's getGameData is the stable template sid, NOT an unresolvable instance sid.
5. No `placeFinalPreviewBuilding` override anywhere (only the base, one vtable slot),
   so walls/beds/etc. all go through the same hooked function — no alternate path.
6. The REAL path could not be exercised programmatically: building a `PreviewBuilding`
   and calling `placeFinalPreviewBuilding()` leaves `justBeenBuilt=null`
   (`[buildreal] RESULT haveJustBuilt=0`), even with the placement-validation flags
   (0x88..0x8F) forced and `figureOutWhichTown` called. placeFinal re-runs its own
   footprint/collision verification against the world raycast, which a headless
   programmatic preview does not have. The hook therefore never fired in any test.

## Real 1.0.65 RVAs (KenshiLib header is a DIFFERENT build, off by +0x310 here)
Runtime log `[build] DIAG modBase=0x7ff641b30000 placeFinal=0x7ff6420046a0 createBuilding=0x7ff6420ac1e0`:
- `PreviewBuilding::placeFinalPreviewBuilding` RVA = **0x4D46A0** (header said 0x4D49B0)
- `RootObjectFactory::createBuilding`         RVA = **0x57C1E0** (header said 0x57C4F0)

## Static disasm of placeFinalPreviewBuilding @ 0x4D46A0 (capstone, exe on disk)
- First ~0x120 bytes = footprint-validation loops: iterate footprints, call a vtable
  method `[r10+0x190]` (collisionTestOK-like); if `al!=0` jump to 0x4D461C (reject).
  This is exactly why the programmatic preview is rejected (no real collision context).
- After validation it does `operator new(0xb0)` (call 0xED650A) + ctor (call 0x1C0E9)
  — builds some 0xB0-byte object (NOT GameData, which is 0x300).
- **`createBuilding` (0x57C1E0) is NOT called in the first 5000 bytes** of placeFinal;
  it must be reached in a later block or a sub-routine after full validation.
  Tracing HOW the GameData is passed to createBuilding (is RDX = this->buildDataPtr
  [PreviewBuilding+0xF8, the template], or a standalone copy?) is the open thread.

## Conclusion
Hypothesis A (unresolvable instance sid) is **refuted with high confidence**: the
GameData-assignment mechanism for buildings (createBuilding) keeps the resolvable
template sid, and there is no per-instance sid copy at placeFinal time. The proposed
fix (capture `self->getGameData()` instead of `justBeenBuilt->getGameData()`) would
be a no-op — both point at the same template. **Do NOT ship that fix.**

## What is still missing / next concrete steps
1. Capture the LITERAL sid `placeFinal_hook` logs from a REAL UI-placed building.
   The hook already logs `[build] LOCAL-PLACE ui=1 sid='X'`. Only a real in-game
   placement fires it. Next: launch host-only free play (save `squad1`,
   `KENSHICOOP_SCENARIO=""`, `KENSHICOOP_TEST_SECONDS=0`), drive the build UI to place
   one camp bed, read the sid. (Script ready: scratchpad `launch_freeplay.ps1`.)
2. OR finish the disasm: continue from 0x4D46A0 past 5000 bytes / into the sub-routine
   until the `call 0x57C1E0`, and inspect RDX (2nd arg = GameData) — confirm it is
   `[preview+0xF8]` (template) vs a copy.
3. If the sid is confirmed template-resolvable, the real cause of Venaen's "no sync"
   is elsewhere — candidates: (a) the hook simply does not fire for the player's real
   build flow (unknown wrapper), (b) a peer/network/gate failure unrelated to the sid,
   (c) the synthetic `build_sync` scenario masked it (it calls `createBuilding` directly
   with a known-good template, so it NEVER exercised placeFinalPreviewBuilding).

## Temporary instrumentation in this commit (REVERT once the question is closed)
- `EngineWorld.cpp findBuildTemplate`: census dump + first-template fallback.
- `EngineWorld.cpp probePlaceBuildingReal`: exercises the real placeFinal path.
- `EngineInternal.cpp installBuildHook`: `[build] DIAG` address log.
- `ScenarioBuildings.cpp BuildProbeScenario::doPlace`: calls probePlaceBuildingReal.
- Disasm helper: scratchpad `disasm_placefinal.py` (RVAs 0x4D46A0 / 0x57C1E0).

## Session 2026-07-21 (evening) — the real-UI test was INCONCLUSIVE (dead log sink)
The player (Zero) hand-built two real campfires (CAMPAMENTO) in a live host
session (pid save `squad1`, plugin build `Jul 21 2026 18:08:17`, `role=HOST`).
No `[build] LOCAL-PLACE` line appeared anywhere. BUT this is **NOT** evidence the
hook failed — the placement-log SINK was dead this run:

- `LOCAL-PLACE` (EngineInternal.cpp ~L996) and the armed-at-startup `[build] DIAG`
  (installBuildHook ~L2028) both emit via `coop::logLine` -> `writeLine` -> the
  `KENSHICOOP_LOG` file only. "detour installed" survives because it uses
  `coopLog`, which ALSO fans out to RE_Kenshi_log.txt via `DebugLog`.
- The **incondicional** startup `[build] DIAG` line is ALSO absent -> proof the
  logLine/writeLine channel is a no-op this run (`g_fp == 0`), independent of any
  placement. Cause: this session was launched WITHOUT the harness, so
  `KENSHICOOP_LOG` was unset; the default relative `KenshiCoop_host.log`
  (Config.cpp:116) never opened a file (no `log opened` line exists for 18:xx in
  any location — root, `RE_Kenshi\`, `mods\`, LOCALAPPDATA). A healthy writeLine
  always writes `log opened` first (CoopLog.cpp:71).
- What DID confirm: plugin alive, host, buildSync ON, detour installed at 3.202s
  (all in RE_Kenshi_log.txt via the DebugLog fan-out).

### Exact next step (do NOT relaunch without this)
Relaunch host with `KENSHICOOP_LOG` set to an ABSOLUTE writable path (this is what
`friend_host.ps1` L301 does and the ad-hoc launch skipped), auto-load `squad1`,
`KENSHICOOP_SCENARIO=""`, `KENSHICOOP_TEST_SECONDS=0`, via `scripts\start_kenshi.ps1`.
Then have the player place ONE campfire and read that file:
- `[build] DIAG ...` present => channel healthy; the placement verdict is now real.
- `LOCAL-PLACE sid='X'` present => hook fires on the real player flow; capture the
  literal sid (that is the datum this whole investigation is chasing).
- DIAG present but LOCAL-PLACE absent after a real placement => HARD evidence the
  detour (RVA 0x4D46A0) is NOT the function the player's build-commit runs through.
  Next: attach x64dbg/CE to the live process, breakpoint modBase+0x4D46A0, place a
  campfire; if BP never hits, the RVA/function is wrong (KenshiLib header
  `_NV_placeFinalPreviewBuilding` resolves to a wrong-build RVA) — hunt the real
  function by the build-UI button flow or a `[PreviewBuilding::` debug string xref.
Session left untouched at Zero's request (he went to play co-op); nothing recompiled.
