# itop

A native, rootless-jailbreak iOS/iPadOS system monitor rendered as a full-screen, fixed-cell terminal grid. Every visible element is monospace text: box-drawing characters, terminal meters, process rows, and an htop-style function-key line. There are no cards, oversized dashboard labels, or native alert dialogs. Metrics are sampled once per second on a background queue; a lightweight display link requests the screen's maximum supported refresh and pauses when backgrounded.

## Features

- Overall and per-core CPU load with a short history graph.
- Physical memory, wired/compressed memory, root filesystem usage, uptime, and battery.
- Read-only process list with CPU and resident memory, sorting, name/PID filtering, row selection, and scrolling.
- Persisted view settings: the active sort key and direction and the filter text are saved to `NSUserDefaults`
  (`itop.sort.key`, `itop.sort.desc`, `itop.filter`) and validated on load, so a relaunch keeps the user's view.
  Paused state is deliberately transient and is not persisted.
- Diagnostics panel: itop energy use, thermal state, Wi-Fi/cellular RX/TX rates,
  collection time, and jetsam headroom for this process (shown as n/a when the
  runtime does not provide a value). Selecting a row adds per-process energy
  plus fault, page-in, context-switch, and syscall rates in the status line.
- Terminal key bar, touch row selection, swipe scrolling, and hardware keyboard shortcuts.
- Process information entitlements are limited to enumeration/inspection; itop does not kill or modify processes.
- iPhone and iPad layouts; deployment target iOS 15; arm64 package.

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
- Tasks bar: the right-aligned `search` tag opens the terminal filter prompt;
  while a filter is active it becomes `search:<text> ✕` — tapping the tag reopens
  the prompt pre-seeded with the current text for in-place editing (htop F4
  style), and tapping `✕` clears the filter without raising the keyboard.
- Row swiping, row selection, and the status line are unchanged. Hardware keys still work: S sort-cycle, F search, J jump, Space pause, R refresh, H/F1 help, Esc cancel, arrows to move selection, PgUp/PgDn to scroll one page, and Home/End to jump to the top or bottom of the list.
- Pressing `J` opens a terminal jump prompt in the status-line region: type a PID and press Return to select it and scroll it into view, or Esc to cancel. Typing an unknown PID reports `no such pid` in the hint. Scrolling still works while the prompt is open.

## Display refresh note

itop requests the maximum refresh rate the current screen supports. The iPad 7,5 used for the device build has a 60 Hz display, so its physical maximum is 60 fps. ProMotion devices can run the display link up to 120 Hz. Metrics still sample at 1 Hz to keep overhead low; the cell grid redraws only when metrics or terminal state change.
