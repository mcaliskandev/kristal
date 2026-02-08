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

- `KRISTAL_CONFIG`: optional config file path (default: `$XDG_CONFIG_HOME/kristal/kristal.conf` or `$HOME/.config/kristal/kristal.conf`)
- `KRISTAL_OUTPUT_SCALE`: floating-point scale factor (default `1.0`)
- `KRISTAL_OUTPUT_LAYOUT`: `auto` (default), `horizontal`, or `vertical`
- `KRISTAL_OUTPUTS_STATE`: path to persist per-output scale/transform/position
- `KRISTAL_OUTPUT_TRANSFORM`: `normal` (default), `90`, `180`, `270`, `flipped`, `flipped-90`, `flipped-180`, `flipped-270`
- `KRISTAL_WINDOW_PLACEMENT`: `auto` (default), `center`, or `cascade`
- `KRISTAL_WINDOW_LAYOUT`: `floating` (default), `stack`, `grid`, or `monocle`
- `KRISTAL_WINDOW_RULES`: semicolon-separated rules (e.g. `app_id=firefox,workspace=2,floating=1;title=Editor,workspace=3`)
- `KRISTAL_XKB_RULES`: XKB rules (optional)
- `KRISTAL_XKB_MODEL`: XKB model (optional)
- `KRISTAL_XKB_LAYOUT`: XKB layout (optional)
- `KRISTAL_XKB_VARIANT`: XKB variant (optional)
- `KRISTAL_XKB_OPTIONS`: XKB options (optional)
- `KRISTAL_KEY_REPEAT_RATE`: key repeat rate in Hz (default `25`)
- `KRISTAL_KEY_REPEAT_DELAY`: key repeat delay in ms (default `600`)
- `KRISTAL_BINDINGS`: semicolon-separated keybindings (e.g. `Alt+Return=terminal;Alt+1=ws1`)
- `KRISTAL_TAP_TO_CLICK`: `0`/`1` (default `0`)
- `KRISTAL_NATURAL_SCROLL`: `0`/`1` (default `0`)
- `KRISTAL_POINTER_ACCEL`: pointer acceleration speed `-1.0..1.0` (default `0.0`)

Example:

```bash
KRISTAL_OUTPUT_SCALE=1.5 KRISTAL_OUTPUT_LAYOUT=vertical ./build/kristal
```

Config files accept the same `KRISTAL_*` keys, one per line as `KEY=VALUE` (lines starting with `#` or `;` are ignored).
Send `SIGHUP` to reload at runtime.
Keybinding actions include `layout-floating`, `layout-stack`, `layout-grid`, `layout-monocle`, and `layout-cycle`.

*Note:* layer-shell support is built only when the wlroots protocol headers are available (the `wlr-layer-shell-unstable-v1-protocol.h` header from `wlr-protocols`). If missing, the compositor still builds but layer-shell is disabled.

## TODO

- [x] Add a real config file with reloadable settings (instead of only env vars).
- [x] Make keybindings configurable (currently hardcoded Alt+... bindings).
- [x] Expand window management: keyboard move/resize, better focus cycling, window rules.
- [x] Add more layout options (beyond floating/stack) and per-workspace layout state.
- [ ] Add server-side decorations and theming controls.
- [x] Add output power management (DPMS) and gamma/brightness controls.
