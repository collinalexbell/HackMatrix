glesv2 = dependency('glesv2', required: 'gles2' in renderers)

if not (glesv2.found() and internal_features['egl'])
	subdir_done()
endif

glslang = find_program('glslang', 'glslangValidator', native: true, required: false, disabler: true)

features += { 'gles2-renderer': true }
wlr_deps += glesv2

wlr_files += files(
	'pass.c',
	'pixel_format.c',
	'renderer.c',
	'texture.c',
)

subdir('shaders')
