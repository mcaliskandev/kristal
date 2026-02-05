#ifndef SERVER_HPP
#define SERVER_HPP

#include "internal.h"

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
