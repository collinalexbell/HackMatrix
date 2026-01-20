#!/bin/sh -eu
#
# usage: gen_pnpids.sh < pnp.ids > pnpids.c

gen_pnps()
{
	while read -r id vendor; do
		[ "${#id}" = 3 ] || exit 1

		printf "\tcase PNP_ID('%c', '%c', '%c'): return \"%s\";\n" \
			"$id" "${id#?}" "${id#??}" "$vendor"
	done
}

cat << EOF
#include "backend/drm/util.h"

#define PNP_ID(a, b, c) ((a & 0x1f) << 10) | ((b & 0x1f) << 5) | (c & 0x1f)
const char *get_pnp_manufacturer(const char code[static 3]) {
	switch (PNP_ID(code[0], code[1], code[2])) {
$(gen_pnps)
	}
	return NULL;
}
#undef PNP_ID
EOF
