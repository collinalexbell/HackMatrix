wlroots reads these environment variables

# wlroots specific

* *WLR_BACKENDS*: comma-separated list of backends to use (available backends:
  libinput, drm, wayland, x11, headless)
* *WLR_NO_HARDWARE_CURSORS*: set to 1 to use software cursors instead of
  hardware cursors
* *WLR_XWAYLAND*: specifies the path to an Xwayland binary to be used (instead
  of following shell search semantics for "Xwayland")
* *WLR_RENDERER*: forces the creation of a specified renderer (available
  renderers: gles2, pixman, vulkan)
* *WLR_RENDER_DRM_DEVICE*: specifies the DRM node to use for
  hardware-accelerated renderers.
* *WLR_RENDER_NO_EXPLICIT_SYNC*: set to 1 to disable explicit synchronization
  support in renderers.
* *WLR_RENDERER_FORCE_SOFTWARE*: set to 1 to force software rendering for GLES2
  and Vulkan
* *WLR_EGL_NO_MODIFIERS*: set to 1 to disable format modifiers in EGL, this can
  be used to understand and work around driver bugs.

## DRM backend

* *WLR_DRM_DEVICES*: specifies the DRM devices (as a colon separated list)
  instead of auto probing them. The first existing device in this list is
  considered the primary DRM device.
* *WLR_DRM_NO_ATOMIC*: set to 1 to use legacy DRM interface instead of atomic
  mode setting
* *WLR_DRM_NO_MODIFIERS*: set to 1 to always allocate planes without modifiers,
  this can fix certain modeset failures because of bandwidth restrictions.
* *WLR_DRM_FORCE_LIBLIFTOFF*: set to 1 to force libliftoff (by default,
  libliftoff is never used)

## Headless backend

* *WLR_HEADLESS_OUTPUTS*: when using the headless backend specifies the number
  of outputs

## libinput backend

* *WLR_LIBINPUT_NO_DEVICES*: set to 1 to not fail without any input devices

## Wayland backend

* *WLR_WL_OUTPUTS*: when using the wayland backend specifies the number of outputs

## X11 backend

* *WLR_X11_OUTPUTS*: when using the X11 backend specifies the number of outputs

## gles2 renderer

* *WLR_RENDERER_ALLOW_SOFTWARE*: allows the gles2 renderer to use software
  rendering

## scenes

* *WLR_SCENE_DEBUG_DAMAGE*: specifies debug options for screen damage related
  tasks for compositors that use scenes (available options: none, rerender,
  highlight)
* *WLR_SCENE_DISABLE_DIRECT_SCANOUT*: disables direct scan-out for debugging.
* *WLR_SCENE_DISABLE_VISIBILITY*: If set to 1, the visibility of all scene nodes
  will be considered to be the full node. Intelligent visibility canculations will
  be disabled. Note that direct scanout will not work for most cases when this
  option is set as surfaces that don't contribute to the rendered output will now
  bail direct scanout (desktop background / black rect underneath).
* *WLR_SCENE_HIGHLIGHT_TRANSPARENT_REGION*: Highlights regions of scene buffers
  that are advertised as transparent through wlr_scene_buffer_set_opaque_region().
  This can be used to debug issues with clients advertizing bogus opaque regions
  with scene based compositors.

# Generic

* *DISPLAY*: if set probe X11 backend in `wlr_backend_autocreate`
* *WAYLAND_DISPLAY*, *WAYLAND_SOCKET*: if set probe Wayland backend in
  `wlr_backend_autocreate`
* *XCURSOR_PATH*: directory where xcursors are located
* *XDG_SESSION_ID*: if set, session ID used by the logind session
