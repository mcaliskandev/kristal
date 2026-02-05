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

- [x] HiDPI support
- [x] Any kind of configuration, e.g. output layout
- [x] Any protocol other than xdg-shell (e.g. layer-shell, for
  panels/taskbars/etc; or Xwayland, for proxied X11 windows)
- [x] Optional protocols, e.g. screen capture, primary selection, virtual keyboard, etc.

Note: layer-shell support is built only when the wlroots protocol headers are
available (the `wlr-layer-shell-unstable-v1-protocol.h` header from
`wlr-protocols`). If missing, the compositor still builds but layer-shell is
disabled.
