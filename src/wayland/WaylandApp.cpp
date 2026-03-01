#include "wayland_app.h"

extern "C" {
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/dmabuf.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
}
#include <signal.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <glm/gtc/matrix_transform.hpp>
#include "screen.h"
#include "components/Bootable.h"
#include <cstdlib>
#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
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
  // Match the X11 path: normalize the surface aspect ratio against the default
  // screen aspect so in-world quads stay a 1:1 pixel mapping.
  float standardRatio = SCREEN_HEIGHT / SCREEN_WIDTH;
  float currentRatio = static_cast<float>(height / width);
  float scaleFactor = currentRatio / standardRatio;
  return glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, scaleFactor, 1.0f));
}

// Minimal declaration to import dmabuf into a GL texture without dragging in
// GLES extension headers that conflict with glad-provided symbols.
using GLeglImageOES = void*;
using PFNGLEGLIMAGETARGETTEXTURE2DOESPROC =
  void (*)(GLenum target, GLeglImageOES image);
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gEGLImageTargetTexture2DOES = nullptr;
static PFNEGLCREATEIMAGEKHRPROC gEglCreateImageKHR = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC gEglDestroyImageKHR = nullptr;

static void
ensureEglImageFns()
{
  if (!gEglCreateImageKHR) {
    gEglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
      eglGetProcAddress("eglCreateImageKHR"));
  }
  if (!gEglDestroyImageKHR) {
    gEglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
      eglGetProcAddress("eglDestroyImageKHR"));
  }
}

