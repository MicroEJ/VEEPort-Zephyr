# Per-board application configuration

Board-specific configuration of the `main` application, picked up automatically by Zephyr:

- `<board_target>.conf` — Kconfig fragment merged on top of `../prj.conf`
  ([reference](https://docs.zephyrproject.org/latest/build/kconfig/setting.html)).
- `<board_target>.overlay` — devicetree overlay
  ([reference](https://docs.zephyrproject.org/latest/build/dts/howtos.html#set-devicetree-overlays)).

Plus one file wired by [`../../cmake/board_linker_sections.cmake`](../../cmake/board_linker_sections.cmake)
(included from `../CMakeLists.txt`) rather than discovered by Zephyr:

- `<board_target>.ld` — optional extra linker sections, appended through
  `zephyr_linker_sources(SECTIONS ...)` — for instance to place a buffer in a specific
  memory region. No `_<revision>` variant.

The file name is the [board target](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html#board-terminology)
with every `/` replaced by `_` (`mimxrt1170_evk/mimxrt1176/cm7` →
`mimxrt1170_evk_mimxrt1176_cm7`). On `.conf` and `.overlay`, appending `_<revision>`
restricts the file to a single
[board revision](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html#multiple-board-revisions).

Board-level Kconfig *defaults* conditional on the hardware present belong in a board
extension, see [`../../boards/`](../../boards/README.md).

---

_Markdown_  
_Copyright 2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
_Build: 7E4D1F7C_
