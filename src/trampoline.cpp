#include <cstdlib>
#include <unistd.h>
int main(int argc, char** argv, char** envp) {
  int pid ;
  while (true) {
    int rv = system("/home/collin/matrix/matrix");
    if (rv != 0) {
      pid = fork();
      if (pid == 0) {
        sleep(2);
        system("xdotool search --class Terminator | xargs ./build/x-raise");
        exit(0);
      }
      int exit = system("terminator");
      if (exit == 42) {
        break;
      }
    }
  }
}
