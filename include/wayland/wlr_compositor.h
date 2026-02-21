#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "entt.hpp"
#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>

struct Engine;
class EntityRegistry;
class WaylandApp;

struct wl_display;
struct wlr_backend;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_output;
struct wlr_swapchain;
struct wlr_input_device;
struct wlr_keyboard;
struct wlr_pointer;
struct wlr_cursor;
struct wlr_seat;
struct wlr_output_layout;
struct wlr_xdg_output_manager_v1;
struct wlr_layer_shell_v1;
struct wlr_xdg_shell;
struct wlr_data_device_manager;
struct wlr_screencopy_manager_v1;
struct wlr_compositor;
struct wlr_xcursor_manager;
struct wlr_surface;
struct wlr_xdg_surface;

enum class HotkeyModifier { Super, Alt };

struct WlrServer;

struct WlrOutputHandle {
  WlrServer* server = nullptr;
  wlr_output* output = nullptr;
  wlr_swapchain* swapchain = nullptr;
  wl_listener frame;
  wl_listener destroy;
  int buffer_age = -1;
  unsigned int depth_rbo = 0;
  int depth_width = 0;
  int depth_height = 0;
};

struct WlrKeyboardHandle {
  WlrServer* server = nullptr;
  wlr_keyboard* keyboard = nullptr;
  wl_listener key;
  wl_listener modifiers;
  wl_listener destroy;
};

struct WlrPointerHandle {
  WlrServer* server = nullptr;
  wlr_pointer* pointer = nullptr;
  wl_listener motion;
  wl_listener motion_abs;
  wl_listener button;
  wl_listener axis;
  wl_listener destroy;
};

struct InputState {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  double delta_x = 0.0;
  double delta_y = 0.0;
  bool have_abs = false;
  double last_abs_x = 0.0;
  double last_abs_y = 0.0;
  bool mouse_buttons[3] = { false, false, false };
  std::unordered_set<uint32_t> pressed_keys;
};

struct PendingWlAction {
  enum Type { Add, Remove } type;
  std::shared_ptr<WaylandApp> app;
  wlr_surface* surface = nullptr;
  bool accessory = false;
  bool layer_shell = false;
  bool menu_surface = false;
  wlr_surface* parent_surface = nullptr;
  int offset_x = 0;
  int offset_y = 0;
  int screen_x = 0;
  int screen_y = 0;
  int screen_w = 0;
  int screen_h = 0;
};

struct ReplayKeyRelease {
  xkb_keysym_t sym = XKB_KEY_NoSymbol;
  uint32_t release_time_msec = 0;
  uint32_t keycode = 0;
  bool update_mods = false;
};

struct WlrServer {
  wl_display* display = nullptr;
  wlr_backend* backend = nullptr;
  wlr_renderer* renderer = nullptr;
  wlr_allocator* allocator = nullptr;
  wl_listener new_output;
  wl_listener new_input;
  wl_listener new_xdg_surface;
  wl_listener new_layer_surface;
  std::unique_ptr<Engine> engine;
  std::shared_ptr<EntityRegistry> registry;
  bool gladLoaded = false;
  double lastFrameTime = 0.0;
  char** envp = nullptr;
  InputState input;
  wlr_compositor* compositor = nullptr;
  wlr_xdg_shell* xdg_shell = nullptr;
  wlr_seat* seat = nullptr;
  wlr_data_device_manager* data_device_manager = nullptr;
  wlr_screencopy_manager_v1* screencopy_manager = nullptr;
  wlr_output_layout* output_layout = nullptr;
  wlr_xdg_output_manager_v1* xdg_output_manager = nullptr;
  wlr_layer_shell_v1* layer_shell = nullptr;
  wlr_output* primary_output = nullptr;
  wlr_input_device* last_keyboard_device = nullptr;
  wlr_input_device* last_pointer_device = nullptr;
  wlr_keyboard* virtual_keyboard = nullptr;
  wlr_xcursor_manager* cursor_mgr = nullptr;
  wlr_cursor* cursor = nullptr;
  bool cursor_visible = false;
  HotkeyModifier hotkeyModifier = HotkeyModifier::Alt;
  uint32_t hotkeyModifierMask = WLR_MODIFIER_ALT;
  bool replayModifierActive = false;
  int replayModifierHeld = 0;
  int replayShiftHeld = 0;
  bool pendingReplayShift = false;
  bool pendingReplayModifier = false;
  uint32_t pendingReplayModifierKeycode = 0;
  xkb_keysym_t pendingReplayModifierSym = XKB_KEY_NoSymbol;
  std::vector<ReplayKeyRelease> pendingReplayKeyups;
  std::unordered_map<wlr_surface*, entt::entity> surface_map;
  std::vector<wlr_xdg_surface*> pending_surfaces;
  std::vector<PendingWlAction> pending_wl_actions;
  bool isX11Backend = false;
  double pointer_x = 0.0;
  double pointer_y = 0.0;
  wl_listener request_set_cursor;
};

// Helper APIs exposed for the main entry.
HotkeyModifier parse_hotkey_modifier();
uint32_t hotkey_modifier_mask(HotkeyModifier modifier);
const char* hotkey_modifier_label(HotkeyModifier modifier);
void delay_if_launching_nested_from_hackmatrix(int argc, char** argv);
void write_pid_for_kill();
void log_env();
void initialize_wlr_logging();

void apply_backend_env_defaults();
bool create_display_and_backend(WlrServer& server);
bool create_renderer_and_allocator(WlrServer& server);
bool init_protocols_and_seat(WlrServer& server);
void register_global_listeners(WlrServer& server);
bool start_backend_and_socket(WlrServer& server);
void install_sigint_handler();
void teardown_server(WlrServer& server);
