# itop

A native, rootless-jailbreak iOS/iPadOS system monitor rendered as a full-screen, fixed-cell terminal grid. Every visible element is monospace text: box-drawing characters, terminal meters, process rows, and an htop-style function-key line. There are no cards, oversized dashboard labels, or native alert dialogs. Metrics are sampled once per second on a background queue; a lightweight display link requests the screen's maximum supported refresh and pauses when backgrounded.

## Features

- Overall and per-core CPU load with a short history graph.
- Physical memory, wired/compressed memory, root filesystem usage, uptime, and battery.
- Read-only process list with CPU and resident memory, sorting, name/PID filtering, row selection, and scrolling.
- Diagnostics panel: itop energy use, thermal state, Wi-Fi/cellular RX/TX rates,
  collection time, and jetsam headroom for this process (shown as n/a when the
  runtime does not provide a value). Selecting a row adds per-process energy
  plus fault, page-in, context-switch, and syscall rates in the status line,
  and shows per-process disk read/write rates for the selected row (cumulative
  totals on wide layouts; narrow layouts drop the least important rate fields
  rather than cutting numbers).
- Device RX/TX sparklines: when the compact layout leaves a spare CPU-panel
  interior row (≤ 3 exposed cores; 2 on the test device), a `RX ▂▅█▄▂▁▃  TX ▁▂▄▆▃▁▂` row
  built from the same ▁▂▃▄▅▆▇█ sparkline charset as the CPU history draws
  under it in green/cyan, log-scaled (`clamp(round(log10(rate+1) /
  log10(100000) * 7), 0..7)`, so 100 KB/s fills the bar and each step is ~5.2x),
  over a 64 one-second-sample ring pre-filled with zeros; zero rates render the
  baseline bar instead of collapsing the row. The wide layout's panel is already
  full (cores + 5 rows), so it silently skips the row instead of growing it.
- Per-process jetsam limits (LIM) and device pressure level (Prs) — n/a where
  iOS denies access. Prs renders as a 0-100 meter in the Mem panel under the
  jetsam-headroom row; LIM appears (honestly, never fabricated) at the end of
  the selected-row detail line.
- Terminal key bar, touch row selection, swipe scrolling, and hardware keyboard shortcuts.
- Process information entitlements are limited to enumeration/inspection; itop does not kill or modify processes.
- iPhone and iPad layouts; deployment target iOS 15; arm64 package.

## memorystatus ABI audit (Prs / LIM)

Device pressure is read with `SYS_memorystatus_get_level` (trap 453) and the
priority list is probed, observationally, with `memorystatus_control`
(trap 440). Evidence checked on the build device:

- `/var/jb/usr/share/SDKs/iPhoneOS.sdk/usr/include/sys/syscall.h:480`
  defines `SYS_memorystatus_control 440`; line 493 defines
  `SYS_memorystatus_get_level 453`.
- `/var/jb/usr/share/SDKs/iPhoneOS.sdk/usr/lib/libSystem.B.tbd:2653`
  exports `_memorystatus_control`, and line 2654 exports
  `_memorystatus_get_level`, so itop links the C wrapper rather than guessing
  a symbol's existence.
- No `sys/kern_memorystatus.h` (or any `memorystatus_priority_entry_t`
  definition) exists under `/var/jb/usr/include` or the SDK includes — a
  grep across `/var/jb` finds nothing but the two headers above.

Because the priority-list **entry struct is unverifiable on this device**, itop
calls the list command only in `--self-test` and prints the raw return/errno;
it never parses entries into fake field meanings. Per-process limits therefore
show `LIM n/a` until a toolchain ships the kernel header, and the pressure
level (no struct involved) does ship. Unfilled expectations are always n/a,
never zero — a missing reading must be distinguishable from a reading of 0.

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
