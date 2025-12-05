CC = gcc
CFLAGS = -std=c11 -DWLR_USE_UNSTABLE \
  -I/usr/local/include/wlroots-0.18 \
  -I /usr/include/pixman-1 \

LDLIBS = $(shell pkg-config --libs xkbcommon pixman-1 wayland-server)

main: main.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

