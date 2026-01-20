allocators = get_option('allocators')
if 'auto' in allocators and get_option('auto_features').enabled()
	allocators = ['gbm', 'udmabuf']
elif 'auto' in allocators and get_option('auto_features').disabled()
	allocators = []
endif

wlr_files += files(
	'allocator.c',
	'shm.c',
	'drm_dumb.c',
)

gbm = disabler()
if 'gbm' in allocators or 'auto' in allocators
	gbm = dependency('gbm', version: '>=21.1', required: 'gbm' in allocators)
endif
if gbm.found()
	wlr_files += files('gbm.c')
	wlr_deps += gbm
	features += { 'gbm-allocator': true }
endif

udmabuf = false
if 'udmabuf' in allocators or 'auto' in allocators
	args = ['-D_GNU_SOURCE']
	prefix = [
		'#include <sys/mman.h>',
		'#include <fcntl.h>',
	]
	udmabuf = (cc.has_function('memfd_create', args: args, prefix: prefix) and
		cc.has_define('F_ADD_SEALS', args: args, prefix: prefix) and
		cc.has_header('linux/udmabuf.h'))
endif
if 'udmabuf' in allocators and not udmabuf
	error('memfd_create(), F_ADD_SEALS and <linux/udmabuf.h> are required for the udmabuf allocator')
endif
if udmabuf
	wlr_files += files('udmabuf.c')
	features += { 'udmabuf-allocator': true }
endif
