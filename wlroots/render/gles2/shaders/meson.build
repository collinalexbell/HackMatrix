embed = find_program('./embed.sh', native: true)

shaders = [
	'common.vert',
	'quad.frag',
	'tex_rgba.frag',
	'tex_rgbx.frag',
	'tex_external.frag',
]

foreach name : shaders
	custom_target(
		'gles2-' + name,
		input: name,
		output: name + '_check',
		command: [glslang, '@INPUT@'],
		build_by_default: true,
	)

	output = name.underscorify() + '_src.h'
	var = name.underscorify() + '_src'
	wlr_files += custom_target(
		output,
		command: [embed, var],
		input: name,
		output: output,
		feed: true,
		capture: true,
	)
endforeach
