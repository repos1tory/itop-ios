# itop UX rationale

This document records the interaction decisions behind the features itop ships
and the reasons behind them, in three families:

- Memory accounting: the device-wide pressure level (`Prs`) and the per-process
  jetsam limit (`LIM`) from the `mem-limits` line of work, which extends the
  memory story itop already told (hardware RAM in the Mem panel, `Jtsm`
  headroom for itop itself, per-process `RES`).
- Terminal review tools: in-place filter editing (htop F4 style), hardware
  paging keys with a jump-to-PID prompt, and persistence of the sort key, sort
  direction, and filter text.
- Signal sending: guarded SIGTERM / SIGKILL / SIGSTOP / SIGCONT to the selected
  process, under an arm-then-fire model.

## 1. Why the Prs row lives in the Mem panel

- **Proximity to related info.** The pressure level is the kernel's own view of
  the same resource the panel already reports: physical usage, compressed pages
  (`Comp`), and the `Jtsm` headroom row are all answers to "how close is the
  device to jetsam pain?" Placing `Prs <level>` directly beneath `Jtsm` keeps
  the jetsam-derived signals in one visual cluster instead of scattering a
  memory metric among CPU diagnostics.
- **Arm-distance legibility.** The row reuses the established panel geometry:
  label column at `memX+2`, the same `█/░` meter bar and width as the `Mem` and
  `Root` rows, value right-aligned two cells into the border. A reader who
  learned the meter grammar from `Mem` reads `Prs` with zero new skills; no
  squinting at a new widget type.
- **Colors mean the same thing everywhere.** `<60 green, 60-84 yellow, ≥85 red`
  is exactly the CPU threshold grammar this terminal already uses for core
  bars and the `avg` readout. One threshold language across meters means one
  intuition to maintain; jetsam level 0-100 maps onto it directly because the
  counter is already normalized to that range.
- **iPad touch: zero new zones.** The row is pure text in the standard box
  grid; it registers no touches and adds no hit regions. The whole feature is
  a painted row — the same interaction contract as the `Jtsm` row it extends.
  (Row-0 header zones for help/refresh/FPS and the Tasks-bar search tag are
  byte-for-byte unchanged; the process table's own zone math shifts down one
  row in wide layout, exactly as the drawn table does, and no new tap targets
  were introduced.)

### Layout cost honesty

Both mem-panel content sections share one column height with the CPU panel in
the wide layout, and the memory panel is taller than its five legacy rows now.
Rather than silently dropping the Prs row on low-core devices (the build device
reports two cores in landscape), the shared panel height grew by exactly one
row (`coreRows + 6` wide instead of `+5`; compact stays 9 because it already
carried a spare interior row). The cost is one process-table row of depth in
wide layouts — chosen over clipping the feature, because a metric that only
exists on high-core-count devices is a trap.

## 2. Why LIM shows in the detail line only, and only when earned

- **Space vs. signal at narrow layout.** The process table is already at
  column capacity; on the narrow ~70-column layout the command column just
  barely clears the right edge, and the per-row extra enumeration (`THR`,
  `ENER`, `FLT` rates) deliberately lives in the single detail line. LIM is
  per-process only true for *one* interesting row at a time (the selected
  process), so it belongs in that line — appended after `SYS` rates so narrow
  screens clip LIM first, keeping all pre-existing data visible. A full column
  would also push the `Command` column off narrow screens entirely, costing
  the table's most-used reader signal for a value that is usually `0`-limit
  default.
- **Honest failure modes.** iDevices deny `memorystatus_control`'s
  `GET_PRIORITY_LIST` to a process without the memory-status entitlement
  (observed in `--self-test`: the wrapper returned rc=-1 and errno=EPERM in
  the shell context). itop's contract: never fabricate, never spam an error a
  row at a time, never print a fake 0 that could read as "no limit". The
  per-process line therefore ends with exactly one `LIM n/a` token while the
  struct layout (see audit below) lacks on-device proof. If a future tree
  verifies the ABI, the display replaces `n/a` with the joined bytes keyed by
  PID; PIDs absent from the list would render `LIM -`.

## 3. Methodology and evidence trail for the memorystatus ABI

Executed on the device shell (2026-10-15, iPad7,5 / iOS 17.7.8, Procursus root
at `/var/jb`):

