#include "wayland_app.h"

extern "C" {
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/swapchain.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
}
#include <drm_fourcc.h>
#include <glm/gtc/matrix_transform.hpp>
#include <EGL/egl.h>
#include <glad/glad.h>
#include <chrono>
#include <vector>

#ifdef WLROOTS_DEBUG_LOGS
constexpr bool kWlrootsDebugLogs = true;
#else
constexpr bool kWlrootsDebugLogs = false;
#endif

static glm::mat4
recomputeHeightScaler(double width, double height)
{
  if (height == 0.0) {
    return glm::mat4(1.0f);
  }
  float scale = static_cast<float>(width / height);
  glm::mat4 scaler(1.0f);
  scaler = glm::scale(scaler, glm::vec3(1.0f, scale, 1.0f));
  return scaler;
}

WaylandApp::WaylandApp(wlr_renderer* renderer,
                       wlr_allocator* allocator,
                       wlr_xdg_surface* xdg,
                       size_t index)
  : renderer(renderer)
  , allocator(allocator)
  , appIndex(index)
{
  this->xdg_surface = xdg;
  this->xdg_toplevel = xdg_surface->toplevel;
  this->surface = xdg_surface->surface;
  this->title = xdg_toplevel && xdg_toplevel->title ? xdg_toplevel->title : "wayland-app";

  surface_commit.notify = [](wl_listener* listener, void* data) {
    auto* self = wl_container_of(listener, static_cast<WaylandApp*>(nullptr), surface_commit);
    self->handle_commit(static_cast<wlr_surface*>(data));
  };
  wl_signal_add(&surface->events.commit, &surface_commit);

  surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
    auto* self = wl_container_of(listener, static_cast<WaylandApp*>(nullptr), surface_destroy);
    self->handle_destroy();
  };
  wl_signal_add(&surface->events.destroy, &surface_destroy);
}

WaylandApp::~WaylandApp()
{
  if (surface) {
    wl_list_remove(&surface_commit.link);
    wl_list_remove(&surface_destroy.link);
    surface = nullptr;
  }
  if (pending_buffer.has_value()) {
    wlr_buffer_unlock(pending_buffer.value());
  }
}

void
WaylandApp::handle_destroy()
{
  if (pending_buffer.has_value()) {
    wlr_buffer_unlock(pending_buffer.value());
    pending_buffer.reset();
  }
  surface = nullptr;
  xdg_surface = nullptr;
  xdg_toplevel = nullptr;

  // Remove listeners to avoid dangling commit handlers.
  wl_list_remove(&surface_commit.link);
  wl_list_remove(&surface_destroy.link);
}

void
WaylandApp::takeInputFocus()
{
  focused = true;
  // Activate the toplevel only when the WM explicitly focuses us.
  if (xdg_toplevel) {
    wlr_xdg_toplevel_set_activated(xdg_toplevel, true);
    if (xdg_surface) {
      wlr_xdg_surface_schedule_configure(xdg_surface);
    }
  }
  if (seat && seat_surface) {
    wlr_keyboard* kbd = wlr_seat_get_keyboard(seat);
    if (kbd) {
      wlr_seat_set_keyboard(seat, kbd);
      wlr_seat_keyboard_notify_enter(
        seat, seat_surface, kbd->keycodes, kbd->num_keycodes, &kbd->modifiers);
      wlr_seat_keyboard_notify_modifiers(seat, &kbd->modifiers);
    }
    // Ensure pointer focus follows the focused app so pointer events are delivered.
    wlr_seat_pointer_notify_enter(seat, seat_surface, 0, 0);
    auto now_ms = static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count());
    wlr_seat_pointer_notify_motion(seat, now_ms, 0, 0);
  }
}

void
WaylandApp::handle_commit(wlr_surface* surface)
{
  if (!surface->buffer) {
    wlr_log(WLR_DEBUG, "wayland-app: commit without buffer");
    return;
  }

  if (pending_buffer.has_value()) {
    wlr_buffer_unlock(pending_buffer.value());
  }

  wlr_buffer* buf = &surface->buffer->base;
  wlr_log(WLR_DEBUG,
          "wayland-app: commit buffer=%p size=%dx%d",
          (void*)buf,
          surface->current.width,
          surface->current.height);
  wlr_buffer_lock(buf);
  pending_buffer = buf;

  width = surface->current.width;
  height = surface->current.height;
  update_height_scalar();
  mapped = true;
}

void
WaylandApp::update_height_scalar()
{
  heightScalar = recomputeHeightScaler(width, height);
}

