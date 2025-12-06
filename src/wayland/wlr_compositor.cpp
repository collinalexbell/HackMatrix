#define WLR_USE_UNSTABLE 1

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <drm_fourcc.h>

extern "C" {
#include <EGL/egl.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
}

#include <glad/glad.h>

#include "engine.h"

namespace {

double
currentTimeSeconds()
{
  static const auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - start;
  return elapsed.count();
}

struct WlrServer;

struct WlrOutputHandle {
  WlrServer* server = nullptr;
  wlr_output* output = nullptr;
  wlr_swapchain* swapchain = nullptr;
  wl_listener frame;
  wl_listener destroy;
  int buffer_age = -1;
  GLuint depth_rbo = 0;
  int depth_width = 0;
  int depth_height = 0;
};

struct WlrKeyboardHandle {
  WlrServer* server = nullptr;
  wlr_keyboard* keyboard = nullptr;
  wl_listener key;
  wl_listener destroy;
};

struct WlrPointerHandle {
  WlrServer* server = nullptr;
  wlr_pointer* pointer = nullptr;
  wl_listener motion;
  wl_listener motion_abs;
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
};

struct WlrServer {
  wl_display* display = nullptr;
  wlr_backend* backend = nullptr;
  wlr_renderer* renderer = nullptr;
  wlr_allocator* allocator = nullptr;
  wl_listener new_output;
  wl_listener new_input;
  std::unique_ptr<Engine> engine;
  bool gladLoaded = false;
  double lastFrameTime = 0.0;
  char** envp = nullptr;
  InputState input;
};

WlrServer* g_server = nullptr;

void
handle_keyboard_destroy(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), destroy);
  wl_list_remove(&handle->key.link);
  wl_list_remove(&handle->destroy.link);
  delete handle;
}

void
handle_keyboard_key(wl_listener* listener, void* data)
{
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), key);
  auto* server = handle->server;
  auto* event = static_cast<wlr_keyboard_key_event*>(data);
  uint32_t keycode = event->keycode + 8;
  const xkb_keysym_t* syms;
  int nsyms = xkb_state_key_get_syms(handle->keyboard->xkb_state, keycode, &syms);
  for (int i = 0; i < nsyms; ++i) {
    if (syms[i] == XKB_KEY_Escape && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      wl_display_terminate(server->display);
    }
    bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
    switch (syms[i]) {
      case XKB_KEY_w:
      case XKB_KEY_W:
        server->input.forward = pressed;
        break;
      case XKB_KEY_s:
      case XKB_KEY_S:
        server->input.back = pressed;
        break;
      case XKB_KEY_a:
      case XKB_KEY_A:
        server->input.left = pressed;
        break;
      case XKB_KEY_d:
      case XKB_KEY_D:
        server->input.right = pressed;
        break;
      default:
        break;
    }
    wlr_log(WLR_DEBUG, "key sym=%u pressed=%d", syms[i], pressed ? 1 : 0);
  }
}

void
handle_new_input(wl_listener* listener, void* data)
{
  auto* server =
    wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_input);
  auto* device = static_cast<wlr_input_device*>(data);
  if (device->type != WLR_INPUT_DEVICE_KEYBOARD) {
    if (device->type == WLR_INPUT_DEVICE_POINTER) {
      auto* pointer = wlr_pointer_from_input_device(device);
      auto* handle = new WlrPointerHandle();
      handle->server = server;
      handle->pointer = pointer;
      handle->motion.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), motion);
        auto* event = static_cast<wlr_pointer_motion_event*>(data);
        handle->server->input.delta_x += event->delta_x;
        handle->server->input.delta_y += event->delta_y;
        wlr_log(WLR_DEBUG, "pointer motion rel dx=%.3f dy=%.3f",
                event->delta_x,
                event->delta_y);
      };
      wl_signal_add(&pointer->events.motion, &handle->motion);
      handle->motion_abs.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), motion_abs);
        auto* event = static_cast<wlr_pointer_motion_absolute_event*>(data);
        if (handle->server->input.have_abs) {
          // Normalize deltas into something closer to pixel units.
          double dx = (event->x - handle->server->input.last_abs_x) * 1000.0;
          double dy = (event->y - handle->server->input.last_abs_y) * 1000.0;
          handle->server->input.delta_x += dx;
          handle->server->input.delta_y += dy;
        }
        handle->server->input.have_abs = true;
        handle->server->input.last_abs_x = event->x;
        handle->server->input.last_abs_y = event->y;
        wlr_log(WLR_DEBUG, "pointer motion abs dx=%.3f dy=%.3f", handle->server->input.delta_x, handle->server->input.delta_y);
      };
      wl_signal_add(&pointer->events.motion_absolute, &handle->motion_abs);
      handle->destroy.notify = [](wl_listener* listener, void* data) {
        (void)data;
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), destroy);
        wl_list_remove(&handle->motion.link);
        wl_list_remove(&handle->motion_abs.link);
        wl_list_remove(&handle->destroy.link);
        delete handle;
      };
      wl_signal_add(&device->events.destroy, &handle->destroy);
    }
    return;
  }

  auto* keyboard = wlr_keyboard_from_input_device(device);
  auto* handle = new WlrKeyboardHandle();
  handle->server = server;
  handle->keyboard = keyboard;
  handle->key.notify = handle_keyboard_key;
  wl_signal_add(&keyboard->events.key, &handle->key);
  handle->destroy.notify = handle_keyboard_destroy;
  wl_signal_add(&device->events.destroy, &handle->destroy);

  xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  xkb_keymap* keymap =
    xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
  wlr_keyboard_set_keymap(keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(keyboard, 25, 600);
}

