// LoadGate - the ONE policy that decides what a join does with a received
// coordinated-load order (PKT_LOAD_GO), factored out of driveLoadSync so the
// unit layer (prototest) can lock it directly.
//
// WHY THIS EXISTS (title-screen stall bug, 2026-07-21):
// A join receives the host's LOAD_GO the instant it connects, but the whole
// evaluation of that GO used to run only when coop::engine::savesReady() was
// true. savesReady() reports SaveManager::gamesExist(), which at the main menu
// can stay false for minutes (the save subsystem populates lazily, and a joiner
// with no local saves of its own may never flip it until it receives one). A
// real session logged 7.5 minutes of the LOAD_GO sitting unevaluated in the
// inbound queue while the player waited at the title screen; only then did the
// join emit its NACK and pull the transfer (which itself took ~2.5 s).
//
// The fix is to separate EVALUATING a GO from LOADING on it:
//   * Fingerprinting the local copy and, on a miss, NACKing to pull the
//     transfer touch only the filesystem (savexfer::folderFingerprint walks the
//     save folder directly). They must NOT wait on the save subsystem - doing
//     so is what stalled the join.
//   * Only an IMMEDIATE match-load actually re-enters the engine's load path, so
//     only that step is gated on savesReady(); when the subsystem is not ready
//     yet the load is DEFERRED (latched) and retried once it is, instead of
//     blocking the NACK/transfer path with it.
//
// Pure inline C++03, zero game/logger/wire dependency (matches ChangeGate.h /
// EngineFaults.h / EngineCaps.h). Fingerprints and load ids are passed as plain
// unsigned int (== coop::u32 on this toolchain); the caller casts.

#ifndef KENSHICOOP_LOAD_GATE_H
#define KENSHICOOP_LOAD_GATE_H

namespace coop {
namespace sync {

// What the join should do with a single received LOAD_GO this tick.
enum LoadGoAction {
    LOADGO_SKIP_STALE,     // loadId <= newest handled: a duplicate/old GO, ignore
    LOADGO_LOAD_NOW,       // local copy matches AND the save subsystem is ready:
                           // bypass-once load into the host's world immediately
    LOADGO_DEFER_LOAD,     // local copy matches but savesReady() is false: latch
                           // the name and load it once the subsystem comes up
    LOADGO_NACK_TRANSFER   // local copy missing/diverged: NACK to pull the host's
                           // folder; NEVER gated on savesReady() (filesystem only)
};

// Decide the action for a received LOAD_GO.
//   pktLoadId    : the GO's monotonic id.
//   loadIdSeen   : newest GO id this join has already handled (0 = none).
//   hostFp       : the folder fingerprint the host announced in the GO.
//   localFp      : fingerprint of the join's on-disk copy (0 = missing folder).
//   savesReady   : does the save subsystem report it is up (SaveManager ready)?
//   forceStream  : test-only override (KENSHICOOP_FORCE_STREAM=1 on a join).
//                  When true a MATCH is deliberately demoted to NACK+transfer so
//                  a single-machine run - where both installs share
//                  %LOCALAPPDATA%\kenshi\save and would therefore always MATCH
//                  and load straight off disk - still exercises the REAL folder
//                  transfer + post-transfer load path. Default false (production).
//                  It lives HERE, inside the one policy, rather than as a second
//                  inline gate at the call site: two gates deciding the same
//                  thing is exactly how the two halves drift apart.
//
// Match requires a present local copy (localFp != 0) whose fingerprint equals
// the host's, and forceStream not armed. A match loads now if the subsystem is
// ready, else defers. Any mismatch/miss goes straight to NACK+transfer
// regardless of savesReady - that is the path the title-screen stall broke.
inline LoadGoAction decideLoadGo(unsigned int pktLoadId, unsigned int loadIdSeen,
                                 unsigned int hostFp, unsigned int localFp,
                                 bool savesReady, bool forceStream = false) {
    if (pktLoadId <= loadIdSeen)
        return LOADGO_SKIP_STALE;
    if (!forceStream && localFp != 0 && localFp == hostFp)
        return savesReady ? LOADGO_LOAD_NOW : LOADGO_DEFER_LOAD;
    return LOADGO_NACK_TRANSFER;
}

} // namespace sync
} // namespace coop

#endif // KENSHICOOP_LOAD_GATE_H
