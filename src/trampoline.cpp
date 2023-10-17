#include <cstdlib>
#include <unistd.h>
int main(int argc, char** argv, char** envp) {
  int pid = fork();
  if(pid == 0) {
    chdir("/home/collin/cyber-logos");
    execle("start", "start", NULL, envp);
    exit(0);
  }
  sleep(5);
  while(true) {
    int rv = system("/home/collin/matrix/matrix");
    if(rv != 0) {
      pid = fork();
      if(pid == 0) {
        sleep(2);
        system("xdotool search --class Xterm | xargs ./build/x-raise");
        exit(0);
      }
      int exit = system("xterm");
      if(exit == 42) {
        break;
      }
    }
  }
}
