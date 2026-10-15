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
- Device RX/TX sparklines: when the compact layout leaves a spare CPU-panel
  interior row (≤ 3 exposed cores; 2 on the test device), a `RX ▂▅█▄▂▁▃  TX ▁▂▄▆▃▁▂` row
  built from the same ▁▂▃▄▅▆▇█ sparkline charset as the CPU history draws
  under it in green/cyan, log-scaled (`clamp(round(log10(rate+1) /
  log10(100000) * 7), 0..7)`, so 100 KB/s fills the bar and each step is ~5.2x),
  over a 64 one-second-sample ring pre-filled with zeros; zero rates render the
  baseline bar instead of collapsing the row. The wide layout's panel is already
  full (cores + 5 rows), so it silently skips the row instead of growing it.
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
- Tasks bar: the right-aligned `search` tag opens the terminal filter prompt; while a filter is active it becomes `search:<text> ✕` where `✕` clears it.
- Row swiping, row selection, and the status line are unchanged. Hardware keys still work: S sort-cycle, F search, Space pause, R refresh, H/F1 help, Esc cancel, arrows to move selection.

## Per-process network bytes: on-device audit

The `Net` readout is device-wide RX/TX from `getifaddrs` (`en*`/`pdp_ip*`
interfaces); it is not per-process. An on-device audit (2026-10-15) found no
verifiable per-process byte accounting source, so itop does not display one:

- `struct rusage_info_v6` (`sys/resource.h` lines 368–413 in the on-device
  toolchain) exposes CPU/QoS times, wakeups, pageins, footprint, disk I/O,
  energy, and instruction/cycle counters — no network field in any v4/v5/v6
  variant.
- `<net/ntstat.h>` is absent and no `nts_`/`ntstat` symbol appears in either
  on-device include tree; no `libnetstat` ships in `/var/jb/usr/lib`, so the
  libnetstat per-stream ABI cannot be verified.
- `<sys/proc_info.h>` and `libproc.h` are absent from both include trees, so
  the `PROC_PIDFDSOCKETINFO`/`struct socket_info` field offsets cannot be
  verified. Inventing those layouts would risk fabricated numbers, which itop
  will not ship.

Revisit if a verifiable source appears on-device; the intended design is
cumulative bytes-since-launch sampled lazily for the selected row only (at
most every 5 s) and shown as `NET <rx>/<tx>` in the status line.

## Display refresh note

itop requests the maximum refresh rate the current screen supports. The iPad 7,5 used for the device build has a 60 Hz display, so its physical maximum is 60 fps. ProMotion devices can run the display link up to 120 Hz. Metrics still sample at 1 Hz to keep overhead low; the cell grid redraws only when metrics or terminal state change.
