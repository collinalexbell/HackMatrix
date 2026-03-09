#include "engine.h"

extern "C" {
    #include <wlr/types/wlr_keyboard.h>
    #include <wlr/types/wlr_seat.h>
}

#include "wayland/keyboard.h"
#include "wayland/wlr_compositor.h"



void
handle_keyboard_destroy(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), destroy);
  wl_list_remove(&handle->modifiers.link);
  wl_list_remove(&handle->key.link);
  wl_list_remove(&handle->destroy.link);
  delete handle;
}

void
process_key_sym(WlrServer* server,
                wlr_keyboard* keyboard,
                xkb_keysym_t sym,
                bool pressed,
                uint32_t time_msec,
                uint32_t keycode = 0,
                bool update_mods = false)
{

  // This is an assertion and generally shouldn't happen
  // in a properly initialized HackMatrix
  if (!server || !server->engine) {
    return;
  }

  auto wm = server->engine->getWindowManager();
  auto controls = server->engine->getControls();

  uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

  bool modifierHeld = (mods & server->hotkeyModifierMask);
  bool shiftHeld = (mods & WLR_MODIFIER_SHIFT);

  // This is where all WM controls are handled (except for QUIT, see below)
  auto resp = controls->handleKeySym(sym, pressed, modifierHeld, shiftHeld);


  // QUIT can not be handled inside the controls itself
  // It must be handled in the serve that owns the display
  if (resp.requestQuit) {
    wl_display_terminate(server->display);
    return;
  }


  // In the event that WM controls handled a symkey,
  // it should not be passed to the app,
  // so return before the seat is notified.
  if (resp.consumed) {
    return;
  }

  // Send the event created by wlr_keybard to the wlr_seat
  // wlr_keyboard is a physical device listened to by wlr
  // In contrast, wlr_seat is a user input context that can merge
  // multiple physical devices or not recieve input from any devices.
  // Apps are notified by seats (reigstered happens in WaylandApp.takeInputFocus)
  // Seats are notified by wlr_keyboard (here, below)
  enum wl_keyboard_key_state state =
    pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
  wlr_seat_keyboard_notify_key(server->seat, time_msec, keycode, state);
  wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
}




// This is the handler callback for a physical wlr_keyboard event.
// This function converts the event into a keysym and then sends it to another function
// for processing and potential action.
void
handle_keyboard_key(wl_listener* listener, void* data)
{
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), key);
  auto* server = handle->server;
  auto* event = static_cast<wlr_keyboard_key_event*>(data);
  // wlroots key events carry a hardware/libinput keycode. Add 8 only for xkb
  // lookup; keep the raw code for notifying the seat.
  uint32_t xkb_keycode = event->keycode + 8;
  const xkb_keysym_t* syms;
  int nsyms =
    xkb_state_key_get_syms(handle->keyboard->xkb_state, xkb_keycode, &syms);
  if (nsyms > 0) {
    bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
    process_key_sym(
      server, handle->keyboard, syms[0], pressed, event->time_msec, event->keycode);
  }
}

void
handle_new_keyboard(WlrServer* server, wlr_input_device* device)
{
  auto* keyboard = wlr_keyboard_from_input_device(device);
  server->last_keyboard_device = device;
  if (server->seat) {
    wlr_seat_set_capabilities(
      server->seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    wlr_seat_set_keyboard(server->seat, keyboard);
  }
  auto* handle = new WlrKeyboardHandle();
  handle->server = server;
  handle->keyboard = keyboard;
  handle->key.notify = handle_keyboard_key;
  wl_signal_add(&keyboard->events.key, &handle->key);
  handle->modifiers.notify = [](wl_listener* listener, void* data) {
    (void)data;
    auto* handle = wl_container_of(
      listener, static_cast<WlrKeyboardHandle*>(nullptr), modifiers);
    if (handle->server && handle->server->seat) {
      wlr_seat_keyboard_notify_modifiers(handle->server->seat,
                                         &handle->keyboard->modifiers);
    }
  };
  wl_signal_add(&keyboard->events.modifiers, &handle->modifiers);
  handle->destroy.notify = handle_keyboard_destroy;
  wl_signal_add(&device->events.destroy, &handle->destroy);

  xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  xkb_keymap* keymap =
    xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
  wlr_keyboard_set_keymap(keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(keyboard, 25, 600);
  if (server->seat) {
    wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
    if (server->seat->keyboard_state.focused_surface) {
      wlr_seat_keyboard_notify_enter(
        server->seat,
        server->seat->keyboard_state.focused_surface,
        keyboard->keycodes,
        keyboard->num_keycodes,
        &keyboard->modifiers);
    }
  }
}