#ifndef RENDER_DMABUF_H
#define RENDER_DMABUF_H

#include <stdbool.h>
#include <stdint.h>

// Copied from <linux/dma-buf.h> to avoid #ifdef soup
#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_WRITE     (2 << 0)
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)

/**
 * Check whether DMA-BUF import/export from/to sync_file is available.
 *
 * If this function returns true, dmabuf_import_sync_file() is supported.
 */
bool dmabuf_check_sync_file_import_export(void);

/**
 * Import a sync_file into a DMA-BUF with DMA_BUF_IOCTL_IMPORT_SYNC_FILE.
 *
 * This can be used to make explicit sync interoperate with implicit sync.
 */
bool dmabuf_import_sync_file(int dmabuf_fd, uint32_t flags, int sync_file_fd);

/**
 * Export a sync_file from a DMA-BUF with DMA_BUF_IOCTL_EXPORT_SYNC_FILE.
 *
 * The sync_file FD is returned on success, -1 is returned on error.
 *
 * This can be used to make explicit sync interoperate with implicit sync.
 */
int dmabuf_export_sync_file(int dmabuf_fd, uint32_t flags);

#endif
