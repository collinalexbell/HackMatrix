cairo = dependency('cairo', required: false, disabler: true)
# Only needed for drm_fourcc.h
libdrm_header = dependency('libdrm').partial_dependency(compile_args: true, includes: true)
wayland_client = dependency('wayland-client', required: false, disabler: true)
wayland_egl = dependency('wayland-egl', required: false, disabler: true)
egl = dependency('egl', version: '>= 1.5', required: false, disabler: true)
glesv2 = dependency('glesv2', required: false, disabler: true)

compositors = {
	'simple': {
		'src': 'simple.c',
	},
	'pointer': {
		'src': 'pointer.c',
	},
	'touch': {
		'src': ['touch.c', 'cat.c'],
	},
	'tablet': {
		'src': 'tablet.c',
	},
	'rotation': {
		'src': ['rotation.c', 'cat.c'],
	},
	'output-layout': {
		'src': ['output-layout.c', 'cat.c'],
	},
	'scene-graph': {
		'src': 'scene-graph.c',
		'proto': ['xdg-shell'],
	},
	'output-layers': {
		'src': 'output-layers.c',
		'proto': [
			'xdg-shell',
		],
	},
	'cairo-buffer': {
		'src': 'cairo-buffer.c',
		'dep': cairo,
	},
	'embedded': {
		'src': [
			'embedded.c',
			protocols_code['xdg-shell'],
			protocols_client_header['xdg-shell'],
		],
		'dep': [wayland_client, wayland_egl, egl, glesv2],
	},
}

foreach name, info : compositors
	extra_src = []
	foreach p : info.get('proto', [])
		extra_src += protocols_server_header[p]
	endforeach

	executable(
		name,
		[info.get('src'), extra_src],
		dependencies: [wlroots, libdrm_header, info.get('dep', [])],
		build_by_default: get_option('examples'),
	)
endforeach
