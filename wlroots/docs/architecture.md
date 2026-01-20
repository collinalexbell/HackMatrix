# Architecture

This document describes the high-level design of wlroots. wlroots is modular:
each module can be used mostly independently from the rest of wlroots. For
instance, some wlroots-based compositors only use its backends, some only use
its protocol implementations.

## Backends

Backends are responsible for exposing input devices and output devices.
wlroots provides DRM and libinput backends to directly drive physical
devices, Wayland and X11 backends to run nested inside another compositor,
and a headless backend. A special "multi" backend is used to combine together
multiple backends, for instance DRM and libinput. Compositors can also
implement their own custom backends if they have special needs.

Input devices such as pointers, keyboards, touch screens, tablets, switches
are supported. They emit input events (e.g. a keyboard key is pressed) which
compositors can handle and forward to Wayland clients.

Output devices are tasked with presenting buffers to the user. They also
provide feedback, for instance presentation timestamps. Some backends support
more advanced functionality, such as displaying multiple buffers (e.g. for the
cursor image) or basic 2D transformations (e.g. rotation, clipping, scaling).

## Renderers

Renderers provide utilities to import buffers submitted by Wayland clients,
and a basic 2D drawing API suitable for simple compositors. wlroots provides
renderer implementations based on OpenGL ES 2, Vulkan and Pixman. Just like
backends, compositors can implement their own renderers, or use the graphics
APIs directly.

To draw an image onto a buffer, compositors will first need to create a
texture, representing a source of pixels the renderer can sample from. This can
be done either by uploading pixels from CPU memory, or by importing already
existing GPU memory via DMA-BUFs. Compositors can then create a render pass
and submit drawing operations. Once they are done drawing, compositors can
submit the rendered buffer to an output.

## Protocol implementations

A number of Wayland interface implementations are provided.

### Plumbing protocols

wlroots ships unopinionated implementations of core plumbing interfaces, for
instance:

- `wl_compositor` and `wl_surface`
- `wl_seat` and all input-related interfaces
- Buffer factories such as `wl_shm` and linux-dmabuf
- Additional protocols such as viewporter and presentation-time

### Shells

Shells give a meaning to surfaces. There are many kinds of surfaces:
application windows, tooltips, right-click menus, desktop panels, wallpapers,
lock screens, on-screen keyboards, and so on. Each of these use-cases is
fulfilled with a shell. wlroots supports xdg-shell for regular windows and
popups, Xwayland for interoperability with X11 applications, layer-shell for
desktop UI elements, and more.

### Other protocols

Many other protocol implementations are included, for instance:

- xdg-activation for raising application windows
- idle-inhibit for preventing the screen from blanking when the user is
  watching a video
- ext-idle-notify for notifying when the user is idle

## Helpers

wlroots provides additional helpers which can make it easier for compositors to
tie everything together:

- `wlr_output_layout` organises output devices in the physical space
- `wlr_cursor` stores the current position and image of the cursor
- `wlr_scene` provides a declarative way to display surfaces

## tinywl

tinywl is a minimal wlroots compositor. It implements basic stacking window
management and only supports xdg-shell. It's extensively commented and is a
good learning resource for developers new to wlroots.
