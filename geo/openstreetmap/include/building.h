#include "coreStructs.h"
#include <vector>
#include <iostream>
#include "apiInterface.h"
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

  void draw(ApiInterface *api) {
    // TODO: add height
    vector<AbsolutePosition> corners = getCorners();
    bool first = true;
    int x[2];
    int z[2];
    for (auto corner : corners) {
      if (first || corner.x < x[0]) {
        x[0] = corner.x;
      }
      if (first || corner.x > x[1]) {
        x[1] = corner.x;
      }
      if (first || corner.z < z[0]) {
        z[0] = corner.z;
      }
      if (first || corner.z > z[1]) {
        z[1] = corner.z;
      }
      first = false;
    }

    int height = 10;
    for (int y = 6; y < height + 6; y++) {
      for (int xs = x[0]; xs < x[1]; xs++) {
        api->addCube(xs - 650, y, -1 * (z[0] - 50), 2);
        api->addCube(xs - 650, y, -1 * (z[1] - 50), 2);
      }

      for (int zs = z[0]; zs < z[1]; zs++) {
        api->addCube(x[0] - 650, y, -1 * (zs - 50), 2);
        api->addCube(x[1] - 650, y, -1 * (zs - 50), 2);
      }
    }
    for(int xs = x[0]; xs < x[1]; xs++) {
      for(int zs = z[0]; zs < z[1]; zs++) {
        api->addCube(xs - 650, 16, -1 * (zs - 50), 2);
      }
    }
  }
};
