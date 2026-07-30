# KenshiCoop wire-protocol history

`src/netproto/Wire.h` names this file as the source of truth for the protocol
but the file never existed. This is it.

Two rules:

1. **Every packet tag is permanent.** Once a `PKT_*` number has shipped in a
   build somebody played on, it is spent. Reusing it is not a compile error and
   not a runtime error - it is silent data corruption (see the 2026-07-31 entry).
2. **Every semantic change to an existing packet needs a `PROTOCOL_VERSION`
   bump**, even when the tag, the struct and its size are all unchanged. The
   handshake has nothing else to work with.

## How the version is actually enforced

`const u16 PROTOCOL_VERSION` in `src/netproto/Wire.h` is compared for **exact
equality** and nothing else:

- `src/plugin/net/NetLink.cpp` ~513 (host validating a client `PKT_HELLO`) and
  ~571 (client validating the host `PKT_WELCOME`)
- `src/plugin/net/SteamInvite.cpp` ~285 (Steam lobby-side pre-check)

There is no build hash, no capability negotiation, no per-channel version and no
back-compat path. A build either matches exactly or is rejected at handshake.

That is a feature, not a gap: it is the only thing standing between two
divergent builds and a silently corrupted session. Every packet decode after the
handshake trusts it - `readPacket` validates `len >= sizeof(T)`, a **minimum**,
so a payload that is the wrong shape but long enough is reinterpreted without a
single error.

## Packet tags in use

Tag numbers are the first byte of every packet. "Introduced" is the
`PROTOCOL_VERSION` the packet first shipped under, as recorded in the `Wire.h`
comment on the enum entry itself; the earliest packets predate per-packet
version comments and are listed by their original phase label.

| Tag | Symbol | Introduced | Direction / role |
|----:|--------|-----------|------------------|
| 1 | `PKT_HELLO` | initial | client -> host: version + name |
| 2 | `PKT_WELCOME` | initial | host -> client: version echo + playerId |
| 3 | `PKT_LEAVE` | initial | net thread -> game thread: a peer left |
| 4 | `PKT_ENTITY_BATCH` | initial | both: owner-tagged EntityState batch, 20 Hz |
| 5 | `PKT_EVENT` | initial | both, RELIABLE: one-shot transitions (KO/death/revive/...) |
| 6 | `PKT_INV_SNAPSHOT` | Phase 4a | RELIABLE container-contents snapshot |
| 7 | `PKT_WORLD_ITEM` | Phase W1 | RELIABLE world-item snapshot |
| 8 | `PKT_WORLD_ITEM_REMOVE` | Phase W1 | RELIABLE world-item cull by netId |
| 9 | `PKT_WORLD_DROP` | Phase W2 | RELIABLE conservation drop intent |
| 10 | `PKT_WORLD_PICKUP` | Phase W3 | RELIABLE conservation pickup intent |
| 11 | `PKT_TIME_PING` | pre-17 | UNRELIABLE wall-clock probe, join -> host |
| 12 | `PKT_TIME_PONG` | pre-17 | UNRELIABLE wall-clock echo, host -> join |
| 13 | `PKT_MEDICAL` | Phase 2 (combat + medical) | RELIABLE owner-authoritative vitals snapshot |
| 14 | `PKT_TREATMENT` | Phase 2 (combat + medical) | RELIABLE first-aid-on-a-driven-copy delta |
| 15 | `PKT_SPEED_REQ` | pre-17 | RELIABLE game-speed request, join -> host |
| 16 | `PKT_SPEED_SET` | pre-17 | RELIABLE arbitrated speed, host -> join |
| 17 | `PKT_STATS` | 17 | RELIABLE owner-authoritative CharStats snapshot |
| 18 | `PKT_STEALTH` | 20 | UNRELIABLE detection-map snapshot, host -> owner |
| 19 | `PKT_SPAWN_REQ` | 21 | RELIABLE unresolved-hand query, join -> host |
| 20 | `PKT_SPAWN_INFO` | 21 | RELIABLE runtime-spawn description, host -> join |
| 21 | `PKT_MONEY` | 22, redefined at 22b | RELIABLE wallet row - **see the semantics note below** |
| 22 | `PKT_FACTION` | 24 | RELIABLE player-faction relation row (symmetric) |
| 23 | `PKT_TIME` | 25 | RELIABLE host-authoritative game clock |
| 24 | `PKT_DOOR` | 26 | RELIABLE baked-door open/lock row |
| 25 | `PKT_BUILD_PLACE` | 27 | RELIABLE placed-building describe/mint |
| 26 | `PKT_BUILD_STATE` | 27 | RELIABLE placer-authoritative construction progress |
| 27 | `PKT_BUILD_DOOR` | 28 | RELIABLE placed-building door row, translated key |
| 28 | `PKT_BUILD_REMOVE` | 28 | RELIABLE placer-authoritative building removal |
| 29 | `PKT_SAVE_REQ` | 31 | RELIABLE join save request |
| 30 | `PKT_SAVE_BEGIN` | 31 | RELIABLE save-transfer announce, host -> join |
| 31 | `PKT_SAVE_FILE` | 31 | RELIABLE save-file chunk, host -> join |
| 32 | `PKT_SAVE_DONE` | 31 | RELIABLE save-transfer CRC table, host -> join |
| 33 | `PKT_SAVE_ACK` | 31 | RELIABLE commit acknowledgement, join -> host |
| 34 | `PKT_LOAD_GO` | 32 | RELIABLE coordinated-load order, host -> join |
| 35 | `PKT_LOAD_REQ` | 32 | RELIABLE join load request |
| 36 | `PKT_LOAD_NACK` | 32 | RELIABLE join copy missing/diverged, join -> host |
| 37 | `PKT_PROD` | 33 | RELIABLE per-object machine state row |
| 38 | `PKT_NPC_CENSUS` | 36 | RELIABLE wide-radius NPC existence list |
| 39 | `PKT_INV_XFER` | 37 | RELIABLE cross-owner transfer intent |
| 40 | `PKT_RESEARCH` | 38 | RELIABLE known-research row (grow-only union) |
| 41 | `PKT_CAM_HINT` | 43 | UNRELIABLE join camera center hint, join -> host |
| 42 | `PKT_COMBAT_HIT` | 45 (upstream) | RELIABLE join-dealt damage report, join -> host |
| 43 | `PKT_BOUNTY` | 45 as tag 42, retagged to 43 at 46 | RELIABLE host-authoritative bounty/crime row |

