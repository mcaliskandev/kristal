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


## TODO

- [ ] HiDPI support
- [ ] Any kind of configuration, e.g. output layout
- [ ] Any protocol other than xdg-shell (e.g. layer-shell, for
  panels/taskbars/etc; or Xwayland, for proxied X11 windows)
- [ ] Optional protocols, e.g. screen capture, primary selection, virtual
  keyboard, etc. Most of these are plug-and-play with wlroots, but they're
  omitted for brevity.