void
WaylandApp::appTexture()
{
  if (!pending_buffer.has_value() || textureId < 0 || textureUnit < 0) {
    return;
  }

  wlr_buffer* buffer = pending_buffer.value();
  if (!buffer) {
    return;
  }

    size_t stride = 0;
    void* data = nullptr;
    uint32_t format = 0;

  auto logSamplesEnabled = []() {
    static int enabled = -1;
    if (enabled == -1) {
      enabled = (kWlrootsDebugLogs || std::getenv("WLR_APP_SAMPLE_LOG")) ? 1 : 0;
    }
    return enabled == 1;
  };

  if (wlr_buffer_begin_data_ptr_access(buffer,
                                       WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                       &data,
                                       &format,
                                       &stride)) {
    if (logSamplesEnabled()) {
      static FILE* logFile = []() {
        FILE* f = std::fopen("/tmp/matrix-wlroots-waylandapp.log", "a");
        return f ? f : stderr;
      }();
      static int lastW = -1;
      static int lastH = -1;
      if (width != lastW || height != lastH) {
        std::fprintf(logFile,
                     "wayland-app: uploading %dx%d stride=%zu fmt=%u texId=%d unit=%d\n",
                     width,
                     height,
                     stride,
                     format,
                     textureId,
                     textureUnit - GL_TEXTURE0);
        std::fflush(logFile);
        lastW = width;
        lastH = height;
      }
      if (!sampleLogged) {
        const uint8_t* raw = static_cast<const uint8_t*>(data);
        uint8_t r0 = raw[0], g0 = raw[1], b0 = raw[2], a0 = raw[3];
        bool ok = (width > 0 && height > 0) && (r0 | g0 | b0 | a0);
        std::fprintf(logFile,
                     "wayland-app: sample size=%dx%d fmt=%u firstPixel=(%u,%u,%u,%u) ok=%d\n",
                     width,
                     height,
                     format,
                     r0,
                     g0,
                     b0,
                     a0,
                     ok ? 1 : 0);
        std::fflush(logFile);
        sampleLogged = true;
      }
    }
    // Convert to RGBA8 if the format isn't directly supported in GLES2.
    const uint8_t* src = static_cast<const uint8_t*>(data);
    std::vector<uint8_t> converted;
    const size_t srcStride = stride;
    const size_t dstStride = static_cast<size_t>(width) * 4;
    GLenum uploadFormat = GL_RGBA;
    uint32_t firstPixel = 0;
    uint32_t rawFirstBytes = 0;
    uint32_t centerPixel = 0;
    uint32_t rawCenterBytes = 0;
    uint32_t rowChecksum = 0;
    // If the buffer appears to be empty, populate a test pattern to verify
    // the rendering path. This will be used only for diagnostics.
    bool injectTestPattern = false;
    uint8_t minR = 255, minG = 255, minB = 255, minA = 255;
    uint8_t maxR = 0, maxG = 0, maxB = 0, maxA = 0;

    auto convert_row = [&](int y, uint8_t* dstRow) {
      const uint8_t* srcRow = src + y * srcStride;
      for (int x = 0; x < width; ++x) {
        const uint8_t* p = srcRow + x * 4;
        uint8_t r = 0, g = 0, b = 0, a = 0;
        switch (format) {
          case DRM_FORMAT_ARGB8888: // memory order (little-endian): B G R A
            b = p[0];
            g = p[1];
            r = p[2];
            a = p[3];
            break;
          case DRM_FORMAT_ABGR8888: // A B G R
            r = p[0];
            g = p[1];
            b = p[2];
            a = p[3];
            break;
          case DRM_FORMAT_BGRA8888: // memory order: A R G B
            a = p[0];
            r = p[1];
            g = p[2];
            b = p[3];
            break;
          case DRM_FORMAT_RGBA8888: // memory order: A B G R
            a = p[0];
            b = p[1];
            g = p[2];
            r = p[3];
            break;
          case DRM_FORMAT_XBGR8888: // memory order: R G B X
            r = p[0];
            g = p[1];
            b = p[2];
            a = 255;
            break;
          case DRM_FORMAT_BGRX8888: // memory order: X R G B
            r = p[1];
            g = p[2];
            b = p[3];
            a = 255;
            break;
          case DRM_FORMAT_RGBX8888: // memory order: X B G R
            b = p[1];
            g = p[2];
            r = p[3];
            a = 255;
            break;
          case DRM_FORMAT_XRGB8888: // Fourcc XRGB8888, bytes in memory B,G,R,X (little endian)
            b = p[0];
            g = p[1];
            r = p[2];
            a = 255;
            break;
          case DRM_FORMAT_XRGB2101010: { // 10:10:10:2 little endian, B,G,R,X
            uint32_t v = *reinterpret_cast<const uint32_t*>(p);
            uint32_t br = (v >> 0) & 0x3FF;
            uint32_t bg = (v >> 10) & 0x3FF;
            uint32_t bb = (v >> 20) & 0x3FF;
            r = (uint8_t)((br * 255) / 1023);
            g = (uint8_t)((bg * 255) / 1023);
            b = (uint8_t)((bb * 255) / 1023);
            a = 255;
            break;
          }
          default:
            // Assume RGBA
            r = p[0];
            g = p[1];
            b = p[2];
            a = p[3];
            break;
        }
        dstRow[x * 4 + 0] = r;
        dstRow[x * 4 + 1] = g;
        dstRow[x * 4 + 2] = b;
        dstRow[x * 4 + 3] = a;
        if (y == 0 && x == 0) {
          firstPixel = (uint32_t(r) << 24) | (uint32_t(g) << 16) |
                       (uint32_t(b) << 8) | uint32_t(a);
        }
        minR = std::min(minR, r);
        minG = std::min(minG, g);
        minB = std::min(minB, b);
        minA = std::min(minA, a);
        maxR = std::max(maxR, r);
        maxG = std::max(maxG, g);
        maxB = std::max(maxB, b);
        maxA = std::max(maxA, a);
      }
    };

    converted.resize(static_cast<size_t>(height) * dstStride);
    for (int y = 0; y < height; ++y) {
      const uint8_t* row = src + y * srcStride;
      if (y == 0 && width > 0) {
        // Log the raw first pixel bytes before conversion for debugging.
        rawFirstBytes = (uint32_t(row[0]) << 24) | (uint32_t(row[1]) << 16) |
                        (uint32_t(row[2]) << 8) | uint32_t(row[3]);
        // Simple checksum of first row
        for (int x = 0; x < width; ++x) {
          rowChecksum += row[x * 4 + 0] + row[x * 4 + 1] +
                         row[x * 4 + 2] + row[x * 4 + 3];
        }
      }
      if (y == height / 2 && width > 0) {
        const uint8_t* p = row + (width / 2) * 4;
        rawCenterBytes = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
                         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
      }
      uint8_t* dstRow = converted.data() + y * dstStride;
      convert_row(y, dstRow);
      if (injectTestPattern) {
        for (int x = 0; x < width; ++x) {
          dstRow[x * 4 + 0] = static_cast<uint8_t>((x * 255) / std::max(1, width - 1));
          dstRow[x * 4 + 1] = static_cast<uint8_t>((y * 255) / std::max(1, height - 1));
          dstRow[x * 4 + 2] = 128;
          dstRow[x * 4 + 3] = 255;
        }
      }
      if (y == height / 2 && width > 0) {
        const uint8_t* dst = converted.data() + y * dstStride + (width / 2) * 4;
        centerPixel = (uint32_t(dst[0]) << 24) | (uint32_t(dst[1]) << 16) |
                      (uint32_t(dst[2]) << 8) | uint32_t(dst[3]);
      }
    }

    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureId);
    GLboolean valid = glIsTexture(textureId);
    if (!valid) {
      wlr_log(WLR_ERROR, "wayland-app: texture %d is not valid", textureId);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 converted.data());
    uploadedWidth = width;
    uploadedHeight = height;
    GLenum errAfter = glGetError();
    if (rowChecksum == 0 && centerPixel == 0 && firstPixel == 0) {
      injectTestPattern = true;
    }
    if (injectTestPattern) {
      glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      0,
                      0,
                      width,
                      height,
                      uploadFormat,
                      GL_UNSIGNED_BYTE,
                      converted.data());
      errAfter = glGetError();
    }
    if constexpr (kWlrootsDebugLogs) {
      FILE* logFile2 = std::fopen("/tmp/matrix-wlroots-waylandapp.log", "a");
      if (logFile2) {
        std::fprintf(logFile2,
                     "WaylandApp upload: texId=%d unit=%d texSize=%dx%d uploadSize=%dx%d stride=%zu fmt=%u glErr=0x%x firstPixel=0x%08x rawFirst=0x%08x center=0x%08x rawCenter=0x%08x rowChecksum=%u min=(%u,%u,%u,%u) max=(%u,%u,%u,%u)\n",
                     textureId,
                     textureUnit - GL_TEXTURE0,
                     width,
                     height,
                     width,
                     height,
                     stride,
                     format,
                     errAfter,
                     firstPixel,
                     rawFirstBytes,
                     centerPixel,
                     rawCenterBytes,
                     rowChecksum,
                     minR,
                     minG,
                     minB,
                     minA,
                     maxR,
                     maxG,
                     maxB,
                     maxA);
        std::fflush(logFile2);
        std::fclose(logFile2);
      }
    }
    wlr_buffer_end_data_ptr_access(buffer);
  }
}
