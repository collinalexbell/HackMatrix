renderers = get_option('renderers')
if 'auto' in renderers and get_option('auto_features').enabled()
	renderers = ['gles2', 'vulkan']
elif 'auto' in renderers and get_option('auto_features').disabled()
	renderers = []
endif

wlr_files += files(
	'color.c',
	'dmabuf.c',
	'drm_format_set.c',
	'drm_syncobj.c',
	'pass.c',
	'pixel_format.c',
	'swapchain.c',
	'wlr_renderer.c',
	'wlr_texture.c',
)

if cc.has_header('linux/dma-buf.h') and target_machine.system() == 'linux'
	wlr_files += files('dmabuf_linux.c')
else
	wlr_files += files('dmabuf_fallback.c')
endif

internal_config.set10('HAVE_EVENTFD', cc.has_header('sys/eventfd.h'))

if 'gles2' in renderers or 'auto' in renderers
	egl = dependency('egl', required: 'gles2' in renderers)
	if egl.found()
		eglext_version = cc.get_define(
			'EGL_EGLEXT_VERSION',
			dependencies: egl,
			prefix: '#include <EGL/eglext.h>',
		).to_int()
		if eglext_version < 20210604
			egl = dependency(
				'',
				required: 'gles2' in renderers,
				not_found_message: 'EGL headers too old',
			)
		endif
	endif
	gbm = dependency('gbm', required: 'gles2' in renderers)
	if egl.found() and gbm.found()
		wlr_deps += [egl, gbm]
		wlr_files += files('egl.c')
		internal_features += { 'egl': true }
	endif
	subdir('gles2')
endif

if 'vulkan' in renderers or 'auto' in renderers
	subdir('vulkan')
endif

subdir('pixman')

subdir('allocator')

lcms2 = dependency('lcms2', required: get_option('color-management'))
if lcms2.found()
	wlr_deps += lcms2
	wlr_files += files('color_lcms2.c')
	features += { 'color-management': true }
else
	wlr_files += files('color_fallback.c')
endif
