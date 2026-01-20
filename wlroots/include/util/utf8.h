#ifndef UTIL_UTF8_H
#define UTIL_UTF8_H

#include <stdbool.h>

/**
 * Return true if and only if the string is a valid UTF-8 sequence.
 */
bool is_utf8(const char *string);

#endif