void
handle_output_destroy(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrOutputHandle*>(nullptr), destroy);
  wl_list_remove(&handle->frame.link);
  wl_list_remove(&handle->destroy.link);
  if (handle->swapchain) {
    wlr_swapchain_destroy(handle->swapchain);
  }
  if (handle->depth_rbo) {
    glDeleteRenderbuffers(1, &handle->depth_rbo);
    handle->depth_rbo = 0;
  }
  delete handle;
}

void
ensure_glad(WlrServer* server)
{
  if (server->gladLoaded) {
    return;
  }
  if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
    wlr_log(WLR_ERROR, "Failed to load GL functions via EGL");
    wl_display_terminate(server->display);
    return;
  }
  const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* glslVersion =
    reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
  std::fprintf(stderr, "wlroots compositor GL version: %s, GLSL: %s\n",
               glVersion ? glVersion : "(null)",
               glslVersion ? glslVersion : "(null)");
  server->gladLoaded = true;
}

void
ensure_depth_buffer(WlrOutputHandle* handle, int width, int height, GLuint fbo)
{
  if (handle->depth_rbo != 0 &&
      handle->depth_width == width &&
      handle->depth_height == height) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER,
                              handle->depth_rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER,
                              handle->depth_rbo);
    return;
  }

  if (handle->depth_rbo != 0) {
    glDeleteRenderbuffers(1, &handle->depth_rbo);
  }
  glGenRenderbuffers(1, &handle->depth_rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, handle->depth_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                            GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER,
                            handle->depth_rbo);
  handle->depth_width = width;
  handle->depth_height = height;
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::fprintf(stderr,
                 "FBO incomplete after attaching depth: 0x%x (size %dx%d)\n",
                 status,
                 width,
                 height);
  }
}

void
handle_output_frame(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrOutputHandle*>(nullptr), frame);
  auto* server = handle->server;

  struct wlr_output_state output_state;
  wlr_output_state_init(&output_state);

  if (!wlr_output_configure_primary_swapchain(
        handle->output, &output_state, &handle->swapchain)) {
    wlr_log(WLR_ERROR, "Failed to configure primary swapchain");
    wlr_output_state_finish(&output_state);
    return;
  }

  wlr_buffer* buffer = wlr_swapchain_acquire(handle->swapchain);
  if (!buffer) {
    wlr_log(WLR_ERROR, "Failed to acquire buffer from swapchain");
    wlr_output_state_finish(&output_state);
    return;
  }

  const wlr_buffer_pass_options pass_options = {};
  wlr_render_pass* pass =
    wlr_renderer_begin_buffer_pass(server->renderer, buffer, &pass_options);
  if (!pass) {
    wlr_log(WLR_ERROR, "Failed to begin buffer pass");
    wlr_buffer_unlock(buffer);
    wlr_output_state_finish(&output_state);
    return;
  }

  int width = handle->swapchain ? handle->swapchain->width : 1;
  int height = handle->swapchain ? handle->swapchain->height : 1;

  ensure_glad(server);

  GLuint fbo = wlr_gles2_renderer_get_buffer_fbo(server->renderer, buffer);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  ensure_depth_buffer(handle, width, height, fbo);
  if (!server->engine) {
    EngineOptions options;
    options.enableControls = false; // wlroots path feeds input directly
    options.enableGui = false;
    options.invertYAxis = true;
    server->engine = std::make_unique<Engine>(nullptr, server->envp, options);
  }

  if (server->engine) {
    Camera* camera = server->engine->getCamera();
    if (camera) {
      camera->handleTranslateForce(
        server->input.forward, server->input.back, server->input.left, server->input.right);
      if (server->input.delta_x != 0.0 || server->input.delta_y != 0.0) {
        camera->handleRotateForce(
          nullptr, server->input.delta_x, -server->input.delta_y);
        server->input.delta_x = 0.0;
        server->input.delta_y = 0.0;
      }
    }
  }

  glViewport(0, 0, width, height);
  double frameStart = currentTimeSeconds();
  server->engine->frame(frameStart);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  wlr_render_pass_submit(pass);

  wlr_output_state_set_buffer(&output_state, buffer);
  if (!wlr_output_commit_state(handle->output, &output_state)) {
    wlr_log(WLR_ERROR, "Failed to commit output frame");
  }
  wlr_output_state_finish(&output_state);
  wlr_buffer_unlock(buffer);
  server->lastFrameTime = frameStart;
}

