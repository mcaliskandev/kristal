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
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
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
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_text_input_v3.h>
#ifdef __cplusplus
#define delete delete_
#endif
#include <wlr/types/wlr_input_method_v2.h>
#ifdef __cplusplus
#undef delete
#endif
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#ifdef KRISTAL_HAVE_XWAYLAND
#include <wlr/xwayland/xwayland.h>
#endif
#include <xkbcommon/xkbcommon.h>

typedef struct wlr_surface Surface;
typedef struct wl_display Display;
typedef struct wl_listener Listener;
typedef struct wl_list List;
typedef struct wlr_allocator Allocator;
typedef struct wlr_backend Backend;
typedef struct wlr_box Box;
typedef struct wlr_compositor Compositor;
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
typedef struct wlr_output_power_manager_v1 OutputPowerManagerV1;
typedef struct wlr_output_power_v1_set_mode_event OutputPowerSetModeEvent;
typedef struct wlr_gamma_control_manager_v1 GammaControlManagerV1;
typedef struct wlr_pointer_axis_event PointerAxisEvent;
typedef struct wlr_pointer_button_event PointerButtonEvent;
typedef struct wlr_pointer_motion_event PointerMotionEvent;
typedef struct wlr_pointer_motion_absolute_event PointerMotionAbsoluteEvent;
typedef struct wlr_pointer_constraints_v1 PointerConstraints;
typedef struct wlr_pointer_constraint_v1 PointerConstraint;
typedef struct wlr_pointer_gestures_v1 PointerGestures;
typedef struct wlr_relative_pointer_manager_v1 RelativePointerManager;
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
typedef struct wlr_tablet_v2_tablet TabletV2;
typedef struct wlr_tablet_v2_tablet_tool TabletToolV2;
typedef struct wlr_tablet_manager_v2 TabletManagerV2;
typedef struct wlr_touch_down_event TouchDownEvent;
typedef struct wlr_touch_up_event TouchUpEvent;
typedef struct wlr_touch_motion_event TouchMotionEvent;
typedef struct wlr_touch_cancel_event TouchCancelEvent;
typedef struct wlr_switch SwitchDevice;
typedef struct wlr_switch_toggle_event SwitchToggleEvent;
typedef struct wlr_xcursor_manager XcursorManager;
typedef struct wlr_xdg_activation_v1 XdgActivation;
typedef struct wlr_xdg_activation_v1_request_activate_event XdgActivateEvent;
typedef struct wlr_xdg_decoration_manager_v1 XdgDecorationManager;
typedef struct wlr_xdg_toplevel_decoration_v1 XdgToplevelDecoration;
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
typedef struct wlr_text_input_manager_v3 TextInputManagerV3;
typedef struct wlr_text_input_v3 TextInputV3;
typedef struct wlr_input_method_manager_v2 InputMethodManagerV2;
typedef struct wlr_input_method_v2 InputMethodV2;
typedef struct wlr_foreign_toplevel_manager_v1 ForeignToplevelManager;
typedef struct wlr_foreign_toplevel_handle_v1 ForeignToplevelHandle;
typedef struct wlr_session_lock_manager_v1 SessionLockManager;
typedef struct wlr_session_lock_v1 SessionLock;
typedef struct wlr_session_lock_surface_v1 SessionLockSurface;
typedef struct wlr_data_control_manager_v1 DataControlManagerV1;
#ifdef KRISTAL_HAVE_XWAYLAND
typedef struct wlr_xwayland Xwayland;
typedef struct wlr_xwayland_surface XwaylandSurface;
typedef struct wlr_xwayland_surface_configure_event XwaylandConfigureEvent;
typedef struct wlr_xwayland_resize_event XwaylandResizeEvent;
#endif

struct wlr_layer_shell_v1;
struct wlr_layer_surface_v1;
typedef struct wlr_layer_shell_v1 LayerShell;
typedef struct wlr_layer_surface_v1 LayerSurface;

enum CursorMode {
	CURSOR_PASSTHROUGH,
	CURSOR_MOVE,
	CURSOR_RESIZE,
};

enum KristalViewType {
	KRISTAL_VIEW_XDG,
	KRISTAL_VIEW_XWAYLAND,
};

enum OutputLayoutMode {
	OUTPUT_LAYOUT_AUTO,
	OUTPUT_LAYOUT_HORIZONTAL,
	OUTPUT_LAYOUT_VERTICAL,
};

enum WindowPlacementMode {
	WINDOW_PLACE_AUTO,
	WINDOW_PLACE_CENTER,
	WINDOW_PLACE_CASCADE,
};

enum WindowLayoutMode {
	WINDOW_LAYOUT_FLOATING,
	WINDOW_LAYOUT_STACK,
};

typedef struct KristalServer KristalServer;
typedef struct KristalOutput KristalOutput;
typedef struct KristalView KristalView;
typedef struct KristalToplevel KristalToplevel;
typedef struct KristalPopup KristalPopup;
typedef struct KristalKeyboard KristalKeyboard;
typedef struct KristalLayerSurface KristalLayerSurface;
typedef struct KristalXwaylandSurface KristalXwaylandSurface;
typedef struct KristalTablet KristalTablet;
typedef struct KristalTabletTool KristalTabletTool;
typedef struct KristalSwitch KristalSwitch;

