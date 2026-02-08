#ifndef SERVER_HPP
#define SERVER_HPP

#include "core/internal.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

struct ServerComponents {
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
	OutputPowerManagerV1 *output_power_mgr;
	Listener output_power_set_mode;
	GammaControlManagerV1 *gamma_control_mgr;
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

class KristalCompositor 
{
public:
    KristalCompositor();
    void Create();
    int Run(const std::string &startup_cmd);
    void CreateDisplay();
    void CreateBackend();
    void CreateRenderer();
    void CreateAllocator();
    void CreateOutputLayer();

    void Destroy();

private:
    std::unique_ptr<ServerComponents> components;
};

#endif // SERVER_HPP
