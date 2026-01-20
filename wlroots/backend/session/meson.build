msg = 'Required for session support.'
udev = dependency('libudev', required: session_required, not_found_message: msg)
libseat = dependency(
	'libseat',
	version: '>=0.2.0',
	fallback: 'seatd',
	default_options: ['server=disabled', 'man-pages=disabled', 'examples=disabled'],
	required: session_required,
	not_found_message: msg,
)
if not (udev.found() and libseat.found())
	subdir_done()
endif

wlr_files += files('session.c')
wlr_deps += [udev, libseat]
features += { 'session': true }