struct KristalServer {
	Display *display;
	Backend *backend;
	Renderer *renderer;
	Allocator *allocator;
	Compositor *compositor;
	Scene *scene;
	SceneOutputLayout *scene_layout;

	XdgShell *xdg_shell;
	Listener new_xdg_toplevel;
	Listener new_xdg_popup;
	LayerShell *layer_shell;
	Listener new_layer_surface;
	List views;
	List layer_surfaces;

	Cursor *cursor;
	XcursorManager *cursor_mgr;
	Listener cursor_motion;
	Listener cursor_motion_absolute;
	Listener cursor_button;
	Listener cursor_axis;
	Listener cursor_frame;
	Listener cursor_swipe_begin;
	Listener cursor_swipe_update;
	Listener cursor_swipe_end;
	Listener cursor_pinch_begin;
	Listener cursor_pinch_update;
	Listener cursor_pinch_end;
	Listener cursor_hold_begin;
	Listener cursor_hold_end;
	Listener cursor_touch_down;
	Listener cursor_touch_up;
	Listener cursor_touch_motion;
	Listener cursor_touch_cancel;
	Listener cursor_touch_frame;
	Listener cursor_tablet_axis;
	Listener cursor_tablet_proximity;
	Listener cursor_tablet_tip;
	Listener cursor_tablet_button;

	Seat *seat;
	Listener new_input;
	Listener request_cursor;
	Listener request_set_selection;
	Surface *focused_surface;
	List keyboards;
	List tablets;
	List tablet_tools;
	List switches;
	enum CursorMode cursor_mode;
	int touch_device_count;
	KristalToplevel *grabbed_toplevel;
	KristalXwaylandSurface *grabbed_xwayland;
	double grab_x;
	double grab_y;
	Box grab_geobox;
	uint32_t resize_edges;

	OutputLayout *output_layout;
	List outputs;
	Listener new_output;
	float output_scale;
	int output_transform;
	enum OutputLayoutMode output_layout_mode;
	int next_output_x;
	int next_output_y;
	const char *output_config_path;
	enum WindowPlacementMode window_placement_mode;
	int next_window_x;
	int next_window_y;
	enum WindowLayoutMode window_layout_mode;
	XdgOutputManager *xdg_output_mgr;
	FractionalScaleManager *fractional_scale_mgr;
	PrimarySelectionManager *primary_selection_mgr;
	ScreencopyManager *screencopy_mgr;
	VirtualKeyboardManager *virtual_keyboard_mgr;
	TextInputManagerV3 *text_input_mgr;
	InputMethodManagerV2 *input_method_mgr;
	InputMethodV2 *input_method;
	TextInputV3 *active_text_input;
	ForeignToplevelManager *foreign_toplevel_mgr;
	DataControlManagerV1 *data_control_mgr;
	SessionLockManager *session_lock_mgr;
	SessionLock *session_lock;
	bool session_locked;
	SceneTree *lock_scene;
	List lock_surfaces;
	Listener new_text_input;
	Listener new_input_method;
	Listener input_method_commit;
	Listener input_method_destroy;
	Listener new_session_lock;
	Listener session_lock_destroy;
	Listener session_lock_unlock;
	Listener new_lock_surface;
	struct wlr_output_manager_v1 *output_manager;
	Listener output_manager_apply;
	Listener output_manager_test;
	XdgDecorationManager *decoration_mgr;
	Listener new_toplevel_decoration;
	XdgActivation *activation_mgr;
	Listener request_activate;
	PointerConstraints *pointer_constraints;
	Listener new_pointer_constraint;
	PointerConstraint *active_constraint;
	RelativePointerManager *relative_pointer_mgr;
	PointerGestures *pointer_gestures;
	struct wlr_idle_notifier_v1 *idle_notifier;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
	Listener new_idle_inhibitor;
	TabletManagerV2 *tablet_manager;
#ifdef KRISTAL_HAVE_XWAYLAND
	Xwayland *xwayland;
	Listener xwayland_ready;
	Listener xwayland_new_surface;
#endif
	int current_workspace;
	int workspace_count;
};

struct KristalOutput {
	List link;
	KristalServer *server;
	Output *wlr_output;
	Listener frame;
	Listener request_state;
	Listener destroy;
};

struct KristalView {
	List link;
	KristalServer *server;
	SceneTree *scene_tree;
	enum KristalViewType type;
	int workspace;
	bool mapped;
	ForeignToplevelHandle *foreign_toplevel;
};

struct KristalToplevel {
	KristalView view;
	XdgToplevel *xdg_toplevel;
	bool has_saved_geometry;
	Box saved_geometry;
	bool placed;
	Listener map;
	Listener unmap;
	Listener commit;
	Listener destroy;
	Listener request_move;
	Listener request_resize;
	Listener request_maximize;
	Listener request_fullscreen;
	Listener set_title;
	Listener set_app_id;
};

