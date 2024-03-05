#pragma once
#include "RotateMovement.h"

enum DoorState {
  OPEN, OPENING, CLOSING, CLOSED
};

struct Door {
  RotateMovement openMovement;
  RotateMovement closeMovement;
  DoorState state;
};

