#include "time_utils.h"
#include <GLFW/glfw3.h>
#include <chrono>

double
nowSeconds()
{
  double t = glfwGetTime();
  if (t > 0.0) {
    return t;
  }
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - start;
  return elapsed.count();
}

