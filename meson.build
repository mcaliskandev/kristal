project('kristal', 'cpp', version : '0.1')

# Helper to run pkg-config and expose the handy variables we need.
pkgconfig = 'pkg-config'
wayland_protocols_data = run_command(pkgconfig, '--variable=pkgdatadir',
	'wayland-protocols', check : true).stdout().strip()
wayland_scanner_bin = run_command(pkgconfig, '--variable=wayland_scanner',
	'wayland-scanner', check : true).stdout().strip()

xdg_shell_xml = join_paths(wayland_protocols_data, 'stable', 'xdg-shell',
	'xdg-shell.xml')
pointer_constraints_xml = join_paths(wayland_protocols_data, 'unstable',
	'pointer-constraints', 'pointer-constraints-unstable-v1.xml')
tablet_v2_xml = join_paths(wayland_protocols_data, 'unstable',
	'tablet', 'tablet-unstable-v2.xml')

xdg_shell_protocol_header = custom_target('xdg-shell-protocol-header',
	output : 'xdg-shell-protocol.h',
	command : [wayland_scanner_bin, 'server-header', xdg_shell_xml, '@OUTPUT@'],
	capture : false,
)
pointer_constraints_protocol_header = custom_target(
	'pointer-constraints-unstable-v1-protocol-header',
	output : 'pointer-constraints-unstable-v1-protocol.h',
	command : [
		wayland_scanner_bin,
		'server-header',
		pointer_constraints_xml,
		'@OUTPUT@'
	],
	capture : false,
)
tablet_v2_protocol_header = custom_target('tablet-v2-protocol-header',
	output : 'tablet-v2-protocol.h',
	command : [wayland_scanner_bin, 'server-header', tablet_v2_xml, '@OUTPUT@'],
	capture : false,
)

wlroots_dep = dependency('wlroots-0.18')
wayland_server_dep = dependency('wayland-server')
xkb_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput')
have_layer_shell = meson.get_compiler('cpp').has_header('wlr-layer-shell-unstable-v1-protocol.h')
layer_shell_sources = []
cpp_extra_args = ['-DWLR_USE_UNSTABLE']
if have_layer_shell
  layer_shell_sources += ['src/shells/LayerShell.cpp']
  cpp_extra_args += ['-DKRISTAL_HAVE_LAYER_SHELL=1']
endif
have_xwayland = meson.get_compiler('cpp').has_header('xcb/xcb_ewmh.h') and
  meson.get_compiler('cpp').has_header('xcb/xcb_icccm.h')
xwayland_sources = []
if have_xwayland
  xwayland_sources += ['src/shells/Xwayland.cpp']
  cpp_extra_args += ['-DKRISTAL_HAVE_XWAYLAND=1']
endif

executable('kristal',
    sources : ['src/main.cpp', 'src/core/Server.cpp',
        'src/input/Input.cpp', 'src/input/Cursor.cpp', 'src/outputs/Output.cpp',
        'src/shells/Xdg.cpp', 'src/protocols/Protocols.cpp',
        xdg_shell_protocol_header, pointer_constraints_protocol_header,
		tablet_v2_protocol_header] + layer_shell_sources + xwayland_sources,
    dependencies : [wlroots_dep, wayland_server_dep, xkb_dep, libinput_dep],
    c_args : ['-DWLR_USE_UNSTABLE'],
    cpp_args : cpp_extra_args,
    install : true,
)