1. `grep -rn "memorystatus_control|kern_memorystatus|MEMORYSTATUS_CMD"
   /var/jb/usr/include /var/jb/usr/share/SDKs` → only two nonzero lines, both in
   `syscall.h`:
   - `/var/jb/usr/share/SDKs/iPhoneOS.sdk/usr/include/sys/syscall.h:480`
     `#define	SYS_memorystatus_control 440`
   - `/var/jb/usr/share/SDKs/iPhoneOS.sdk/usr/include/sys/syscall.h:493`
     `#define	SYS_memorystatus_get_level 453`
2. `grep -n "_memorystatus" /var/jb/usr/share/SDKs/iPhoneOS.sdk/usr/lib/libSystem.B.tbd`
   → `_memorystatus_control` (line 2653), `_memorystatus_get_level` (line 2654).
   The libc wrappers exist, so main.m declares
   `extern int memorystatus_control(uint32_t, int32_t, uint32_t, void*, size_t);`
   and calls the wrapper in `--self-test` with a malloc'd buffer that is
   always `free()`d; a fallback to `syscall(440, ...)` is documented but was
   not needed — the link succeeded on this toolchain.
3. `ls .../usr/include/sys | grep -i memorystatus|jetsam` and a full
   `grep -rln "memorystatus" .../usr/include/` → matches only `syscall.h`.
   There is no `kern_memorystatus.h`, hence **no on-device `memorystatus_priority_entry_t`**
   evidence, and itop refuses to conjure offsets for
   `{pid, priority, user_data, limit_bytes, ...}` from memory. A guess like
   (`command[256]`, `state`) would describe a different structure family
   (the memorystatus *journal/listentry* variants of xnu), and constructing an
   unverifiable offset list into "verified" comments would be fabrication.
4. `sys/syscall.h` also confirms `SYS_memorystatus_available_memory 534`
   (line 574), the trap basis for the already-shipped `os_proc_available_memory`
   headroom readout — consistency check that this syscall table serves the
   build device.

Shipped because it is struct-free (nothing to guess):

- `ITopMemoryPressureLevel()` calls trap 453 with a sentinel-primed uint32_t.
  Success = return 0 AND the sentinel overwritten, which also blocks a broken
  wrapper from presenting uninitialized memory as level 0.

Skipped deliberately (a wrong guess is unshippable; `n/a` is shippable):

- Per-priority list parsing (`LIM <bytes>` join): no on-device layout proof;
  `--self-test` prints the observational result (`rc`, `errno`) and the app
  shows `LIM n/a`.

## 4. Failure modes

| Case | Display | Notes |
| --- | --- | --- |
| `SYS_memorystatus_get_level` EPERM / error | `Prs` row shows `n/a`, no meter bar | A bar at 0 would lie ("no pressure"); only real data draws one. |
| Level lands outside 0-100 | meter clamps to full; `n/100` text prints the raw value | Clamping touches the meter only, not the reported number. |
| Priority list EPERM inside app | `LIM n/a` in detail line | Single token, single row; no error banner, no log spam. |
| Priority list rc>0 but unparseable layout | `LIM n/a` (never parsed total) | Parsing is gated strictly on header evidence existing. |
| Sentinel survives (kernel wrote nothing) | treated as unavailable | Guards against a wrapper bug presenting uninitialized memory. |

## 5. Robustness notes

- `--self-test` asserts the formatter contract (`0B` / `15M` / `5.5G` /
  `1024.0G`, plus a width-10 fixed-width clip) so the display path owns an
  exact-string test independent of kernel cooperation.
- The probe buffer is fixed-size (64 KiB), stack-free, and freed; no
  allocation retention across the 1 Hz loop, and the level call itself
  allocates nothing.
- Overflow care: the level value is read into `uint32_t`; the meter consumes
  `MIN(level, 100)`; the panel-bottom guard compares against
  `memoryY + panelHeight - 1` explicitly so a hypothetical 1-core device
  degrades to "row hidden" instead of overwriting the box border.
- Sort/search/tree behavior untouched; the feature adds one drawn row and one
  appended detail token. The help page documents both, and the freefall of no
  honest data is always the word `n/a`.

## 6. In-place filter editing (htop F4 affordance)

