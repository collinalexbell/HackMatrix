hwdata = dependency(
	'hwdata',
	required: 'drm' in backends,
	native: true,
	not_found_message: 'Required for the DRM backend.',
)

libdisplay_info = dependency(
	'libdisplay-info',
	version: '>=0.2.0',
	required: 'drm' in backends,
	fallback: 'libdisplay-info',
	not_found_message: 'Required for the DRM backend.',
)

libliftoff = dependency(
	'libliftoff',
	version: '>=0.4.0',
	fallback: 'libliftoff',
	required: get_option('libliftoff'),
)

if not (hwdata.found() and libdisplay_info.found() and features['session'])
	subdir_done()
endif

hwdata_dir = hwdata.get_variable(pkgconfig: 'pkgdatadir')
pnpids_c = custom_target(
	'pnpids.c',
	output: 'pnpids.c',
	input: files(hwdata_dir / 'pnp.ids'),
	feed: true,
	capture: true,
	command: files('gen_pnpids.sh'),
)
wlr_files += pnpids_c

wlr_files += files(
	'atomic.c',
	'backend.c',
	'drm.c',
	'fb.c',
	'legacy.c',
	'monitor.c',
	'properties.c',
	'renderer.c',
	'util.c',
)

if libliftoff.found()
	wlr_files += files('libliftoff.c')
	internal_config.set10('HAVE_LIBLIFTOFF_0_5', libliftoff.version().version_compare('>=0.5.0'))
endif

features += { 'drm-backend': true }
internal_features += { 'libliftoff': libliftoff.found() }
wlr_deps += libdisplay_info
wlr_deps += libliftoff