WaylandApp::WaylandApp(wlr_renderer* renderer,
                       wlr_allocator* allocator,
                       wlr_xdg_surface* xdg,
                       size_t index,
                       bool request_initial_size)
  : renderer(renderer)
  , allocator(allocator)
  , appIndex(index)
{
  this->xdg_surface = xdg;
  this->xdg_toplevel = xdg_surface->toplevel;
  this->surface = xdg_surface->surface;
  this->title = xdg_toplevel && xdg_toplevel->title ? xdg_toplevel->title : "wayland-app";
  if (surface && surface->resource && surface->resource->client) {
    wl_client_get_credentials(surface->resource->client, &clientPid, nullptr, nullptr);
  }

  // Default to the same normalized size we ask X11 windows for (85% of screen)
  // so the initial quad mapping matches screen pixels even before the first
  // buffer commit provides real dimensions.
  width = Bootable::DEFAULT_WIDTH;
  height = Bootable::DEFAULT_HEIGHT;
  update_height_scalar();
  if (request_initial_size) {
    requestSize(width, height);
  }

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

WaylandApp::WaylandApp(wlr_renderer* renderer,
                       wlr_allocator* allocator,
                       wlr_surface* surf,
                       const std::string& titleHint,
                       size_t index)
  : renderer(renderer)
  , allocator(allocator)
  , appIndex(index)
{
  this->surface = surf;
  this->xdg_surface = nullptr;
  this->xdg_toplevel = nullptr;
  this->title = titleHint;
  if (surface && surface->resource && surface->resource->client) {
    wl_client_get_credentials(surface->resource->client, &clientPid, nullptr, nullptr);
  }

  width = surface ? surface->current.width : Bootable::DEFAULT_WIDTH;
  height = surface ? surface->current.height : Bootable::DEFAULT_HEIGHT;
  if (width <= 0 || height <= 0) {
    width = Bootable::DEFAULT_WIDTH;
    height = Bootable::DEFAULT_HEIGHT;
  }
  update_height_scalar();

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
  ensureEglImageFns();
  if (importedImage != EGL_NO_IMAGE_KHR && gEglDestroyImageKHR) {
    gEglDestroyImageKHR(eglGetCurrentDisplay(), importedImage);
    importedImage = EGL_NO_IMAGE_KHR;
  }
  if (importedBufferLocked && importedBuffer) {
    wlr_buffer_unlock(importedBuffer);
  }
  importedBuffer = nullptr;
  importedBufferLocked = false;
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
  // If the seat is still focused on this surface, clear it to avoid dangling
  // pointer focus after the client is gone (can crash on next enter).
  if (seat && seat_surface) {
    if (seat->pointer_state.focused_surface == seat_surface) {
      wlr_seat_pointer_notify_clear_focus(seat);
    }
    if (seat->keyboard_state.focused_surface == seat_surface) {
      wlr_seat_keyboard_notify_clear_focus(seat);
    }
  }
  seat_surface = nullptr;
  seat = nullptr;
  ensureEglImageFns();
  if (importedImage != EGL_NO_IMAGE_KHR && gEglDestroyImageKHR) {
    gEglDestroyImageKHR(eglGetCurrentDisplay(), importedImage);
    importedImage = EGL_NO_IMAGE_KHR;
  }
  if (importedBufferLocked && importedBuffer) {
    wlr_buffer_unlock(importedBuffer);
  }
  importedBufferLocked = false;
  importedBuffer = nullptr;
  surface = nullptr;
  xdg_surface = nullptr;
  xdg_toplevel = nullptr;

  // Remove listeners to avoid dangling commit handlers.
  wl_list_remove(&surface_commit.link);
  wl_list_remove(&surface_destroy.link);
}

void
WaylandApp::close()
{
  if (xdg_toplevel) {
    wlr_xdg_toplevel_send_close(xdg_toplevel);
  }
  if (clientPid > 0) {
    kill(clientPid, SIGTERM);
    // In case the client ignores SIGTERM, follow up with SIGKILL.
    kill(clientPid, SIGKILL);
  }
}

void
WaylandApp::requestSize(int width, int height)
{
  if (xdg_toplevel && width > 0 && height > 0) {
    wlr_xdg_toplevel_set_size(xdg_toplevel, width, height);
    // Trigger a configure so the client applies the size.
    if (xdg_surface) {
      wlr_xdg_surface_schedule_configure(xdg_surface);
    }
  }
}

void
WaylandApp::takeInputFocus()
{
  focused = true;
  if (!seat_surface) {
    return;
  }
  // Activate the toplevel only when the WM explicitly focuses us.
  if (xdg_toplevel) {
    wlr_xdg_toplevel_set_activated(xdg_toplevel, true);
    if (xdg_surface) {
      wlr_xdg_surface_schedule_configure(xdg_surface);
    }
  }
  if (seat && seat_surface) {
    // Reset any previous pointer focus so the seat cleanly re-targets this app.
    wlr_seat_pointer_notify_clear_focus(seat);
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
    wlr_seat_pointer_notify_frame(seat);
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
  sampleLogged = false;
  needsImport = true;

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
  ensureEglImageFns();

  auto destroyImportedImage = [&]() {
    ensureEglImageFns();
    if (importedImage != EGL_NO_IMAGE_KHR && gEglDestroyImageKHR) {
      gEglDestroyImageKHR(eglGetCurrentDisplay(), importedImage);
      importedImage = EGL_NO_IMAGE_KHR;
    }
    if (importedBufferLocked && importedBuffer) {
      wlr_buffer_unlock(importedBuffer);
    }
    importedBufferLocked = false;
    importedBuffer = nullptr;
  };

  // Skip work if we've already imported this buffer and nothing new was
  // committed.
  if (!needsImport && buffer == importedBuffer) {
    return;
  }

  // Try zero-copy: import the client's dmabuf into a persistent EGLImage and
  // bind it to the app's texture. Once imported, the client can update the
  // buffer contents without us re-uploading.
  wlr_dmabuf_attributes dmabuf{};
  if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
    if (!gEGLImageTargetTexture2DOES) {
      gEGLImageTargetTexture2DOES =
        reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
          eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    }
    if (gEGLImageTargetTexture2DOES == nullptr) {
      // Can't import without the extension.
    } else {
    EGLDisplay eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay != EGL_NO_DISPLAY) {
      if (buffer != importedBuffer) {
        destroyImportedImage();
      }

      static const EGLint planeFdAttrs[WLR_DMABUF_MAX_PLANES] = {
        EGL_DMA_BUF_PLANE0_FD_EXT,
        EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT,
        EGL_DMA_BUF_PLANE3_FD_EXT,
      };
      static const EGLint planeOffsetAttrs[WLR_DMABUF_MAX_PLANES] = {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT,
        EGL_DMA_BUF_PLANE3_OFFSET_EXT,
      };
      static const EGLint planePitchAttrs[WLR_DMABUF_MAX_PLANES] = {
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT,
        EGL_DMA_BUF_PLANE3_PITCH_EXT,
      };
      static const EGLint planeModLoAttrs[WLR_DMABUF_MAX_PLANES] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
      };
      static const EGLint planeModHiAttrs[WLR_DMABUF_MAX_PLANES] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT,
      };

      std::vector<EGLint> imageAttrs;
      imageAttrs.reserve(4 + dmabuf.n_planes * 6 + 1);
      imageAttrs.push_back(EGL_WIDTH);
      imageAttrs.push_back(dmabuf.width);
      imageAttrs.push_back(EGL_HEIGHT);
      imageAttrs.push_back(dmabuf.height);
      imageAttrs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
      imageAttrs.push_back(static_cast<EGLint>(dmabuf.format));

      for (int i = 0; i < dmabuf.n_planes; ++i) {
        imageAttrs.push_back(planeFdAttrs[i]);
        imageAttrs.push_back(dmabuf.fd[i]);
        imageAttrs.push_back(planeOffsetAttrs[i]);
        imageAttrs.push_back(static_cast<EGLint>(dmabuf.offset[i]));
        imageAttrs.push_back(planePitchAttrs[i]);
        imageAttrs.push_back(static_cast<EGLint>(dmabuf.stride[i]));
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
          imageAttrs.push_back(planeModLoAttrs[i]);
          imageAttrs.push_back(static_cast<EGLint>(dmabuf.modifier & 0xFFFFFFFFu));
          imageAttrs.push_back(planeModHiAttrs[i]);
      imageAttrs.push_back(static_cast<EGLint>(dmabuf.modifier >> 32));
        }
      }
      imageAttrs.push_back(EGL_NONE);

      EGLImageKHR image = EGL_NO_IMAGE_KHR;
      if (gEglCreateImageKHR) {
        image = gEglCreateImageKHR(
          eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, imageAttrs.data());
      }
      if (image != EGL_NO_IMAGE_KHR) {
        glActiveTexture(textureUnit);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gEGLImageTargetTexture2DOES(GL_TEXTURE_2D, reinterpret_cast<GLeglImageOES>(image));
        GLenum importErr = glGetError();
        if (importErr == GL_NO_ERROR) {
          destroyImportedImage();
          wlr_buffer_lock(buffer);
          importedImage = image;
          importedBuffer = buffer;
          importedBufferLocked = true;
          uploadedWidth = width;
          uploadedHeight = height;
          needsImport = false;
          return;
        }
        if (gEglDestroyImageKHR) {
          gEglDestroyImageKHR(eglDisplay, image);
        }
      }
    }
  }
  }

  // Slow fallback: CPU read + GL upload for non-dmabuf buffers.
  destroyImportedImage();
  size_t stride = 0;
  void* data = nullptr;
  uint32_t format = 0;
  const uint8_t* src = nullptr;
  size_t srcStride = 0;
  std::vector<uint8_t> fallbackPixels;
  bool beganDataPtrAccess = false;
  bool haveData = false;

  if (wlr_buffer_begin_data_ptr_access(buffer,
                                       WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                       &data,
                                       &format,
                                       &stride)) {
    src = static_cast<const uint8_t*>(data);
    srcStride = stride;
    beganDataPtrAccess = true;
    haveData = true;
  } else if (renderer) {
    // Fallback: read pixels via wlroots renderer (handles dmabuf-only buffers).
    wlr_texture* tex = wlr_texture_from_buffer(renderer, buffer);
    if (tex) {
      fallbackPixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
      struct wlr_box srcBox = { 0, 0, width, height };
      wlr_texture_read_pixels_options opts{
        fallbackPixels.data(),
        DRM_FORMAT_ABGR8888,
        static_cast<uint32_t>(width * 4),
        0,
        0,
        srcBox,
      };
      if (wlr_texture_read_pixels(tex, &opts)) {
        src = fallbackPixels.data();
        srcStride = opts.stride;
        stride = srcStride;
        format = opts.format;
        haveData = true;
      }
      wlr_texture_destroy(tex);
    }
  }

  if (!haveData) {
    return;
  }

  // Fast path: directly upload all common 32-bit formats without swizzle when
  // stride matches. This avoids per-pixel conversion for repaint-heavy apps.
  auto try_direct_upload = [&]() -> bool {
    if (width <= 0 || height <= 0) {
      return false;
    }
    if (srcStride != static_cast<size_t>(width) * 4) {
      return false;
    }
    GLenum glFormat = 0;
    switch (format) {
      case DRM_FORMAT_ARGB8888: // memory: B G R A
        glFormat = GL_BGRA;
        break;
      case DRM_FORMAT_ABGR8888: // memory: R G B A
        glFormat = GL_RGBA;
        break;
      case DRM_FORMAT_XRGB8888: // memory: B G R X (opaque)
        glFormat = GL_BGRA;
        break;
      case DRM_FORMAT_XBGR8888: // memory: R G B X (opaque)
        glFormat = GL_RGBA;
        break;
      default:
        return false;
    }
    // Ensure BGRA is supported before issuing the upload; otherwise fall back
    // to the conversion path to avoid undefined behaviour on drivers lacking
    // the extension.
    bool bgraSupported = false;
#ifdef GLAD_GL_EXT_texture_format_BGRA8888
    bgraSupported = bgraSupported || GLAD_GL_EXT_texture_format_BGRA8888;
#endif
#ifdef GLAD_GL_APPLE_texture_format_BGRA8888
    bgraSupported = bgraSupported || GLAD_GL_APPLE_texture_format_BGRA8888;
#endif
#ifdef GLAD_GL_EXT_read_format_bgra
    bgraSupported = bgraSupported || GLAD_GL_EXT_read_format_bgra;
#endif
#ifdef GLAD_GL_IMG_read_format
    bgraSupported = bgraSupported || GLAD_GL_IMG_read_format;
#endif
#ifdef GLAD_GL_OES_required_internalformat
    bgraSupported = bgraSupported || GLAD_GL_OES_required_internalformat;
#endif
    if (glFormat == GL_BGRA && !bgraSupported) {
      return false;
    }
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 width,
                 height,
                 0,
                 glFormat,
                 GL_UNSIGNED_BYTE,
                 src);
    uploadedWidth = width;
    uploadedHeight = height;
    GLenum errAfter = glGetError();
    needsImport = false;
    if (beganDataPtrAccess) {
      wlr_buffer_end_data_ptr_access(buffer);
    }
    importedBuffer = buffer;
    return errAfter == GL_NO_ERROR;
  };
  if (try_direct_upload()) {
    return;
  }

  // Convert to RGBA8 if the format isn't directly supported in GLES2.
  std::vector<uint8_t> converted;
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
        case DRM_FORMAT_XBGR2101010: { // B,G,R,X 10-bit
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
        case DRM_FORMAT_ARGB2101010: { // B,G,R,A 10-bit with 2-bit alpha
          uint32_t v = *reinterpret_cast<const uint32_t*>(p);
          uint32_t br = (v >> 0) & 0x3FF;
          uint32_t bg = (v >> 10) & 0x3FF;
          uint32_t bb = (v >> 20) & 0x3FF;
          uint32_t ba = (v >> 30) & 0x3;
          r = (uint8_t)((br * 255) / 1023);
          g = (uint8_t)((bg * 255) / 1023);
          b = (uint8_t)((bb * 255) / 1023);
          a = (uint8_t)((ba * 255) / 3);
          break;
        }
        case DRM_FORMAT_ABGR2101010: { // R,G,B,A 10-bit with 2-bit alpha
          uint32_t v = *reinterpret_cast<const uint32_t*>(p);
          uint32_t br = (v >> 0) & 0x3FF;
          uint32_t bg = (v >> 10) & 0x3FF;
          uint32_t bb = (v >> 20) & 0x3FF;
          uint32_t ba = (v >> 30) & 0x3;
          b = (uint8_t)((br * 255) / 1023);
          g = (uint8_t)((bg * 255) / 1023);
          r = (uint8_t)((bb * 255) / 1023);
          a = (uint8_t)((ba * 255) / 3);
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

  bool forceOpaque = ((maxA == 0 || minA == 0) && (maxR | maxG | maxB));
  if (forceOpaque) {
    for (int y = 0; y < height; ++y) {
      uint8_t* row = converted.data() + y * dstStride;
      for (int x = 0; x < width; ++x) {
        row[x * 4 + 3] = 255;
      }
    }
    if (centerPixel != 0 && firstPixel == 0) {
      firstPixel = centerPixel;
    }
    if (firstPixel != 0) {
      firstPixel |= 0x000000FF;
    }
    if (centerPixel != 0) {
      centerPixel |= 0x000000FF;
    }
    maxA = 255;
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
  importedBuffer = buffer;
  needsImport = false;
  if (beganDataPtrAccess) {
    wlr_buffer_end_data_ptr_access(buffer);
  }
}
