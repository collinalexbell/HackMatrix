#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/vt.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

int
switchToTTY(int ttyNumber)
{
  char command[50];
  // Use the chvt command with sudo to switch to the specified virtual terminal
  std::snprintf(command, sizeof(command), "chvt %d", ttyNumber);
  return std::system(command);
}

int
main(int argc, char** argv)
{
  // Switch to TTY3 (you can change the TTY number as needed)
  int tty = 3;
  if (argc >= 2) {
    tty = std::stoi(argv[1]);
  }
  if (switchToTTY(tty) == 0) {
    printf("Switched to TTY3\n");
  } else {
    printf("Failed to switch to TTY3\n");
  }
  return 0;
}
