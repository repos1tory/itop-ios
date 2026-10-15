# itop UX rationale — filter editing, paging, persistent settings

This branch (`filter-edit`) delivers three small, independent features: in-place
filter editing (htop F4 style), hardware paging keys with a jump-to-PID prompt,
and persistence of the sort key, sort direction, and filter text. This document
records the interaction decisions and the reasons behind them.

## 1. In-place filter editing (htop F4 affordance)

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

## 2. Paging hit model — keyboard only

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

## 3. Jump overlay (J → typed PID → Return)

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
tappable. The "top area" reading of the brief was resolved in favor of the
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
prompt or a later jump. No new touch zones exist for jump, per the Task B
constraint; it is keyboard-first by design.

## 4. What persists and why

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

## 5. NSUserDefaults suite hygiene in `--self-test`

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

## 6. Self-test coverage summary

Run with `./build.sh test` (builds and runs `./build/itop.app/itop --self-test`):

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

All three print one `ok` line and the harness exits non-zero if any assertion
fails.
