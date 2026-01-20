#include <wlr/util/log.h>

#include "render/dmabuf.h"

bool dmabuf_check_sync_file_import_export(void) {
	return false;
}

bool dmabuf_import_sync_file(int dmabuf_fd, uint32_t flags, int sync_file_fd) {
	wlr_log(WLR_ERROR, "DMA-BUF sync_file import IOCTL not available on this system");
	return false;
}

int dmabuf_export_sync_file(int dmabuf_fd, uint32_t flags) {
	wlr_log(WLR_ERROR, "DMA-BUF sync_file export IOCTL not available on this system");
	return false;
}