**What exists.** The Tasks bar carries a right-aligned terminal tag: `search`
when no filter is active, `search:<text> ✕` when one is. Tapping the tag opens
the canvas `UIKeyInput` prompt; the status line takes over with
`Search: <draft>_  Return: accept  Esc: cancel` in amber. Tapping the trailing
`✕` (the last 2 cells of the tag) clears the filter in place, no keyboard.

**Why tap-to-edit and not tap-to-open-only.** Once the user has a filter, the
most common follow-up is a small correction — removing one character, adding a
suffix, or switching to a different process name that shares a prefix. Re-typing
the whole query (opening an empty prompt) or filtering again from scratch is a
harder mental operation than nudging the text that is already visible on the row
where their finger already is. htop's F4 behaves exactly this way: the prompt
re-opens seeded with the current pattern and the user edits from there. Because
`beginSearch` already seeds `_searchDraft` from `filterText` and preserves
`_searchOriginal` for Esc, the edit flow needs no new state.

**Why the ✕ stays separate and on the same row.** Clearing is a destructive-ish
but fully reversible one-tap action; keeping it adjacent to the text it clears
preserves locality (you can see the text disappear as you clear it). The last 2
cells (` ✕`) are excluded from the edit zone so the two affordances do not sit
on top of each other; the geometry lives in one shared helper
(`ITopSearchTagGeometry` / `ITopSearchTapAction`) used by drawing, touch
routing, and the `--self-test` checks, so the hit targets can never drift from
what is painted. The tag hides entirely when the row gets too narrow (the
computed column would collide with the `Tasks n/m` title); in that case the
hardware `F` key still opens the editor — no ambiguous taps, no clipped input
reaching the prompt region.

**Modal behavior while typing.** Row, header, and function-zone taps are
suppressed while a prompt is open (`if (_searching || _jumping) return;`). The
filter stays live while typing (rows refilter on each keystroke, as before),
Return accepts, Esc restores the previously accepted text, and ✕ clears without
raising the keyboard. Neither path resets the active filter before you choose to
commit something else.

## 7. Paging hit model — keyboard only

**What exists.** Hardware `PgUp`/`PgDn` scroll exactly one page
(`_rowCapacity` rows, clamped into the offset window), `Home`/`End` jump to the
top/bottom, `↑`/`↓` move selection with scroll-into-view, and one-finger swipe
in the process area scrolls as before.

**Why no touch paging buttons.** itop runs mostly on iPads viewed at arm's
length or greater (held in landscape, propped on a stand, or docked next to an
external display). At that distance, touch targets smaller than roughly 10 mm
become straining and error-prone: the average adult index fingertip pad plus the
offset between the anatomical finger tip and the on-screen "touch point" makes
precise small vertical strips unreliable, and buttons sharing the right edge
with the scrollable list would be hit by the same fat-finger gestures that are
meant to scroll. Scrolling by direct manipulation is already the ergonomically
superior touch gesture for paged review of a list — a paging button would
duplicate it with a worse mechanism. Hardware paging, by contrast, is a
muscle-memory pattern borrowed from every terminal pager (htop, less, tmux) and
costs no screen space; the TUI aesthetic explicitly avoids icon buttons. This
keeps every touch gesture unambiguous: rows select, headers sort, the search tag
edits, swipes scroll — nothing new to aim at.

**Implementation notes.** The offset math
(`ITopScrollMaximum` / `ITopScrollClamp` / `ITopPagedOffset`) is pure and shared
with the `--self-test` checks; there is no unsigned underflow (page-up clamps at
0, list shorter than the viewport never scrolls, and the zero-capacity state
before the first layout pass is inert because `drawRect` re-clamps every frame
anyway). Paging never moves the selection — it mirrors swipe scrolling — so the
status line keeps describing the row the user last picked.

## 8. Jump overlay (J → typed PID → Return)

**Design.** Pressing `J` opens a temporary prompt that reuses the search
prompt's region and styling: the amber status line shows
`jump: <pid>?  Return jump  Esc cancel`, with the typed draft inserted in place.
It "replaces the search prompt region while active" so there is only ever one
prompt region on screen and no duplicate prompt UIs to style or route taps
around — the shared `_searching`/`_jumping` modes extend the same `UIKeyInput`
path (`_searchDraft` for search, a separate small `_jumpDraft` for digits)
rather than duplicating a second input pipeline.

