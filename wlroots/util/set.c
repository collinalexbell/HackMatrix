#include "util/set.h"

ssize_t set_add(uint32_t values[], size_t *len, size_t cap, uint32_t target) {
	for (uint32_t i = 0; i < *len; ++i) {
		if (values[i] == target) {
			return i;
		}
	}
	if (*len == cap) {
		return -1;
	}
	values[*len] = target;
	return (*len)++;
}

ssize_t set_remove(uint32_t values[], size_t *len, size_t cap, uint32_t target) {
	for (uint32_t i = 0; i < *len; ++i) {
		if (values[i] == target) {
			--(*len);
			values[i] = values[*len];
			return i;
		}
	}
	return -1;
}
