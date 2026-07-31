// JailAnchor.h - captive furniture-kind conflict policy (pure, zero
// game/Win32 deps).
//
// A caged prisoner is often ALSO shackled: the owner's reliable furniture
// edges settle on the cage (publish kind priority bed=1 > cage=2 > chained=3)
// while the lossy continuous bodyState batch can still say CHAINED-only, so
// the driven copy sees streamKind=3 against an edge-vouched localKind=2. The
// kind=3 self-heal used to resolve that by BREAKING the cage and re-chaining
// every FURN_HEAL_MS - a median 75-88 u (tail 885 u) re-seat teleport on
// 10-15 bodies per session (spike 58, findings 1-2).
//
// The rule (spike 58 follow-up 1): while a RELIABLE edge vouches the local
// cage/bed, the cage/bed is the transform anchor and the shackle is an
// EQUIP-only state - never break the anchor over a continuous-bit
// disagreement. A local cage NO edge vouches for is a stale attach at the
// wrong spot (the Flashbox case) and must still be broken and re-chained.
// Tested in src/prototest/main.cpp (testJailAnchor); used by
// Replicator::applyTargets in ReplicatorDrive.cpp.

#ifndef COOP_JAIL_ANCHOR_H
#define COOP_JAIL_ANCHOR_H

namespace coop {

enum ChainAnchorAct {
    CHAIN_ANCHOR_NONE    = 0, // stream not chained, or already chained locally
    CHAIN_ANCHOR_HOLD    = 1, // edge-vouched cage/bed anchors the transform;
                              //   re-assert the chain EQUIP-only, never break
    CHAIN_ANCHOR_RECHAIN = 2  // no/stale local furniture: existing heal path
                              //   (break an unvouched cage/bed, re-chain)
};

// One decision for one driven body per tick. streamKind = what the owner's
// continuous batch reports (0 none / 1 bed / 2 cage / 3 chained); localKind =
// where our copy actually sits (readFurniture, 0 when absent/invalid);
// edgeKind = the furniture kind last vouched for this body by a RELIABLE
// edge (RECV ENTER / host PEER-ENTER author; 0 = none, cleared on EXIT).
inline ChainAnchorAct chainAnchorStep(int streamKind, int localKind,
                                      int edgeKind) {
    if (streamKind != 3) return CHAIN_ANCHOR_NONE; // conflict is kind-3 only
    if (localKind == 3)  return CHAIN_ANCHOR_NONE; // already chained: in sync
    if ((localKind == 1 || localKind == 2) && edgeKind == localKind)
        return CHAIN_ANCHOR_HOLD;                  // vouched anchor wins
    return CHAIN_ANCHOR_RECHAIN;                   // stale/absent: heal as before
}

// ---------------------------------------------------------------------------
// Third-party placement retention (protocol 36).
//
// A body this client's world sim put into a bed/cage, but whose OWNER is a
// peer, streams back NO occupancy bit (the owner's engine never ran the
// action), so the debounced HEAL EXIT in applyTargets ejects it after
// FURN_EXIT_MS. Protocol 36 suppresses that exit and authors the ENTER for
// the owner instead - but it used to key that authority off `downish` (the
// occupant is KO'd/dead) read FRESH every tick, which breaks twice:
//
//   * MED BED (kind 1): the bed heals the KO faster than the 3 s exit
//     debounce, so downish flips false while the body is still in the bed and
//     the very next tick starts the debounce that ejects it - the host's own
//     sim re-beds it, and the occupant bounces every ~3 s.
//   * CAGE (kind 2): a guard arresting a CONSCIOUS squad member (a player who
//     surrendered rather than being knocked out) is downish=false from the
//     first tick, so the retention never engaged at all and the prisoner was
//     ejected from the cage every 3 s.
//
// Both are the same guard, so both take the same rule: retention follows the
// PLACEMENT, not the instantaneous body state.
//   - A CAGE is never entered voluntarily in Kenshi (imprisonment is always
//     someone else's action), so a peer-owned squad body sitting in a local
//     cage is host-authored by construction - hold it unconditionally.
//   - A BED is voluntary (a driven copy's own AI can bed itself while the
//     owner walks around, which must still be ejected), so a bed still needs
//     the KO to claim it - but once claimed the claim LATCHES (peerHeldKind),
//     surviving the heal that clears downish.
// The latch is dropped when the local copy actually leaves the furniture, or
// when the debounced exit fires, so a later voluntary bed pose is judged
// fresh. Tested in src/prototest/main.cpp (testPeerFurnHold); used by
// Replicator::applyTargets in ReplicatorDrive.cpp.
enum PeerFurnAct {
    PEER_FURN_RELEASE = 0, // no host claim on this seat: run the debounced exit
    PEER_FURN_HOLD    = 1  // host-authored placement: hold the seat, author the
                           //   ENTER for the owner, never self-heal-eject
};

// localKind    = where our copy actually sits (readFurniture; 1 bed / 2 cage,
//                anything else is not a transform anchor here).
// isSquad      = the occupant belongs to a player squad (world NPCs keep the
//                pre-existing owner-authored behaviour).
// downish      = the occupant reads KO'd/dead right now (streamed bodyState,
//                the reliable KO/death latch, or the local copy's own read).
// peerHeldKind = the furniture kind THIS client already claimed for this body
//                (0 = none) - the latch that survives a med bed's fast heal.
inline PeerFurnAct peerFurnStep(int localKind, bool isSquad, bool downish,
                                int peerHeldKind) {
    if (!isSquad) return PEER_FURN_RELEASE;              // world NPCs unchanged
    if (localKind != 1 && localKind != 2)
        return PEER_FURN_RELEASE;                        // not a bed/cage anchor
    if (localKind == 2) return PEER_FURN_HOLD;           // a cage is never voluntary
    if (downish) return PEER_FURN_HOLD;                  // KO'd body laid in a bed
    if (peerHeldKind == localKind) return PEER_FURN_HOLD; // latched across the heal
    return PEER_FURN_RELEASE;                            // conscious voluntary bed
}

} // namespace coop

#endif // COOP_JAIL_ANCHOR_H
