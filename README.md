# Kristal 

Minimal wlroots-based Wayland compositor.

## Building

Dependencies:
- wlroots
- wayland-protocols

```
meson setup build
ninja -C build
```

## Technical Details 

### Components
- backend/renderer/allocator
- seat
- cursor
- scene graph
- output layout
- xdg-shell listeners
- layer-shell listener
- optional wlroots protocol managers

## Configuration

Use environment variables before launching:

- `KRISTAL_OUTPUT_SCALE`: floating-point scale factor (default `1.0`)
- `KRISTAL_OUTPUT_LAYOUT`: `auto` (default), `horizontal`, or `vertical`

Example:

```bash
KRISTAL_OUTPUT_SCALE=1.5 KRISTAL_OUTPUT_LAYOUT=vertical ./build/kristal
```


## TODO

- [x] Xwayland support for X11 apps (`wlr_xwayland` wired up)
- [x] Output management protocol (runtime monitor enable/disable, modes, scale, position)
- [x] Input device coverage beyond keyboard/mouse: touch, tablet, gestures, switch
- [x] Keybinding system for common WM actions (launchers, move/resize, close, etc.)
- [x] Window layout/workspaces (workspaces + view cycling)
- [x] Client/server decorations policy (xdg-decoration) or custom titlebars
- [x] Activation protocol (xdg-activation) and focus-stealing rules
- [x] Pointer constraints + relative pointer (lock/confine pointer for games)
- [x] Idle + idle-inhibit handling (screensaver/lock integration)

Note: layer-shell support is built only when the wlroots protocol headers are
available (the `wlr-layer-shell-unstable-v1-protocol.h` header from
`wlr-protocols`). If missing, the compositor still builds but layer-shell is
disabled.