**Why the prompt lives in the existing prompt region instead of a floating top
box.** A status-line prompt matches how the search filter already works, keeps
all transient messages in one predictable line, and avoids covering the CPU or
memory panels with a decal that would occlude live meters while also being
tappable. The "top area" reading was resolved in favor of the
prompt-region reading ("the prompt replaces the search prompt region while
active"), which is also what keeps the hit model simple.

**Rules.** Digits only (up to 7 — pids are `int`), punctuation and arrows are
ignored, Return commits: if the PID exists in the *visible* list it is selected
and scrolled into view (same `revealRowIndex` math as arrow-key selection); if
it does not, the prompt stays up and the hint reads
`jump: <pid>?  no such pid  Return jump  Esc cancel` until the next keystroke.
Esc cancels and returns to the previous state; paging keys still work while the
prompt is open so the list can be reviewed before committing. All jump state
(`_jumpDraft`, `_jumpMiss`, `_jumping`) is pruned by `pruneJumpState` whenever
either prompt closes — including via `beginSearch`, `cancelSearch`,
`clearFilter`, and Return-accept — so no orphan draft can leak into the search
prompt or a later jump. No new touch zones exist for jump; it is
keyboard-first by design.

## 9. The safety model: arm, then fire

The governing principle: **read-only is the default posture; sending a signal
must be impossible to do by accident and obvious while you are doing it on
purpose.**

Touch has no hover state, an off-by-one finger has no undo, and `kill(2)` has
no confirmation dialog on our side. The classic desktop answer (right-click
menu → menu item) does not exist, so the two desktop ideas that *do* carry
over were kept:

1. **De contextualize the destructive action from its target.** Selecting a
   row (tap) and destroying that row (fire) are two different gestures aimed
   at two different parts of the screen. There is no swipe zone, no row
   long-press, and *no signal zone anywhere in the read-only layout* — the
   `touchesEnded` row branch (`terminal_ui.inc`) only sets `selectedPID`.
2. **Make the enable step itself an explicit, visibly different mode.** Tapping
   the small `sig` tag (or pressing `K`) enters the armed state
   (`toggleSignalArmed`). Arming requires that the highlighted row actually
   exists in the rendered list (`_renderedRows` contains `selectedPID`), so the
   armed target is always a row the user can see highlighted on screen. The
   signal only ever fires from `sendSignal:`, which requires `_signalArmed`
   and immediately disarms — a second tap in a *named, colored zone*.

Why this is the right model for touch: the arm step costs one cheap,
reversible tap on a quiet tag; the fire step costs one tap on a zone that did
not exist a moment ago and that announces itself with a full-screen red
outline. A stray tap during the armed state has a huge probability of falling
anywhere outside the bottom line — and *every* tap outside the fire zones,
including on the `sig`/task-title area, header zones, rows, and the two border
cells of the bottom line, disarms harmlessly (`touchesEnded`, armed
intercept). A drag is even harder to misuse: while armed,
`touchesMoved` marks any vertical excursion > 10 pt as `_didMove` and
`touchesEnded` turns a moved touch into a plain disarm ("drags disarm, they
never fire"), so a swipe across the screen can never land as a tap on a fire
zone.

Misfire accounting for the second tap: the bottom line is the only live
surface (roughly 2–3 % of the canvas), the two border cells of that line are
disarm-only, and the fire surfaces are labeled. Every other pixel of the
screen is the safe exit.

## 10. Hit-target sizing, in cells

The grid is `_terminalColumns` × `_terminalRows` with `_cellWidth` measured
from "M" (≈ 6.3 pt on the compact 10.5 pt font used on iPhone-class widths,
≈ 7.2 pt on the 12 pt font used on iPad-class widths ≥ 620 pt).

iPad (7,5, portrait 768 pt, non-compact): columns ≈ 104.
iPad landscape (1024 pt): columns ≈ 140.

| Zone (armed, bottom line) | Columns | iPad portrait width | iPad landscape width |
|---|---|---|---|
| `TERM` (left half)        | ≈ half of the line minus center | ≈ 300 pt | ≈ 400 pt |
| `KILL` (right half)       | ≈ half of the line minus center | ≈ 300 pt | ≈ 400 pt |
| `STOP`/`CONT` windows     | 4 cells each                    | ≈ 29 pt  | ≈ 29 pt  |

Apple's tappable-minimum guideline is 44 pt. The two destructive zones are
6–9× that, i.e. they miss nothing about user intent: a thumb that means
"TERM" gets TERM even when sloppily placed. The deliberately small targets are
`STOP`/`CONT` — and that asymmetry is the point: **SIGSTOP is reversible**
(SIGCONT fixes it), so it is allowed a modest target and a calm cyan color;
the irreversible ones are the huge, loud, labeled surfaces. Between the
`STOP` and `CONT` windows sit two gap cells that fall through to the half rule
(TERM/KILL), matching the left-half / right-half rule; the outer
border cells disarms.

The `sig` arm tag is intentionally *small* (3 cells, ≈ 20–25 pt) and quiet
(`theme.dim`, next to the bright green `search` tag). It is an enabling
action, not a firing action; smallness here costs nothing because there is no
consequence to hitting it or missing it — worst case is arming, and armed
state is dominated by its red outline plus a one-word exit ("anywhere"). This
inverts the usual scaling rule on purpose: enable small, execute huge.

## 11. Geometry of the armed state

While armed (all drawn fresh each `drawRect`; disarming restores everything
because nothing is mutated — the next redraw simply skips the armed layer):

- **Tasks bar tag** (`sigSlotColumn` logic): the dim `sig` label is replaced by
  the legend `TERM ▸ KILL` — `TERM` in yellow, `▸` dim, `KILL` in red. It sits
  in the same right-aligned slot with the same one-column dead gap before the
  untouched `search` zone. The legend is a *reminder*, not a fire button: any
  other tap (including on `sig` again) disarms, and taps on the entire Tasks
  bar row disarm while armed, so the armed tag's two
  colored spans stay read-only. This keeps exactly one firing surface (the
  bottom line) instead of two different-sized ones.

- **Full-canvas border**: one-cell-thick `┌──...──┐` / `│` column pairs /
  `└──...──┘` in `theme.red` drawn over the existing grid perimeter rows
  (`armedNotice` block at the end of `drawRect`). Redrawing on top is the
  cleanest possible "restore": the next 1 Hz sample redraw or any state change
  rebuilds the perimeter content exactly as before, so no save/restore of
  covered cells can ever leak pixels.

- **Armed notice and zone row**: the notice text
  `ARMED: tap TERM or KILL below to signal PID <pid> · tap elsewhere to disarm`
  replaces the selection-detail status on the row *above* the bottom line, and
  the bottom line becomes the fire-zone row (`TERM` left, `STOP CONT` center,
  `KILL` right, red border ends). The process list shrinks by exactly one row
  (`_rowCapacity--` while armed) so nothing is overwritten — the layout
  reflows instead of overpainting rows. Rationale: "tap TERM or KILL *below*"
  only describes reality if the zones are physically below the message, and
  the bottom edge of the screen is also where a thumb typing/steering on an
  iPad rests, which is where you want the biggest targets. The normal
  selection-detail status text is byte-identical when disarmed (same format
  string, same branch order; the armed and result branches come first).

- **Auto-disarm after send**: `sendSignal:` clears `_signalArmed` *before*
  forwarding to the delegate, so a slow/failed kill can never leave a live
  fire surface behind, and a second accidental fire is impossible.

## 12. Color choices

The palette already encodes severity on the data side (CPU ≥ 85 % red,
≥ 60 % yellow: `drawRect` core bars). Signals reuse the same semantics:

- `TERM` = `theme.yellow` — caution: the process dies, but this is the polite
  way. Yellow is the "think twice" color in this palette.
- `KILL` = `theme.red` — destructive, unrestricted, no negotiation. Red inside
  a red border reads as "inside the dangerous thing, the most dangerous thing".
- `STOP` / `CONT` = `theme.cyan` — lifecycle control that is *not* destructive;
  cyan is the palette's informational color (`?` tag, column names). Giving
  STOP/CONT the destructive colors would admit them to a club they do not
  belong in and would wash out the red/yellow signal.
- Armed border and armed notice = `theme.red`; the result line prints green on
  `ok` and red on failure (including guard refusals), matching the meter
  color language.

No new colors are introduced; the state is communicated by *reweighting*
existing ones.

## 13. What is intentionally impossible

- **Stray-tap kill.** In the read-only layout there is no signal UI at all;
  row taps only select (`touchesEnded` final branch). The wiring is
  delegate-only: the UI cannot reach `kill(2)` except through
  `sendSignal:`, which refuses anything but the armed state.
- **Swipe-kill / drag-kill.** No swipe zones exist; while armed, movement
  disables firing entirely (`_didMove`).
- **Broadcast kill.** `ITopSignalGuardAllows` (main.m, pure) refuses
  `pid <= 0` — `kill(2)` with 0 or −1 broadcasts to the process group/all
  processes; with any negative pid it signals a process group. No pid from
  the UI can even be negative: rows come from `proc_listpids` filtered
  `pid <= 0` in `sampleProcesses:`.
- **Self-kill.** The guard compares against `getpid()` and refuses. itop
  appears in its own list, so *typing* this mistake must be impossible, not
  just untyped: the refusal runs on the monitor queue right before the
  syscall, so a UI regression still cannot suicide the app.
- **Killing launchd.** `pid == 1` refused ("launchd (pid 1) is never
  signalled").
- **Firing from another mode.** While searching the keyboard's keyCommands are
  suppressed and `toggleSignalArmed` no-ops; arming closes help; while armed,
  S/F/R/H/Space/arrows disarm instead of acting (`disarmIfArmed` in
  `ITopViewController`), and the header `?`/`R`/FPS zones are covered by the
  top border and disarm on tap. There is no path to fire while the process
  list is not in command.
- **Silent outcomes.** The result line always names target together:
  `SIGTERM -> 4486 (zsh): ok` / `SIGKILL -> 4486 (zsh): failed: No such
  process` / `... refused: itop never signals itself` (`itopSignal:` in
  `ITopViewController` builds the line from the name captured before the
  async call, so a process that dies mid-call is still named). The result is
  sticky — it survives the 1 Hz redraw — and clears on the next action that
  changes selection/sort/filter or on re-arming, because a "failed" verdict
  you only see for one frame is no verdict at all.

## 14. Keyboard parity

`k` toggles the armed state; `1` TERM, `9` KILL, `p` STOP, `c` CONT, and Esc
to disarm. The four fire keys are only registered while armed
(`ITopViewController keyCommands`), which mirrors the visual zones: un-armed,
the key does not exist; armed, it fires the same `sendSignal:` path with the
same guard regions, so keyboard and touch cannot diverge in behavior. The
companion keys intentionally sit far apart (1/9/p/c) so a fumbled `k` cannot
walk into a fire key by adjacency even on the same key row.

## 15. Signing

The only signing change is one added sbpl permission,
`(allow signal)`, appended to the existing
`com.apple.security.temporary-exception.sbpl` array in `itop.entitlements` —
the same pattern CocoaTop uses on this device. `(allow process-info*)` and
`(allow process-info-listpids)` (reads) stay as-is; nothing is weakened and
nothing else is added.

## 16. Testability and limits

The guard is a pure function, `ITopSignalGuardAllows(pid_t, NSString **)`,
separate from the syscall wrapper `ITopMonitor itopSendSignal:toPID:error:`.
`--self-test` asserts six guard outcomes (self/0/−1/−42 refused with the
expected reason keywords, `launchd`/pid 1 refused, an ordinary pid allowed)
and provably never calls `kill(2)` — the simulation is decision-level only,
so running the test cannot signal SpringBoard, launchd, the test itself, or
anything else.

Known, accepted limits (also stated in the README): the pid can be recycled
between arming, firing, and delivery; the name printed with the outcome is
captured from the last snapshot when the fire happened, and the syscall
stays as fast as it can be — this is a debugging tool, not a transactional
system. What the limits never allow, by construction, is anything other than
"one named signal to one positive, guarded pid that the user highlighted and
fired on twice."

## 17. What persists and why

Persisted keys (standard `NSUserDefaults` suite): `itop.sort.key`
(sort key as a string), `itop.sort.desc` (direction), `itop.filter` (filter
text).

- **Sort key/direction:** re-derived drawing state at relaunch would flicker the
  table into a different order for one frame; persisting makes the relaunch
  seamless and keeps the status line's selected-process narrative stable.
- **Filter text:** a respring, userspace reboot, or screen-reader-triggered
  relaunch on a jailbroken device should not silently widen the visible list:
  users filter to a workload (e.g. `launchd`) and expect to keep it.
- **Not persisted — paused:** pause is genuinely transient session state ("hold
  these numbers while I look"), and the sampling timer follows the foreground
  lifecycle anyway; persisting it would relaunch the app into a paused state
  where nothing refreshes, which looks like breakage. Same for theme: itop has
  one fixed ANSI-dark terminal palette, so there is no dark/light setting to
  store.
- **Validation (hardening):** reads validate against the known sort-key set
  (`CPU%`, `RES`, `MEM%`, `VIRT`, `TIME+`, `PID`, `USER`, `PRI`, `NI`,
  `Command`); anything else — including garbage like `EVIL` or a spatially
  corrupted plist value — degrades to `CPU%` descending via
  `ITopSortKeyValid`. The direction is read defensively (`objectForKey` +
  `respondsToSelector:@selector(boolValue)`) and falls back to the canonical
  per-key default; a non-string filter degrades to an empty filter. Loading
  assigns ivars directly and never writes back, so a damaged plist is not
  "repaired" into a mystery value at launch, and fresh installs do no writes at
  all. Writes happen only on change (`setFilterText:`/`setSortKey:`/
  `setSortDescending:`), one key per change, matching the "cheap single-key
  writes, no coalescing" requirement.

## 18. NSUserDefaults suite hygiene in `--self-test`

The production code path uses the standard suite only. Every canvas exercised
by `--self-test` gets an injected `NSUserDefaults` store
(`settingStore`), always the throwaway suite `"itop-test"` — the round-trip test
writes a known configuration into that suite, loads it back through the canvas,
then writes garbage (`"EVIL"` sort key, array-valued direction/filter) and
asserts the defaults take over. The domain is removed twice over: once at the
start of the run (a stale-domain guard in case a previous run was killed
mid-test) and once in a function-scope `@finally`, so even an early `return 1`
from any failing check releases the domain. Consequently the tester never
touches, and never leaves residue in, the real user defaults of the device it
runs on.

## 19. Self-test coverage summary

Run with `./build.sh test` (builds and runs `./build/itop.app/itop --self-test`):

- **memorystatus / net / disk probes** (pre-existing) — the pressure level is
  read against a sentinel (a refusal prints `n/a`, never a fabricated 0), the
  priority list is probed observationally (`rc`/`errno` only; entries are never
  parsed), the `ITopTermBytes` formatter contract is asserted as exact strings
  (`0B` / `15M` / `5.5G` / `1024.0G` plus a width-10 clip), the per-process
  network source audit re-runs, sparkline levels are checked
  for monotonicity/clamping and the RX/TX ring for its 64-entry window, and
  disk rates are checked for window arithmetic (counter-reset clamps to 0,
  finite) plus live values over a real rusage window.
- **filter-tap self-check** — shared geometry at 100/70/130 columns, ✕ = last 2
  cells, no-filter tag routes to edit, oversized filter and title-collision hide
  the tag (`NSNotFound`), canvas-level edit round trip (type → Return accept →
  reopen with existing text → Esc preserves → ✕ clears).
- **paging & jump self-check** — page clamps on fabricated 50-row arrays with a
  10-row viewport (advance/back/top/bottom, no underflow, short list),
  row-index PID lookup (hit and miss), PID draft parsing (empty/negative/
  overflow rejected), canvas-level prompt cycle (Return selects 1023 and
  closes; a miss keeps the prompt and the previous selection; Esc closes).
- **settings persistence self-check** — suite round trip (sort key, non-default
  direction, filter), load does not rewrite the store, garbage degradation to
  `CPU%` descending and empty filter, change-driven single-key writes.
- **signal guard self-check** — six guard outcomes asserted (self, `0`, `−1`,
  `−42`, and `launchd`/pid 1 refused; an ordinary pid allowed), with `kill(2)`
  provably never called by the test.

Each self-check block prints its own result line and the harness exits
non-zero if any assertion fails.