struct KristalPopup {
	XdgPopup *xdg_popup;
	Listener commit;
	Listener destroy;
};

#ifdef KRISTAL_HAVE_XWAYLAND
struct KristalXwaylandSurface {
	KristalView view;
	XwaylandSurface *xwayland_surface;
	Listener associate;
	Listener dissociate;
	Listener map;
	Listener unmap;
	Listener destroy;
	Listener request_move;
	Listener request_resize;
	Listener request_configure;
	Listener request_activate;
	Listener map_request;
	Listener set_title;
};
#endif

struct KristalKeyboard {
	List link;
	KristalServer *server;
	Keyboard *wlr_keyboard;
	Listener modifiers;
	Listener key;
	Listener destroy;
};

struct KristalTablet {
	List link;
	KristalServer *server;
	struct wlr_tablet *wlr_tablet;
	TabletV2 *tablet_v2;
	Listener destroy;
};

struct KristalTabletTool {
	List link;
	KristalServer *server;
	struct wlr_tablet_tool *wlr_tool;
	TabletToolV2 *tool_v2;
	Listener destroy;
};

struct KristalSwitch {
	List link;
	KristalServer *server;
	SwitchDevice *wlr_switch;
	Listener toggle;
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

struct KristalSessionLockSurface {
	List link;
	KristalServer *server;
	SessionLockSurface *lock_surface;
	SceneTree *scene_tree;
	Listener destroy;
};

void focus_toplevel(KristalToplevel *toplevel, Surface *surface);
void reset_cursor_mode(KristalServer *server);
void focus_surface(KristalServer *server, Surface *surface);
void server_apply_workspace(KristalServer *server, int workspace);
void server_move_focused_to_workspace(KristalServer *server, int workspace);
void server_close_focused(KristalServer *server);
void server_update_output_manager_config(KristalServer *server);
void server_arrange_workspace(KristalServer *server);
void server_text_input_focus(KristalServer *server, Surface *surface);
void server_register_foreign_toplevel(KristalView *view, const char *title, const char *app_id);
void server_update_foreign_toplevel(KristalView *view, const char *title, const char *app_id);
void server_unregister_foreign_toplevel(KristalView *view);

void server_new_output(Listener *listener, void *data);
void server_cursor_motion(Listener *listener, void *data);
void server_cursor_motion_absolute(Listener *listener, void *data);
void server_cursor_button(Listener *listener, void *data);
void server_cursor_axis(Listener *listener, void *data);
void server_cursor_frame(Listener *listener, void *data);
void server_cursor_swipe_begin(Listener *listener, void *data);
void server_cursor_swipe_update(Listener *listener, void *data);
void server_cursor_swipe_end(Listener *listener, void *data);
void server_cursor_pinch_begin(Listener *listener, void *data);
void server_cursor_pinch_update(Listener *listener, void *data);
void server_cursor_pinch_end(Listener *listener, void *data);
void server_cursor_hold_begin(Listener *listener, void *data);
void server_cursor_hold_end(Listener *listener, void *data);
void server_cursor_touch_down(Listener *listener, void *data);
void server_cursor_touch_up(Listener *listener, void *data);
void server_cursor_touch_motion(Listener *listener, void *data);
void server_cursor_touch_cancel(Listener *listener, void *data);
void server_cursor_touch_frame(Listener *listener, void *data);
void server_cursor_tablet_axis(Listener *listener, void *data);
void server_cursor_tablet_proximity(Listener *listener, void *data);
void server_cursor_tablet_tip(Listener *listener, void *data);
void server_cursor_tablet_button(Listener *listener, void *data);

void server_new_input(Listener *listener, void *data);
void seat_request_cursor(Listener *listener, void *data);
void seat_request_set_selection(Listener *listener, void *data);
void server_new_pointer_constraint(Listener *listener, void *data);
void server_output_manager_apply(Listener *listener, void *data);
void server_output_manager_test(Listener *listener, void *data);
void server_output_power_set_mode(Listener *listener, void *data);
void server_new_toplevel_decoration(Listener *listener, void *data);
void server_request_activate(Listener *listener, void *data);
void server_new_idle_inhibitor(Listener *listener, void *data);
void server_new_text_input(Listener *listener, void *data);
void server_new_input_method(Listener *listener, void *data);
void server_input_method_commit(Listener *listener, void *data);
void server_input_method_destroy(Listener *listener, void *data);
void server_new_session_lock(Listener *listener, void *data);
void server_session_lock_destroy(Listener *listener, void *data);
void server_session_lock_unlock(Listener *listener, void *data);
void server_new_lock_surface(Listener *listener, void *data);
#ifdef KRISTAL_HAVE_XWAYLAND
void server_xwayland_ready(Listener *listener, void *data);
void server_new_xwayland_surface(Listener *listener, void *data);
#endif

void server_new_xdg_toplevel(Listener *listener, void *data);
void server_new_xdg_popup(Listener *listener, void *data);
void server_new_layer_surface(Listener *listener, void *data);

#ifdef __cplusplus
}
#endif
