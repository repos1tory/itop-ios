# itop — kill/signal UX rationale

This document explains the design decisions behind signal sending (SIGTERM /
SIGKILL / SIGSTOP / SIGCONT) in itop. The governing principle: **read-only is
the default posture; sending a signal must be impossible to do by accident
and obvious while you are doing it on purpose.**

All code references are to `terminal_ui.inc` (the TUI) and `main.m` unless a
file is named.

## 1. The safety model: arm, then fire

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

## 2. Hit-target sizing, in cells

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
(TERM/KILL), matching the spec's "left half / right half" language; the outer
border cells disarms.

The `sig` arm tag is intentionally *small* (3 cells, ≈ 20–25 pt) and quiet
(`theme.dim`, next to the bright green `search` tag). It is an enabling
action, not a firing action; smallness here costs nothing because there is no
consequence to hitting it or missing it — worst case is arming, and armed
state is dominated by its red outline plus a one-word exit ("anywhere"). This
inverts the usual scaling rule on purpose: enable small, execute huge.

## 3. Geometry of the armed state

While armed (all drawn fresh each `drawRect`; disarming restores everything
because nothing is mutated — the next redraw simply skips the armed layer):

- **Tasks bar tag** (`sigSlotColumn` logic): the dim `sig` label is replaced by
  the legend `TERM ▸ KILL` — `TERM` in yellow, `▸` dim, `KILL` in red. It sits
  in the same right-aligned slot with the same one-column dead gap before the
  untouched `search` zone. The legend is a *reminder*, not a fire button: as
  the spec's "any other tap (including on `sig` again) disarms" requires, taps
  on the entire Tasks bar row disarm while armed, so the armed tag's two
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

## 4. Color choices

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

## 5. What is intentionally impossible

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

## 6. Keyboard parity

`k` toggles the armed state; `1` TERM, `9` KILL, `p` STOP, `c` CONT, and Esc
to disarm. The four fire keys are only registered while armed
(`ITopViewController keyCommands`), which mirrors the visual zones: un-armed,
the key does not exist; armed, it fires the same `sendSignal:` path with the
same guard regions, so keyboard and touch cannot diverge in behavior. The
companion keys intentionally sit far apart (1/9/p/c) so a fumbled `k` cannot
walk into a fire key by adjacency even on the same key row.

## 7. Signing

The only signing change is one added sbpl permission,
`(allow signal)`, appended to the existing
`com.apple.security.temporary-exception.sbpl` array in `itop.entitlements` —
the same pattern CocoaTop uses on this device. `(allow process-info*)` and
`(allow process-info-listpids)` (reads) stay as-is; nothing is weakened and
nothing else is added.

## 8. Testability and limits

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
