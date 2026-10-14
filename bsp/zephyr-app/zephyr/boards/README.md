# Board extensions

[Board extensions](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html#board-extensions)
for the boards this VEE Port supports, laid out as `<vendor>/<board>/`. Made visible to the
build by `board_root` in `../module.yml`.

Use this only for board-level settings that an application `.conf` cannot express, because a
`.conf` can only *assign* a value:

- `board.yml` — declares the extension (`extend: <upstream board>`). Required.
- `Kconfig.defconfig` — Kconfig *defaults* conditional on the hardware present
  ([reference](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html#default-board-configuration)).
- `board.cmake` — default flash/debug runner; see the comments in the file for what is
  ignored at this point of the build.

Plain application configuration — Kconfig values and devicetree — goes in
[`../../main/boards/`](../../main/boards/README.md) instead.

---

_Markdown_  
_Copyright 2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
_Build: 7E4D1F7C_
