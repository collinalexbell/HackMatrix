#pragma once
#include "components/RotateMovement.h"
#include <entt.hpp>

enum TurnState {
  TURNED, TURNING, UNTURNED, UNTURINING
};

struct Key {
  entt::entity lockable;
  TurnState state;
  RotateMovement turnMovement;
  RotateMovement unturnMovement;
};
