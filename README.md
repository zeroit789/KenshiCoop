# KenshiCoop

Setup + Demo: [https://www.youtube.com/watch?v=OqwVRRZEYGM](https://www.youtube.com/watch?v=OqwVRRZEYGM)

Experimental **co-op multiplayer for [Kenshi](https://lofigames.com/)**, built as an
[RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) /
[KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) plugin.

One player hosts their world; a friend connects (LAN, direct UDP, or Steam P2P)
and plays their own squad inside it. The plugin replicates squads, NPCs, combat,
inventory and equipment, direct trades between the players' squads, items
dropped on the ground (both directions), base building and container contents,
money, game speed, and more - and saves are coordinated: any save either player
makes becomes one shared save, streamed to both machines automatically.

> **Status: work in progress.** This is a hobby project under active
> development. Expect rough edges, desyncs, and crashes. Two players is the
> current design target.

## Session additions

This branch of the fork bundles a batch of fixes and small features on top of
[nhoral/KenshiCoop](https://github.com/nhoral/KenshiCoop) so two players can run
all of them together in a single build right now. Each change is also proposed
individually as an open pull request against nhoral's repository (see the open
PRs, roughly **#5–#26**, against `nhoral/KenshiCoop`) and is still pending review
and merge upstream — this combined branch just brings them together for play.

The changes included:

- **Steam persona name tags** — show each player's Steam persona name over their squad, with an F2 toggle to hide it.
- **Bounty / crime sync** — replicate per-character bounty and crime state (host-authoritative).
- **Per-object crafting authority + symmetric research** — the join now drives the production machines it placed, and researched tech is a grow-only union so a join's research reaches the host.
- **Shared-wallet money sync from the real source** — money replicates from the authoritative wallet delta instead of a stale snapshot, with a fixed clamp-vs-baseline desync edge case.
- **Time-aligned locomotion interpolation** — smoother synced movement by aligning interpolation to the peer clock.
- **World-item duplication mitigation** — reduce dropped-item dupes across the two clients.
- **Cross-tab control release** — releasing control of a squad tab hands authority over cleanly.
- **Carried-body self-heal on the join** — fix a join-side carried character being stuck "carried forever".
- **Per-sender stale-row guards** — symmetric channels drop stale rows per sender instead of clobbering fresh state.
- **Jail kind-conflict anchor + halt fix** — stop chained/caged captives oscillating and re-seating across clients.
- **Save-sync bad-CRC deletion + stranded-transfer recovery** — a save chunk that fails its CRC is discarded and re-fetched rather than kept corrupt, and a transfer interrupted mid-flight (e.g. a crash) cleans up on the next launch instead of leaving orphaned state.
- **Status-line auto-hide** — the on-screen co-op status banner fades out once the session is settled.
- **Build-warnings cleanup** — silence the remaining compiler warnings in the plugin build.
- **NPC chase-teleport fix** — pursuers no longer fall out of replication range and hard-snap repeatedly while chasing a fleeing player.
- **Walk-drive on-stop settle** — tightens the final approach so a driven body doesn't overshoot and rubber-band when its source stops.
- **On-screen connection errors + connect/disconnect toast** — a protocol mismatch or rejected connection now shows a real message in-game instead of only in the log, and a brief toast marks the moment a peer connects or disconnects.
- **Remembered friend's Steam ID** — the last pasted peer ID persists between relaunches instead of requiring a re-paste every session.
- **Free camera mode** — a local, client-only free-fly camera (F3 + WASD/Q-E/arrows) for screenshots and video, reimplemented for our target game version.

Reliability fixes added on 2026-07-21:

- **Steam-transport reset on UDP reconnect** — switching a live session from Steam to UDP now clears the stale Steam peer selector, so the link no longer keeps tunnelling over Steam after the panel has been switched to UDP.
- **3+ player guard counts concurrent peers** — the host-side "third player unsupported" safety guard now fires on the number of *simultaneously* connected joins instead of a monotonic join counter, so a single friend reconnecting after a network drop (CGNAT/flaky link) no longer trips it on every reconnect.
- **Title-screen join no longer stalls on the host's load** — a joining player evaluates the host's coordinated-load signal (fingerprint the on-disk save, and on a miss request the transfer) directly at the title screen instead of waiting for the save subsystem to come up. This removes a multi-minute stall where a matched load sat unconsumed in the inbound queue while the join sat at the menu; only the actual engine load is deferred until the save subsystem is ready.

> **Tuning (pending in-play validation, not a confirmed fix):** the walk-drive
> catch-up gain default (`KENSHICOOP_CATCHUP_K`) was lowered from 2.0 to 1.4 to
> reduce remote-character overshoot on direction changes. This is a feel tweak
> that still needs a real session to validate and may be re-tuned or reverted.

Host-only speed/pause and performance fixes added on 2026-07-21:

- **Host-only game speed and pause authority** — game speed and pause are now driven by the host and mirrored to the join, so the two clients no longer fight over the simulation rate; a join changing speed locally no longer desyncs the session.
- **Logger flush throttled to ~250 ms** — `CoopLog` flushed the log file on every line while holding the lock shared with the net thread, adding a synchronous disk flush to that critical section on each call. Flushes are now coalesced to roughly every 250 ms; every close/error teardown path still forces an immediate flush, so the "survives a hard kill" property is preserved.
- **Single `OutputDebugStringA` per net log line** — `netLog`/`netErr` formatted their debugger output with three separate `OutputDebugStringA` calls (prefix, message, newline); they now build one buffer and emit a single call, cutting the debugger-string syscall cost and preventing cross-thread interleaving between the fragments.
- **De-duplicated publish key-set build** — the owned-entity publish path built the same `keyOf(buf[0..n))` set twice per publish (once for the carried-body sweep, once for the furniture sweep); it is now built once and reused, and only when at least one sweep will actually run.

## How it works

- `KenshiCoop.dll` is loaded into the game by RE_Kenshi. It hooks the engine via
  KenshiLib and drives all game mutation on the main thread.
- Networking is [ENet](https://github.com/lsalzman/enet) over UDP, with an
  optional Steam P2P tunnel (no port forwarding needed).
- The host is authoritative for the world; each client is authoritative for its
  own squad. See `docs/API_REFERENCE.md` for the full engine-control surface and
  wire protocol.

```
src/plugin/       The KenshiCoop plugin (net, sync/replication, engine facade, scenarios)
src/netproto/     Shared wire-protocol headers (plain C++03, compiled by everything)
src/nettest/      Standalone ENet console app (transport de-risking)
src/netsim/       Protocol simulator
src/prototest/    Wire-protocol unit tests
src/tunneltest/   Steam-tunnel socket-hook tests
scripts/          Build, deploy, session, and automated-test tooling (PowerShell)
docs/             Build guide + engine/API reference
third_party/      ENet patches, VC10 compat shim (deps are fetched, not committed)
```

## Try it (play with a friend)

Two players, two machines. You configure the session **inside the game** with
an in-game panel (press **F2**) - you swap Steam IDs by clipboard right in the
panel, so there's no config file to edit and no launcher scripts to run. (A tiny
`coop_config.json` is only needed for LAN / direct-UDP games.)

### Before you start (both players)

1. **Kenshi 1.0.65 (Steam)**, set to windowed mode: launch Kenshi once, then
   Options > Video > un-check **Full Screen**.
2. **[RE_Kenshi 0.3.1+](https://www.nexusmods.com/kenshi/mods/847)** installed
   (free Nexus mod - it loads the co-op plugin into the game).
3. **Steam running and online** on both machines. That's the whole network
   setup: the connection is Steam P2P, so there's no port forwarding, no
   router configuration, and no IP addresses. (A direct-UDP mode is also
   available for LAN / port-forwarded games.)

### 1. Install the mod

Grab `KenshiCoop-kit.zip` from
[this fork's latest release](https://github.com/zeroit789/KenshiCoop/releases/latest)
and unzip it anywhere (both players) - this branch's build, with everything
listed above included, not the upstream `nhoral/KenshiCoop` release (which
doesn't have these fixes yet). You do not need to clone this repository - but
if you did, the same kit is in [dist/mod-kit](dist/mod-kit).

The zip contains a single **`KenshiCoop`** folder. Copy that folder into your
Kenshi `mods` directory so you end up with
`<Kenshi>\mods\KenshiCoop\KenshiCoop.dll` (default Steam path:
`C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\`). Then launch
Kenshi and enable **KenshiCoop** in the Mods menu.

### 2. Connect in-game (press F2)

The Co-op panel works at the **main menu** (before you load a game) as well as
in-game, so the joining player doesn't need to load anything first.

1. Press **F2** to open the Co-op panel.
2. **Swap Steam IDs.** Each player clicks **"Copy my Steam ID"** and sends it to
   the other (Steam chat, Discord, ...). When you receive your friend's ID, copy
   it, then click **"Paste friend's Steam ID"** - the panel shows the ID it
   captured. The last ID you paste is remembered between relaunches, so you
   only need to do this once per friend, not once per session.
3. Leave **Transport** on **STEAM**.
4. **Host:** load the save you want to play, or start a new game - pick
   **Multiplayer (Wanderer x2)** from the start list for a ready-made two-squad
   co-op start (see below). Then set **Role: HOST** and toggle **Connection** to
   **ONLINE**.
5. **Join:** straight from the **main menu** - no save needed - set
   **Role: JOIN** and toggle **Connection** to **ONLINE**. The host streams its
   world to you on connect and you load right into it. (If you already have an
   identical copy of the host's save on disk, it's used as-is instead of
   transferring.)
6. The white status line shows live state (and a banner over your leader shows
   it too, in-game - it auto-hides a few seconds after you're solidly connected,
   see below). Toggle **Connection** to **OFFLINE** to leave.

**LAN / direct-UDP (advanced):** skip the Steam ID swap. Open
`<Kenshi>\mods\KenshiCoop\coop_config.json`, set `"transport": "udp"`, and put
the host's address in `"ip"` / `"port"`. Then in the panel set **Transport: UDP**
and go ONLINE. The `ip`/`port` are re-read whenever you go ONLINE, so no restart
is needed after an edit.

### Controls & on-screen info

- **F2** opens and closes the Co-op panel. It works at the main menu and in-game
  (as above), so you never need a launcher or a command line.
- **Show player names.** The panel has a **"Show player names: ON/OFF"** toggle
  (ON by default). While it's ON, your friend's **Steam persona name** floats over
  the units they control on your screen, so you can tell their characters apart
  from ordinary NPCs at a glance. Switch it OFF for a clean view. If Steam can't
  supply a name (your friend isn't on your Steam friends list yet, or you're on
  the LAN/UDP transport) the tag falls back to the character's own name, or
  `[Remote Player]`.
- **Status banner.** The banner over your squad leader is colored by connection
  state: **red** = offline, **yellow** = connecting/waiting, **green** =
  connected. Once the session has been solidly connected (green) for about ten
  seconds it **auto-hides** so it stops cluttering the screen, and pops back the
  moment the state changes (your friend disconnects, or a reconnect starts). The
  white status line in the F2 panel always shows the current state.
- **Connect/disconnect toast.** A brief "Peer connected"/"Peer disconnected"
  message flashes on-screen the moment the network state actually changes -
  separate from the persistent status banner above, and it fades on its own
  after a few seconds.
- **F3 free camera.** Toggle a local, client-only free-fly camera - fly with
  **WASD** + **Q/E** (up/down), look with the **arrow keys**, hold **Shift** to
  move faster. Handy for screenshots/video; doesn't affect your character or
  sync to your friend.

### Good to know

- **You each control your own squad.** With one squad tab per player, the host
  runs squad 1 and the joining player squad 2. Your friend's squad is visible
  and synced on your screen, but answers only to them. If your save has only
  one squad, move some units into a second squad tab in-game to give them a crew.
- **Two-player start included.** The KenshiCoop mod ships a **"Multiplayer
  (Wanderer x2)"** game start (New Game -> pick it from the list): the vanilla
  Wanderer start, but with two wanderers already split into separate squads, so
  the host gets squad 1 and the joining player squad 2 with no manual tab-splitting.
  The start was authored by [zeroit789](https://github.com/zeroit789).
- **The joining player doesn't need the host's save.** The host picks the save
  (or starts a new game); when the join connects from the menu, the host's world
  is streamed over automatically. Already having an identical copy on disk just
  skips the transfer.
- **Saving just works.** Any save either player makes during a session becomes
  one shared save on both machines, streamed to the other side automatically.
  To resume next time, the host loads that save and goes online, and the join
  can reconnect straight from the main menu again.

### If something goes wrong

- **"The co-op plugin has not started"** - RE_Kenshi didn't load it. Check
  `<Kenshi>\RE_Kenshi_log.txt` for `KenshiCoop`; reinstalling
  [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) usually fixes it.
- **No connection (Steam)** - both Steams must be online (not offline mode), and
  each side must have **Pasted** the *other* player's ID (the panel shows the
  captured ID - confirm it matches). If "Paste friend's Steam ID" reports the
  clipboard wasn't a Steam ID, have your friend re-copy theirs. Look for
  `[steam] session ... active=1` in `<Kenshi>\KenshiCoop_*.log`.
- **"protocol mismatch" / "version mismatch" on-screen or in the log** - one of
  you has an older build; both players should re-install from the same
  release.

The kit's `README.txt` has the full setup + troubleshooting list.

## Building

The plugin must be compiled with the **Visual C++ 2010 (v100) x64 toolset** (a
KenshiLib requirement). Full toolchain setup, gotchas, and install steps are in
[docs/BUILD_SETUP.md](docs/BUILD_SETUP.md). Short version, once prerequisites
are in place:

```bash
cmd //c scripts/build_plugin.cmd
```

Dependencies are fetched, not committed:

- KenshiLib + precompiled libs: clone
  [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps)
  into `third_party/KenshiLib_deps/`
- ENet: clone [lsalzman/enet](https://github.com/lsalzman/enet) into
  `third_party/enet/enet/` and apply the patches in `third_party/enet/patches/`
  (see `third_party/enet/README.md`)

## Development and testing

`scripts/` contains an automated two-client test harness: `dev_cycle.ps1`
rebuilds, deploys to two local installs, launches host + join, runs a named
scenario, and produces a numeric PASS/FAIL verdict from the two logs.
`regress.ps1` runs the scenario regression suite. See
[docs/BUILD_SETUP.md](docs/BUILD_SETUP.md) Parts D-E for details.

## Credits

- [nhoral](https://github.com/nhoral) (Mike Cook) - the original **KenshiCoop**
  project and all of the base co-op work this fork builds on
- **Lo-Fi Games** - [Kenshi](https://lofigames.com/) itself; none of this exists
  without their game
- [BFrizzleFoShizzle](https://github.com/BFrizzleFoShizzle) - RE_Kenshi and
  KenshiLib, which make plugins like this possible
- [lsalzman/enet](https://github.com/lsalzman/enet) - UDP networking library
- [zeroit789](https://github.com/zeroit789) - the "Multiplayer (Wanderer x2)"
  co-op game start ([#15](https://github.com/nhoral/KenshiCoop/pull/15))

## License

[AGPL-3.0](LICENSE). KenshiLib and RE_Kenshi are GPLv3; this plugin links
KenshiLib under GPLv3 section 13 (GPL/AGPL combination). Not affiliated with
Lo-Fi Games. Non-commercial fan project.
