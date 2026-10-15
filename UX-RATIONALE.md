# UX rationale — memory pressure level (Prs) and per-process jetsam limit (LIM)

Branch `mem-limits` extends the memory story itop already told (hardware RAM in
the Mem panel, `Jtsm` headroom for itop itself, per-process `RES`) with the two
jetsam views that are actually accessible from userspace: the device-wide
pressure level and, conditionally, per-process jetsam limits.

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
   evidence, and this branch refuses to conjure offsets for
   `{pid, priority, user_data, limit_bytes, ...}` from memory. The guess in the
   task brief (`command[256]`, `state`) described a different structure family
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