Next free tag: **44**.

## 2026-07-31 - the upstream merge collision (protocol 46)

Merging `nhoral/KenshiCoop` main (`5d62e0a`, tagged v0.46) into this fork
surfaced **two** collisions that had both been live under the *same*
`PROTOCOL_VERSION = 45`. They were created independently: this fork and upstream
each shipped a v45 build, neither knowing what the other had spent.

### Collision 1 - tag 42 meant two different packets

| | this fork | nhoral/main |
|---|---|---|
| tag 42 | `PKT_BOUNTY` | `PKT_COMBAT_HIT` |
| struct | `BountyPacket` | `CombatHitPacket` |
| size | 86 bytes | 37 bytes |
| direction | host -> clients | join -> host |

Both live inside the same `#pragma pack(push,1)` block, so the bytes line up
with no padding to disagree about. `readPacket` only checks
`len >= sizeof(T)` - a **minimum**, not an exact match - so a `BountyPacket`
(86 B) arriving at a peer that expects `CombatHitPacket` (37 B) passes the
length check and is decoded as a combat hit. The traced worst case: bytes from
the faction stringID land on the `flesh` / `blood` float fields, and the victim
goes from healthy to zero HP with no error logged anywhere.

**Fix:** `PKT_BOUNTY` moved to tag **43**, the first free integer. Every use
site goes through the enum symbol (`NetLink.cpp`, `ReplicatorChannels.cpp`,
`prototest/main.cpp`), so nothing else changed. `prototest` now asserts both
`PKT_BOUNTY == 43` and `PKT_BOUNTY != PKT_COMBAT_HIT`.

### Collision 2 - `PKT_MONEY` (tag 21) means opposite things

This one no retag can fix, because nothing about the packet differs:

| | this fork | nhoral/main |
|---|---|---|
| tag | 21 | 21 |
| struct | `MoneyPacket` | `MoneyPacket` |
| size | 13 bytes | 13 bytes |
| `tabRank` | unused, always 0 | squad-tab rank (0 = host tab, 1 = join tab, ...) |
| `money` | signed **DELTA** to apply to the shared faction wallet | **ABSOLUTE** `Ownerships::money` for that tab's platoon |

The divergence is not accidental drift: this fork deliberately redefined the
packet at protocol **22b** (`Wire.h`, "Protocol 22b: shared player-wallet
DELTA") as part of the money-sync fix, because Kenshi's player money is one
shared faction wallet and streaming per-tab absolutes fought itself. Upstream
kept the original per-tab absolute. Both are defensible; what is not defensible
is both shipping under version 45.

Same tag, same size, same field layout, opposite meaning. There is no framing
check that can catch it. A v45 fork build talking to a v45 upstream build would
hand a delta to a receiver that writes it as an absolute balance - money
silently wrong on both sides, and it persists into the save.

**Fix:** `PROTOCOL_VERSION` 45 -> **46**. Because the handshake is an exact
equality test, a version this fork alone uses converts "connects and quietly
corrupts the wallet" into a clean, immediate connection rejection against any
build that is not exactly this one. This fork keeps the delta semantics (the
shared-wallet model the money-sync fix is built on).

**Do not lower `PROTOCOL_VERSION` back to 45** while `PKT_MONEY` carries a delta
here and an absolute upstream. Reconciling the two wallet models is a separate
piece of work; until it happens, the version number is the guard.

### Why the fork and upstream can collide at all

Both trees bump the same single `PROTOCOL_VERSION` constant, in parallel,
without coordination. Nothing detects that two branches spent version 45 on
different things. If this fork ever proposes its channels upstream, the tag
allocations - not just the version number - have to be reconciled first. That
proposal is Zero's call and is not part of this merge.

## Adding a packet - checklist

1. Take the next free tag from the table above and add the row here.
2. Bump `PROTOCOL_VERSION` and add a dated entry to this file.
3. Update the `PROTOCOL_VERSION` assertion in `src/prototest/main.cpp`.
4. Add a `sizeof()` assertion for the new struct in `src/prototest/main.cpp`
   (the packed layout is part of the contract, and prototest builds with the
   same v100 toolchain as the DLL precisely so the sizes it locks are real).
5. Add a `roundTrip<T>` case so the tag/struct pairing is pinned.
