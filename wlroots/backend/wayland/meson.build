wayland_client = dependency('wayland-client',
	kwargs: wayland_kwargs,
)
wlr_deps += wayland_client

wlr_files += files(
	'backend.c',
	'output.c',
	'seat.c',
	'pointer.c',
	'tablet_v2.c',
)

client_protos = [
	'drm',
	'linux-dmabuf-v1',
	'linux-drm-syncobj-v1',
	'pointer-gestures-unstable-v1',
	'presentation-time',
	'relative-pointer-unstable-v1',
	'tablet-v2',
	'viewporter',
	'xdg-activation-v1',
	'xdg-decoration-unstable-v1',
	'xdg-shell',
]

foreach proto : client_protos
	wlr_files += protocols_client_header[proto]
endforeach
