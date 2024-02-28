#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void terminator() {
  pid_t pid = fork(); // Create a child process

  if (pid == 0) {
    // Child process: Execute terminator
    if (execlp("terminator", "terminator", NULL) == -1) {
      std::cerr << "Error executing terminator" << std::endl;
      exit(1);
    }
    exit(0);
  } else if (pid > 0) {
    // Parent process: Wait for terminator to finish
    int status;
    waitpid(pid, &status, 0);
  }
}

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
      terminator();
    }
  }
}
