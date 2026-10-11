/*
 Copyright (C) 2026 KreedzBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "cs/scs.h"

#include <array>
#include <iosfwd>
#include <string>
#include <vector>

// Replays ground-truth scripts (see fixture/ground-truth/README.md) through TbCsLib and
// reads player state logs recorded on HLDS.
namespace tb::cs::groundtruth
{

// The player state after one command, as the recorder logs it.
struct Record
{
  long command = 0;
  std::array<float, 3> origin{};
  std::array<float, 3> velocity{};
  int flags = 0; // FL_ONGROUND, FL_DUCKING, FL_INWATER and FL_WATERJUMP only
  int inDuck = 0;
  int duckTime = 0;
  float fuser2 = 0;
  float fallVelocity = 0;
  int waterLevel = 0;
  float viewOfsZ = 0;
  float maxSpeed = 0;
};

// The commands after one "reset" line.
struct Segment
{
  std::string name; // from the "# name" comment before the reset, if any
  std::vector<Record> records;
};

// The configuration the recorder's player has: a pistol (250 u/s), multiplayer, and
// already quantized commands.
scs_config recorderConfig();

// Runs the script on ctx, which must have a world. Throws std::runtime_error on
// malformed lines.
std::vector<Segment> replay(scs_context& ctx, std::istream& script);

// Reads a recorder (or replay) log. Throws std::runtime_error on malformed lines or if
// the recorder reported an error.
std::vector<Segment> readLog(std::istream& log);

// The result of checkClientInput.
struct InputCheck
{
  long frames = 0;
  long pressFrames = 0; // frames in which a movement key went down
  long truncated = 0;   // recorded with moves already truncated to integers
  long otherSpeed = 0;  // built for another weapon's speed, or while frozen
  long differences = 0;
  std::vector<std::string> examples; // the first differences
};

// Replays the user commands of a demo (see fixture/ground-truth/README.md) through
// scs_build_usercmd: the movement keys each command's buttons show held are pressed and
// released before it is built, so press frames (half speed) and the clip to the
// weapon's speed are exercised like in the game. A command that differs is built again
// with the speed its recorded moves imply; if that matches, the player held another
// weapon (an AWP clips to 210) or was frozen (1). Throws std::runtime_error on
// malformed lines.
InputCheck checkClientInput(std::istream& commands, float clientMaxSpeed = 250.0f);

} // namespace tb::cs::groundtruth
