#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#ifdef __cplusplus
#define static
#endif
#include <wlr/types/wlr_scene.h>
#ifdef __cplusplus
#undef static
#endif
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

typedef struct wlr_surface Surface;
typedef struct wl_display Display;
typedef struct wl_listener Listener;
typedef struct wl_list List;
typedef struct wlr_allocator Allocator;
typedef struct wlr_backend Backend;
typedef struct wlr_box Box;
typedef struct wlr_cursor Cursor;
typedef struct wlr_input_device InputDevice;
typedef struct wlr_keyboard Keyboard;
typedef struct wlr_keyboard_key_event KeyboardKeyEvent;
typedef struct wlr_output Output;
typedef struct wlr_output_event_request_state OutputEventRequestState;
typedef struct wlr_output_layout OutputLayout;
typedef struct wlr_output_layout_output OutputLayoutOutput;
typedef struct wlr_output_mode OutputMode;
typedef struct wlr_output_state OutputState;
typedef struct wlr_pointer_axis_event PointerAxisEvent;
typedef struct wlr_pointer_button_event PointerButtonEvent;
typedef struct wlr_pointer_motion_event PointerMotionEvent;
typedef struct wlr_pointer_motion_absolute_event PointerMotionAbsoluteEvent;
typedef struct wlr_renderer Renderer;
typedef struct wlr_scene Scene;
typedef struct wlr_scene_buffer SceneBuffer;
typedef struct wlr_scene_layer_surface_v1 SceneLayerSurface;
typedef struct wlr_scene_node SceneNode;
typedef struct wlr_scene_output SceneOutput;
typedef struct wlr_scene_output_layout SceneOutputLayout;
typedef struct wlr_scene_surface SceneSurface;
typedef struct wlr_scene_tree SceneTree;
typedef struct wlr_seat Seat;
typedef struct wlr_seat_client SeatClient;
typedef struct wlr_seat_pointer_request_set_cursor_event SeatPointerRequestSetCursorEvent;
typedef struct wlr_seat_request_set_selection_event SeatRequestSetSelectionEvent;
typedef struct wlr_xcursor_manager XcursorManager;
typedef struct wlr_xdg_output_manager_v1 XdgOutputManager;
typedef struct wlr_xdg_popup XdgPopup;
typedef struct wlr_xdg_shell XdgShell;
typedef struct wlr_xdg_surface XdgSurface;
typedef struct wlr_xdg_toplevel XdgToplevel;
typedef struct wlr_xdg_toplevel_resize_event XdgToplevelResizeEvent;
typedef struct wlr_fractional_scale_manager_v1 FractionalScaleManager;
typedef struct wlr_primary_selection_v1_device_manager PrimarySelectionManager;
typedef struct wlr_screencopy_manager_v1 ScreencopyManager;
typedef struct wlr_virtual_keyboard_manager_v1 VirtualKeyboardManager;

struct wlr_layer_shell_v1;
struct wlr_layer_surface_v1;
typedef struct wlr_layer_shell_v1 LayerShell;
typedef struct wlr_layer_surface_v1 LayerSurface;

enum CursorMode {
	CURSOR_PASSTHROUGH,
	CURSOR_MOVE,
	CURSOR_RESIZE,
};

enum OutputLayoutMode {
	OUTPUT_LAYOUT_AUTO,
	OUTPUT_LAYOUT_HORIZONTAL,
	OUTPUT_LAYOUT_VERTICAL,
};

typedef struct KristalServer KristalServer;
typedef struct KristalOutput KristalOutput;
typedef struct KristalToplevel KristalToplevel;
typedef struct KristalPopup KristalPopup;
typedef struct KristalKeyboard KristalKeyboard;
typedef struct KristalLayerSurface KristalLayerSurface;

struct KristalServer {
	Display *display;
	Backend *backend;
	Renderer *renderer;
	Allocator *allocator;
	Scene *scene;
	SceneOutputLayout *scene_layout;

	XdgShell *xdg_shell;
	Listener new_xdg_toplevel;
	Listener new_xdg_popup;
	LayerShell *layer_shell;
	Listener new_layer_surface;
	List toplevels;
	List layer_surfaces;

	Cursor *cursor;
	XcursorManager *cursor_mgr;
	Listener cursor_motion;
	Listener cursor_motion_absolute;
	Listener cursor_button;
	Listener cursor_axis;
	Listener cursor_frame;

	Seat *seat;
	Listener new_input;
	Listener request_cursor;
	Listener request_set_selection;
	List keyboards;
	enum CursorMode cursor_mode;
	KristalToplevel *grabbed_toplevel;
	double grab_x;
	double grab_y;
	Box grab_geobox;
	uint32_t resize_edges;

	OutputLayout *output_layout;
	List outputs;
	Listener new_output;
	float output_scale;
	enum OutputLayoutMode output_layout_mode;
	int next_output_x;
	int next_output_y;
	XdgOutputManager *xdg_output_mgr;
	FractionalScaleManager *fractional_scale_mgr;
	PrimarySelectionManager *primary_selection_mgr;
	ScreencopyManager *screencopy_mgr;
	VirtualKeyboardManager *virtual_keyboard_mgr;
};

struct KristalOutput {
	List link;
	KristalServer *server;
	Output *wlr_output;
	Listener frame;
	Listener request_state;
	Listener destroy;
};

struct KristalToplevel {
	List link;
	KristalServer *server;
	XdgToplevel *xdg_toplevel;
	SceneTree *scene_tree;
	bool has_saved_geometry;
	Box saved_geometry;
	Listener map;
	Listener unmap;
	Listener commit;
	Listener destroy;
	Listener request_move;
	Listener request_resize;
	Listener request_maximize;
	Listener request_fullscreen;
};

struct KristalPopup {
	XdgPopup *xdg_popup;
	Listener commit;
	Listener destroy;
};

struct KristalKeyboard {
	List link;
	KristalServer *server;
	Keyboard *wlr_keyboard;
	Listener modifiers;
	Listener key;
	Listener destroy;
};

struct KristalLayerSurface {
	List link;
	KristalServer *server;
	LayerSurface *layer_surface;
	SceneLayerSurface *scene_layer_surface;
	Listener map;
	Listener unmap;
	Listener commit;
	Listener destroy;
};

void focus_toplevel(KristalToplevel *toplevel, Surface *surface);
void reset_cursor_mode(KristalServer *server);

void server_new_output(Listener *listener, void *data);
void server_cursor_motion(Listener *listener, void *data);
void server_cursor_motion_absolute(Listener *listener, void *data);
void server_cursor_button(Listener *listener, void *data);
void server_cursor_axis(Listener *listener, void *data);
void server_cursor_frame(Listener *listener, void *data);

void server_new_input(Listener *listener, void *data);
void seat_request_cursor(Listener *listener, void *data);
void seat_request_set_selection(Listener *listener, void *data);

void server_new_xdg_toplevel(Listener *listener, void *data);
void server_new_xdg_popup(Listener *listener, void *data);
void server_new_layer_surface(Listener *listener, void *data);

#ifdef __cplusplus
}
#endif
