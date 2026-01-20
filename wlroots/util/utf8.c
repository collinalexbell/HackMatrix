#include <stdint.h>
#include "util/utf8.h"

static bool in_range(char x, uint8_t low, uint8_t high) {
	uint8_t v = (uint8_t)x;
	return low <= v && v <= high;
}

bool is_utf8(const char *string) {
	/* Returns true iff the string is 'well-formed', as defined by
	 * Unicode Standard 15.0.0. See Chapter 3, D92 and Table 3.7.
	 *
	 * UTF-8 strings are sequences of code points encoded in one of the
	 * following ways. The first byte determines the pattern.
	 *
	 * 00..7F
	 * C2..DF 80..BF
	 * E0     A0..BF 80..BF
	 * E1..EC 80..BF 80..BF
	 * ED     80..9F 80..BF
	 * EE..EF 80..BF 80..BF
	 * F0     90..BF 80..BF 80..BF
	 * F1..F3 80..BF 80..BF 80..BF
	 * F4     80..8F 80..BF 80..BF
	 */
	uint8_t range_table[9][8] = {
		{0x00, 0x7F},
		{0xC2, 0xDF, 0x80, 0xBF},
		{0xE0, 0xE0, 0xA0, 0xBF, 0x80, 0xBF},
		{0xE1, 0xEC, 0x80, 0xBF, 0x80, 0xBF},
		{0xED, 0xED, 0x80, 0x9F, 0x80, 0xBF},
		{0xEE, 0xEF, 0x80, 0xBF, 0x80, 0xBF},
		{0xF0, 0xF0, 0x90, 0xBF, 0x80, 0xBF, 0x80, 0xBF},
		{0xF1, 0xF3, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF},
		{0xF4, 0xF4, 0x80, 0x8F, 0x80, 0xBF, 0x80, 0xBF},
	};
	int lengths[9] = {
		1, 2, 3, 3, 3, 3, 4, 4, 4
	};

	while (string[0]) {
		bool accept = false;
		for (int i = 0; i < 9; i++) {
			if (!in_range(string[0], range_table[i][0],
					range_table[i][1])) {
				continue;
			}
			for (int j = 1; j < lengths[i]; j++) {
				if (!in_range(string[j], range_table[i][2 * j],
						range_table[i][2 * j + 1])) {
					// Early exit is necessary to avoid
					// reading past the null terminator
					return false;
				}
			}
			string += lengths[i];
			accept = true;
			break;
		}
		if (!accept) {
			return false;
		}
	}

	return true;
}