void
handle_new_output(wl_listener* listener, void* data)
{
  auto* server =
    wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_output);
  auto* output = static_cast<wlr_output*>(data);

  if (!wlr_output_init_render(output, server->allocator, server->renderer)) {
    wlr_log(WLR_ERROR, "Failed to init render for output %s", output->name);
    return;
  }

  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);
  wlr_output_mode* mode = wlr_output_preferred_mode(output);
  if (mode != nullptr) {
    wlr_output_state_set_mode(&state, mode);
  }
  if (!wlr_output_commit_state(output, &state)) {
    wlr_log(WLR_ERROR, "Failed to commit output %s", output->name);
    wlr_output_state_finish(&state);
    return;
  }
  wlr_output_state_finish(&state);

  auto* handle = new WlrOutputHandle();
  handle->server = server;
  handle->output = output;
  handle->frame.notify = handle_output_frame;
  wl_signal_add(&output->events.frame, &handle->frame);
  handle->destroy.notify = handle_output_destroy;
  wl_signal_add(&output->events.destroy, &handle->destroy);

  wlr_output_schedule_frame(output);
}

} // namespace

int
main(int argc, char** argv, char** envp)
{
  (void)argc;
  (void)argv;
  WlrServer server = {};
  server.envp = envp;

  wlr_log_init(WLR_DEBUG, nullptr);

  // If wlroots was built without X11 backend and we're running under Wayland,
  // force the Wayland backend to avoid the X11 autoselection.
  if (!std::getenv("WLR_BACKENDS") && std::getenv("WAYLAND_DISPLAY")) {
    setenv("WLR_BACKENDS", "wayland", 1);
  }

  server.display = wl_display_create();
  if (!server.display) {
    std::fprintf(stderr, "Failed to create Wayland display\n");
    return EXIT_FAILURE;
  }
  g_server = &server;

  server.backend =
    wlr_backend_autocreate(wl_display_get_event_loop(server.display), nullptr);
  if (!server.backend) {
    std::fprintf(stderr, "Failed to create wlroots backend\n");
    return EXIT_FAILURE;
  }

  server.renderer = wlr_renderer_autocreate(server.backend);
  if (!server.renderer) {
    std::fprintf(stderr, "Failed to create renderer\n");
    return EXIT_FAILURE;
  }

  server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
  if (!server.allocator) {
    std::fprintf(stderr, "Failed to create allocator\n");
    return EXIT_FAILURE;
  }

  server.new_output.notify = handle_new_output;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);
  server.new_input.notify = handle_new_input;
  wl_signal_add(&server.backend->events.new_input, &server.new_input);

  if (!wlr_backend_start(server.backend)) {
    std::fprintf(stderr, "Failed to start wlroots backend\n");
    return EXIT_FAILURE;
  }

  const char* socket = wl_display_add_socket_auto(server.display);
  if (!socket) {
    std::fprintf(stderr, "Failed to create Wayland socket\n");
    return EXIT_FAILURE;
  }
  setenv("WAYLAND_DISPLAY", socket, true);

  std::signal(SIGINT, [](int) {
    if (g_server && g_server->display) {
      wl_display_terminate(g_server->display);
    }
  });

  wl_display_run(server.display);

  if (server.engine) {
    server.engine.reset();
  }
  if (server.allocator) {
    wlr_allocator_destroy(server.allocator);
  }
  if (server.renderer) {
    wlr_renderer_destroy(server.renderer);
  }
  if (server.backend) {
    wlr_backend_destroy(server.backend);
  }
  if (server.display) {
    wl_display_destroy(server.display);
  }

  return EXIT_SUCCESS;
}
