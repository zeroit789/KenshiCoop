// HealForward - the treatment-forward (first aid over the network) policy for
// the medical channel's rise detector.
//
// The detector lives in Replicator::applyMedical (ReplicatorChannels.cpp): on a
// DRIVEN copy, a local bandaging level that has risen above what the owner last
// published means first aid was administered on THIS machine, so the resulting
// levels are forwarded to the owner (reliable, raise-only on apply).
//
// Why the pace matters. applyMedical calls engine::writeMedical, which
// OVERWRITES the driven copy's bandaging with the owner's value on every
// snapshot - and owned members publish at up to 2.5 Hz (MIN_SEND_MS = 400).
// So the healer's locally accrued progress is reset to the owner's level about
// every 400 ms. Whatever the detector fails to forward before the next snapshot
// is simply lost. With the original 1 Hz forward throttle only the last <= 400 ms
// of each second ever reached the owner: a ~40 s first-aid job crawled to ~95 s
// in ~1-unit steps (field report 2026-07-31). The forward pace therefore has to
// be a good deal faster than the snapshot pace, not slower - hence a 100 ms
// throttle and an epsilon well under one bandage unit.
//
// Why relaxing it is safe. engine::applyBandageParts on the authority is
// RAISE-ONLY and idempotent, so a duplicate, out-of-order or slightly optimistic
// forward can never lower a bandage or undo the owner's own first aid. The cost
// of forwarding more eagerly is bounded reliable traffic (<= 1 packet per
// throttle window per body, and only while a bandage is actually rising); the
// cost of forwarding late is lost healing.
//
// Pure inline C++03, zero game/logger/wire dependency (matches ChangeGate.h /
// CarriedHeal.h) so the unit layer (prototest, testHealForward) locks the policy
// directly.

#ifndef KENSHICOOP_HEAL_FORWARD_H
#define KENSHICOOP_HEAL_FORWARD_H

namespace coop {
namespace sync {

// Forward pace. 100 ms (10 Hz ceiling) sits comfortably under the 400 ms
// snapshot clobber, so at most ~100 ms of accrued bandaging is ever lost to a
// snapshot; the rise gate below keeps the channel silent when nothing is being
// treated, so this is a ceiling, not a rate.
const unsigned long HEAL_FWD_THROTTLE_MS = 100;

// Minimum rise (bandage units) that counts as first aid administered here.
// Only has to clear float noise: the compare is against a value this machine
// wrote itself from the owner's stream, so there is no cross-machine rounding
// to absorb. The old 0.5 was one medicalHash quantization bucket, which at the
// engine's ~2.6 units/s bandage rate discarded up to ~190 ms of progress per
// snapshot window on top of the throttle.
const float HEAL_RISE_EPS = 0.05f;

// What the detector can compare a part's local bandaging against this pass.
enum HealBaseline {
    HEAL_BASE_NONE  = 0, // no baseline and none can be formed - skip this part
    HEAL_BASE_SEED  = 1, // no baseline yet - adopt the local level as one, no send
    HEAL_BASE_READY = 2  // a usable baseline exists - run the rise test
};

// BASELINE selection for one anatomy part.
//   local         : this copy's current bandaging (< 0 = the part is absent /
//                   unreadable here).
//   recvBand      : the baseline held for this part (< 0 = none held).
//   haveSnapshot  : an owner medical snapshot has been applied to this body.
//
// With a snapshot in hand, recvBand < 0 means the OWNER has no such part (the
// packet carried used = 0, or its anatomy is shorter than ours) - forwarding
// there is pointless because applyBandageParts would find no part to raise, so
// the part is skipped exactly as before.
//
// WITHOUT a snapshot the body is a "pristine copy" to us: we are driving it but
// its owner has not published medical yet (a peer squad member in the first
// seconds of a session, before the 3 s safety resend). The old detector had no
// baseline at all there and refused to forward, so first aid started in that
// window was thrown away and the treatment only began once the owner's snapshot
// landed. Seeding the baseline from the CURRENT local level instead arms the
// detector immediately: a seed can never manufacture a treatment (it is by
// construction equal to the level it will be compared against), it only makes
// the NEXT rise forwardable.
inline HealBaseline healBaselineFor(float local, float recvBand,
                                    bool haveSnapshot) {
    if (local < 0.0f) return HEAL_BASE_NONE;   // nothing to bandage here
    if (recvBand >= 0.0f) return HEAL_BASE_READY;
    return haveSnapshot ? HEAL_BASE_NONE       // owner has no such part
                        : HEAL_BASE_SEED;      // pristine copy - seed from local
}

// FORWARD PACE gate (per body). True when a forward may go out this pass: the
// very first one always may (lastFwdMs == 0), then at most once per throttleMs.
// Unsigned subtraction tolerates the clock's midnight wrap within one window.
inline bool healForwardDue(unsigned long nowMs, unsigned long lastFwdMs,
                           unsigned long throttleMs) {
    return lastFwdMs == 0 || (nowMs - lastFwdMs) >= throttleMs;
}

// RISE test for one part. local must clear BOTH the baseline (what we believe
// the owner has) and sentBand (what we already forwarded and is still in
// flight, < 0 = nothing outstanding), so an unacknowledged rise is not re-sent
// every window while the owner's echo travels.
inline bool healPartRose(float local, float baseline, float sentBand,
                         float eps) {
    if (local <= baseline + eps) return false;
    return sentBand < 0.0f || local > sentBand + eps;
}

} // namespace sync
} // namespace coop

#endif // KENSHICOOP_HEAL_FORWARD_H
