#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <limits.h>

std::string execdir() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    std::string executablePath = std::string(result, (count > 0) ? count : 0);
    return executablePath.substr(0, executablePath.find_last_of("/\\"));
}

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
    std::string execfile = execdir() + "/matrix";
    int rv = system(execfile.c_str());
    if (rv != 0) {
      pid = fork();
      if (pid == 0) {
        sleep(5);
        system("xdotool search --class Terminator | xargs ./build/x-raise");
        exit(0);
      }
      terminator();
    }
  }
}
