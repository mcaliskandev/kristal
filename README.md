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
- `KRISTAL_OUTPUTS_STATE`: path to persist per-output scale/transform/position
- `KRISTAL_OUTPUT_TRANSFORM`: `normal` (default), `90`, `180`, `270`, `flipped`, `flipped-90`, `flipped-180`, `flipped-270`
- `KRISTAL_WINDOW_PLACEMENT`: `auto` (default), `center`, or `cascade`
- `KRISTAL_WINDOW_LAYOUT`: `floating` (default) or `stack`

Example:

```bash
KRISTAL_OUTPUT_SCALE=1.5 KRISTAL_OUTPUT_LAYOUT=vertical ./build/kristal
```


## TODO

Implemented:
- [x] Xwayland support for X11 apps (`wlr_xwayland` wired up)
- [x] Output management protocol (runtime monitor enable/disable, modes, scale, position)
- [x] Input device coverage beyond keyboard/mouse: touch, tablet, gestures, switch
- [x] Keybinding system for common WM actions (launcher, cycle, close, workspace switch/move)
- [x] Window workspaces (workspaces + view cycling)
- [x] Client/server decorations policy (xdg-decoration, client-side enforced)
- [x] Activation protocol (xdg-activation) and focus-stealing rules
- [x] Pointer constraints + relative pointer (lock/confine pointer for games)
- [x] Idle + idle-inhibit handling (screensaver/lock integration)

Missing (main compositor features):
- [ ] Window placement policy (initial position, centering, and simple rules)
- [ ] Interactive move/resize bindings (e.g., mod+drag, mod+right-drag)
- [ ] Tiling layout option or at least simple auto-tiling
- [ ] Per-output configuration persistence (scale/transform/position saved and restored)
- [ ] Output transforms (rotation/flip) exposed via config or management protocol
- [ ] Input configuration (keymap/layout, repeat rate, libinput options like tap-to-click)
- [ ] Text input / IME support (`zwp_text_input_v3`, virtual keyboard integration)
- [ ] Foreign toplevel management (`wlr-foreign-toplevel-management`) for panels/task switchers
- [ ] Data-control protocol (`wlr-data-control`) for clipboard managers
- [ ] Session lock protocol (`ext-session-lock-v1`) for screen lock integration

Note: layer-shell support is built only when the wlroots protocol headers are
available (the `wlr-layer-shell-unstable-v1-protocol.h` header from
`wlr-protocols`). If missing, the compositor still builds but layer-shell is
disabled.
