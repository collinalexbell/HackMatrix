/*
 * This is a stable interface of wlroots. Future changes will be limited to:
 *
 * - New functions
 * - New struct members
 * - New enum members
 *
 * Note that wlroots does not make an ABI compatibility promise - in the future,
 * the layout and size of structs used by wlroots may change, requiring code
 * depending on this header to be recompiled (but not edited).
 *
 * Breaking changes are announced in the release notes and follow a 1-year
 * deprecation schedule.
 */

#ifndef WLR_UTIL_LOG_H
#define WLR_UTIL_LOG_H

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

enum wlr_log_importance {
	WLR_SILENT = 0,
	WLR_ERROR = 1,
	WLR_INFO = 2,
	WLR_DEBUG = 3,
	WLR_LOG_IMPORTANCE_LAST,
};

typedef void (*wlr_log_func_t)(enum wlr_log_importance importance,
	const char *fmt, va_list args);

/**
 * Set the log verbosity and callback.
 *
 * Only messages less than or equal to the supplied verbosity will be logged.
 * If the callback is NULL, the default logger is used.
 *
 * This function can be called multiple times to update the verbosity or
 * callback function.
 */
void wlr_log_init(enum wlr_log_importance verbosity, wlr_log_func_t callback);

/**
 * Get the current log verbosity configured by wlr_log_init().
 */
enum wlr_log_importance wlr_log_get_verbosity(void);

#ifdef __GNUC__
#define _WLR_ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define _WLR_ATTRIB_PRINTF(start, end)
#endif

void _wlr_log(enum wlr_log_importance verbosity, const char *format, ...) _WLR_ATTRIB_PRINTF(2, 3);
void _wlr_vlog(enum wlr_log_importance verbosity, const char *format, va_list args) _WLR_ATTRIB_PRINTF(2, 0);

#ifdef _WLR_REL_SRC_DIR
// strip prefix from __FILE__, leaving the path relative to the project root
#define _WLR_FILENAME ((const char *)__FILE__ + sizeof(_WLR_REL_SRC_DIR) - 1)
#else
#define _WLR_FILENAME __FILE__
#endif

#define wlr_vlog(verb, fmt, args) \
	_wlr_vlog(verb, "[%s:%d] " fmt, _WLR_FILENAME, __LINE__, args)

#if __STDC_VERSION__ >= 202311L

#define wlr_log(verb, fmt, ...) \
	_wlr_log(verb, "[%s:%d] " fmt, _WLR_FILENAME, __LINE__ __VA_OPT__(,) __VA_ARGS__)
#define wlr_log_errno(verb, fmt, ...) \
	wlr_log(verb, fmt ": %s" __VA_OPT__(,) __VA_ARGS__, strerror(errno))

#else

#define wlr_log(verb, fmt, ...) \
	_wlr_log(verb, "[%s:%d] " fmt, _WLR_FILENAME, __LINE__, ##__VA_ARGS__)
#define wlr_log_errno(verb, fmt, ...) \
	wlr_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#endif

#endif
