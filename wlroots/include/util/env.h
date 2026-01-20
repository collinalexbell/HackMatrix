#ifndef UTIL_ENV_H
#define UTIL_ENV_H

#include <stdbool.h>
#include <unistd.h>

/**
 * Parse a bool from an environment variable.
 *
 * On success, the parsed value is returned. On error, false is returned.
 */
bool env_parse_bool(const char *option);

/**
 * Pick a choice from an environment variable.
 *
 * On success, the choice index is returned. On error, zero is returned.
 *
 * switches is a NULL-terminated array.
 */
size_t env_parse_switch(const char *option, const char **switches);

#endif
