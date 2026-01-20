icondir = get_option('icon_directory')
if icondir == ''
	icondir = get_option('prefix') / get_option('datadir') / 'icons'
endif
internal_config.set_quoted('ICONDIR', icondir)

wlr_files += files(
	'wlr_xcursor.c',
	'xcursor.c',
)
