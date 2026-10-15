# itop

A native, rootless-jailbreak iOS/iPadOS system monitor rendered as a full-screen, fixed-cell terminal grid. Every visible element is monospace text: box-drawing characters, terminal meters, process rows, and an htop-style function-key line. There are no cards, oversized dashboard labels, or native alert dialogs. Metrics are sampled once per second on a background queue; a lightweight display link requests the screen's maximum supported refresh and pauses when backgrounded.

## Features

- Overall and per-core CPU load with a short history graph.
- Physical memory, wired/compressed memory, root filesystem usage, uptime, and battery.
- Read-only process list with CPU and resident memory, sorting, name/PID filtering, row selection, and scrolling.
- Diagnostics panel: itop energy use, thermal state, Wi-Fi/cellular RX/TX rates,
  collection time, and jetsam headroom for this process (shown as n/a when the
  runtime does not provide a value). Selecting a row adds per-process energy
  plus fault, page-in, context-switch, and syscall rates in the status line.
- Terminal key bar, touch row selection, swipe scrolling, and hardware keyboard shortcuts.
- Process information entitlements are limited to enumeration/inspection. Sending signals
  is the one deliberate exception and is always opt-in (see *Dangerous actions* below).
- iPhone and iPad layouts; deployment target iOS 15; arm64 package.

### Dangerous actions

itop can send SIGTERM, SIGKILL, SIGSTOP, or SIGCONT to the selected process, but the
read-only posture is kept for everything accidental:

- Nothing can be signalled from the normal UI: no swipes, no row long-press, and the
  row tap only selects. The bottom line normally never offers any signal zone.
- Sending is two-step. Tap the right-aligned `sig` tag in the Tasks bar (or press `K`)
  to **arm**: the tag reads `TERM ▸ KILL`, the whole terminal gets a bright red
  one-cell border, and the status line reads
  `ARMED: tap TERM or KILL below to signal PID <pid> · tap elsewhere to disarm`.
- Only while armed, the bottom line splits into hit zones: left `TERM` (yellow),
  center `STOP`/`CONT` (cyan), right `KILL` (red). The signal fires on this armed
  second tap and the state auto-disarms. Any other tap (the tags, header zones,
  rows, the border) disarms harmlessly; drags never fire. `Esc` also disarms, and
  while armed all other keys (S/F/R/H/Space/arrows) disarm instead of acting.
- The outcome prints in the status line as `SIGTERM -> 4486 (zsh): ok` or
  `failed: <errno message>` (e.g. `No such process` for a pid that exited).
- Prefer `TERM` and give the process a moment to exit; `KILL` is the last resort.
- Guard rails in code: itop refuses to signal itself, `launchd` (pid 1), and any
  broadcast form of kill(2) (`pid 0`, negative pids/pid -1). The guard is a pure
  function asserted by `--self-test`, which never calls kill(2) at all.
- Signing: the ad-hoc ldid signature adds a single `(allow signal)` entry to the
  existing sbpl exception list so kill(2) can reach other processes; every other
  entitlement is unchanged.

## Build, test, install

This source tree expects the Procursus jailbreak toolchain at `/var/jb`.

```sh
./build.sh test       # build app + Debian package, then run on-device telemetry smoke test
./build.sh install    # build and install package (root access required)
./build.sh clean
```

Package output: `itop_1.1.0_iphoneos-arm64.deb`. Installing registers the home-screen app named **itop**. If the icon has not appeared after installation, run `uicache -p /var/jb/Applications/itop.app` as root or respring.

The bottom function-key bar is gone; its actions are integrated into the UI and all remain tappable:

- Header row: `?` opens help, `R` re-samples now, and tapping the **FPS** readout holds or resumes sampling (it turns amber while held).
- Column titles are sort buttons: tap to sort by that column, tap again to flip `▼/▲` direction. The active column shows the arrow in green.
- Tasks bar: the right-aligned `search` tag opens the terminal filter prompt; while a filter is active it becomes `search:<text> ✕` where `✕` clears it. Immediately left of `search` (one dead gap cell) sits the `sig` arm tag for signals; see *Dangerous actions*.
- Row swiping, row selection, and the status line are unchanged. Hardware keys still work: S sort-cycle, F search, Space pause, R refresh, H/F1 help, Esc cancel, arrows to move selection, and K to arm/disarm signals (while armed: 1 TERM, 9 KILL, p STOP, c CONT).

## Display refresh note

itop requests the maximum refresh rate the current screen supports. The iPad 7,5 used for the device build has a 60 Hz display, so its physical maximum is 60 fps. ProMotion devices can run the display link up to 120 Hz. Metrics still sample at 1 Hz to keep overhead low; the cell grid redraws only when metrics or terminal state change.
