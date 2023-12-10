#include "coreStructs.h"
#include <vector>
#include <iostream>
#pragma once

using namespace std;

class Building {
  vector<AbsolutePosition> corners;
 public:
  void addCorner(AbsolutePosition corner) {
    if(corners.size() < 4) {
      corners.push_back(corner);
    }
  }
  vector<AbsolutePosition> getCorners() {
    return corners;
  }

  int size() {
    return corners.size();
  }

  void printCorners() {
    cout << "(";
    for(auto corner: corners) {
      cout << "(";
      cout << corner.x << "," << corner.y << "," << corner.z;
      cout << ")";
    }
    cout << ")" << endl;
  }
};
