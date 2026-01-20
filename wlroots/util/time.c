#include <stdint.h>
#include <time.h>

#include "util/time.h"

int64_t timespec_to_msec(const struct timespec *a) {
	return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
}

int64_t timespec_to_nsec(const struct timespec *a) {
	return (int64_t)a->tv_sec * NSEC_PER_SEC + a->tv_nsec;
}

void timespec_from_nsec(struct timespec *r, int64_t nsec) {
	r->tv_sec = nsec / NSEC_PER_SEC;
	r->tv_nsec = nsec % NSEC_PER_SEC;
}

int64_t get_current_time_msec(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return timespec_to_msec(&now);
}

void timespec_sub(struct timespec *r, const struct timespec *a,
		const struct timespec *b) {
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}
