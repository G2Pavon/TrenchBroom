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

#include "GroundTruth.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <istream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace tb::cs::groundtruth
{
namespace
{

// FL_ONGROUND, FL_DUCKING, FL_INWATER, FL_WATERJUMP
constexpr int kFlagMask = SCS_FL_ONGROUND | SCS_FL_DUCKING | (1 << 4) | (1 << 11);
constexpr float kWeaponMaxSpeed = 250.0f;

// The movement state an extended reset line carries (a recording that starts moving).
struct MovingState
{
  float velocity[3] = {};
  int flags = 0; // of kFlagMask
  int inDuck = 0;
  float duckTime = 0;
  float fuser2 = 0;
  float fallVelocity = 0;
  int oldbuttons = 0;
  float viewOfsZ = 17;
};

// Matches the recorder's reset(): at rest, standing, movement state cleared, unless a
// moving state is given.
void reset(
  scs_context& ctx,
  float x,
  float y,
  float z,
  float pitch,
  float yaw,
  const MovingState& moving = {})
{
  const float origin[3] = {x, y, z};
  const float angles[3] = {pitch, yaw, 0};
  scs_spawn(&ctx, origin, angles);

  auto state = scs_player_state{};
  scs_get_state(&ctx, &state);
  std::memcpy(state.origin, origin, sizeof(origin));
  std::memset(state.velocity, 0, sizeof(state.velocity));
  std::memset(state.basevelocity, 0, sizeof(state.basevelocity));
  std::memset(state.punchangle, 0, sizeof(state.punchangle));
  state.view_ofs[0] = 0;
  state.view_ofs[1] = 0;
  state.view_ofs[2] = 17;
  state.v_angle[0] = pitch;
  state.v_angle[1] = yaw;
  state.v_angle[2] = 0;
  state.flags &= ~kFlagMask;
  state.in_duck = 0;
  state.duck_time = 0;
  state.time_step_sound = 0;
  state.swim_time = 0;
  state.fall_velocity = 0;
  state.fuser[1] = 0;
  state.oldbuttons = 0;
  state.waterlevel = 0;
  state.watertype = 0;
  state.gravity = 1;
  state.friction = 1;
  state.movetype = SCS_MOVETYPE_WALK;
  state.maxspeed = kWeaponMaxSpeed;
  std::memcpy(state.velocity, moving.velocity, sizeof(state.velocity));
  state.flags |= moving.flags & kFlagMask;
  state.in_duck = moving.inDuck;
  state.duck_time = moving.duckTime;
  state.fuser[1] = moving.fuser2;
  state.fall_velocity = moving.fallVelocity;
  state.oldbuttons = moving.oldbuttons;
  state.view_ofs[2] = moving.viewOfsZ;
  scs_set_state(&ctx, &state);
}

Record record(long command, const scs_context& ctx)
{
  auto s = scs_player_state{};
  scs_get_state(&ctx, &s);
  auto r = Record{};
  r.command = command;
  r.origin = {s.origin[0], s.origin[1], s.origin[2]};
  r.velocity = {s.velocity[0], s.velocity[1], s.velocity[2]};
  r.flags = s.flags & kFlagMask;
  r.inDuck = s.in_duck;
  r.duckTime = static_cast<int>(s.duck_time);
  r.fuser2 = s.fuser[1];
  r.fallVelocity = s.fall_velocity;
  r.waterLevel = s.waterlevel;
  r.viewOfsZ = s.view_ofs[2];
  r.maxSpeed = s.maxspeed;
  return r;
}

std::runtime_error error(const std::string& message, const std::string& line)
{
  return std::runtime_error{message + ": " + line};
}

// Parses a float in decimal or C99 hex notation (as %a writes it).
float parseFloat(std::istringstream& in, const std::string& line)
{
  auto word = std::string{};
  if (!(in >> word))
  {
    throw error("missing value", line);
  }
  char* end = nullptr;
  const auto value = std::strtof(word.c_str(), &end);
  if (end == word.c_str() || *end != '\0')
  {
    throw error("bad number", line);
  }
  return value;
}

int parseInt(std::istringstream& in, const std::string& line)
{
  return static_cast<int>(parseFloat(in, line));
}

} // namespace

scs_config recorderConfig()
{
  auto config = scs_config{};
  scs_config_defaults(&config);
  config.clientmaxspeed = kWeaponMaxSpeed;
  config.multiplayer = 1;
  config.quantize_usercmd = 0; // scripts hold already quantized values
  return config;
}

std::vector<Segment> replay(scs_context& ctx, std::istream& script)
{
  auto segments = std::vector<Segment>{};
  auto name = std::string{};
  auto command = long{0};
  auto line = std::string{};
  while (std::getline(script, line))
  {
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    auto in = std::istringstream{line};
    auto word = std::string{};
    if (!(in >> word))
    {
      continue;
    }
    if (word[0] == '#')
    {
      name = line.substr(line.find_first_not_of("# "));
    }
    else if (word == "reset")
    {
      const auto x = parseFloat(in, line);
      const auto y = parseFloat(in, line);
      const auto z = parseFloat(in, line);
      const auto pitch = parseFloat(in, line);
      const auto yaw = parseFloat(in, line);
      auto moving = MovingState{};
      if (!(in >> std::ws).eof())
      {
        // vx vy vz flags in_duck duck_time fuser2 fall_velocity oldbuttons view_ofs_z
        for (auto& v : moving.velocity)
        {
          v = parseFloat(in, line);
        }
        moving.flags = parseInt(in, line);
        moving.inDuck = parseInt(in, line);
        moving.duckTime = parseFloat(in, line);
        moving.fuser2 = parseFloat(in, line);
        moving.fallVelocity = parseFloat(in, line);
        moving.oldbuttons = parseInt(in, line);
        moving.viewOfsZ = parseFloat(in, line);
      }
      reset(ctx, x, y, z, pitch, yaw, moving);
      segments.push_back({name, {}});
      name.clear();
    }
    else if (word == "cmd")
    {
      if (segments.empty())
      {
        throw error("command before the first reset", line);
      }
      auto cmd = scs_usercmd{};
      cmd.msec = static_cast<uint8_t>(parseInt(in, line));
      cmd.viewangles[0] = parseFloat(in, line);
      cmd.viewangles[1] = parseFloat(in, line);
      cmd.forwardmove = parseFloat(in, line);
      cmd.sidemove = parseFloat(in, line);
      cmd.upmove = parseFloat(in, line);
      cmd.buttons = static_cast<uint16_t>(parseInt(in, line));
      scs_step(&ctx, &cmd);
      segments.back().records.push_back(record(command++, ctx));
    }
    else
    {
      throw error("unknown script line", line);
    }
  }
  return segments;
}

std::vector<Segment> readLog(std::istream& log)
{
  auto segments = std::vector<Segment>{};
  auto line = std::string{};
  while (std::getline(log, line))
  {
    auto in = std::istringstream{line};
    auto word = std::string{};
    if (!(in >> word) || word == "spawned" || word == "done")
    {
      continue;
    }
    if (word == "error")
    {
      throw error("the recorder failed", line);
    }
    if (word == "reset")
    {
      segments.emplace_back();
      continue;
    }
    if (segments.empty())
    {
      throw error("record before the first reset", line);
    }
    auto r = Record{};
    r.command = std::atol(word.c_str());
    for (auto& v : r.origin)
    {
      v = parseFloat(in, line);
    }
    for (auto& v : r.velocity)
    {
      v = parseFloat(in, line);
    }
    r.flags = parseInt(in, line);
    r.inDuck = parseInt(in, line);
    r.duckTime = parseInt(in, line);
    r.fuser2 = parseFloat(in, line);
    r.fallVelocity = parseFloat(in, line);
    r.waterLevel = parseInt(in, line);
    r.viewOfsZ = parseFloat(in, line);
    r.maxSpeed = parseFloat(in, line);
    segments.back().records.push_back(r);
  }
  return segments;
}

InputCheck checkClientInput(std::istream& commands, const float clientMaxSpeed)
{
  struct Key
  {
    uint16_t bit;
    scs_kbutton scs_client_input::* button;
  };
  static constexpr Key keys[] = {
    {SCS_IN_FORWARD, &scs_client_input::forward},
    {SCS_IN_BACK, &scs_client_input::back},
    {SCS_IN_MOVELEFT, &scs_client_input::moveleft},
    {SCS_IN_MOVERIGHT, &scs_client_input::moveright},
    {SCS_IN_RUN, &scs_client_input::speed},
    {SCS_IN_JUMP, &scs_client_input::jump},
    {SCS_IN_DUCK, &scs_client_input::duck},
  };

  auto input = scs_client_input{};
  scs_client_input_defaults(&input);

  auto result = InputCheck{};
  auto previous = 0;
  auto line = std::string{};
  while (std::getline(commands, line))
  {
    auto in = std::istringstream{line};
    auto word = std::string{};
    if (!(in >> word))
    {
      continue;
    }
    const auto msec = static_cast<uint8_t>(std::atoi(word.c_str()));
    const auto buttons = parseInt(in, line);
    const auto forward = parseFloat(in, line);
    const auto side = parseFloat(in, line);
    const auto up = parseFloat(in, line);

    auto pressed = false;
    for (auto i = 0; i < int(std::size(keys)); ++i)
    {
      auto& button = input.*keys[i].button;
      const auto down = (buttons & keys[i].bit) != 0;
      const auto wasDown = (previous & keys[i].bit) != 0;
      if (down && !wasDown)
      {
        scs_key_down(&button, i + 1);
        pressed = pressed || keys[i].bit != SCS_IN_JUMP;
      }
      else if (!down && wasDown)
      {
        scs_key_up(&button, i + 1);
      }
    }
    previous = buttons;

    const auto matches = [&](const scs_usercmd& cmd) {
      return cmd.forwardmove == forward && cmd.sidemove == side && cmd.upmove == up;
    };
    const auto matchesTruncated = [&](const scs_usercmd& cmd) {
      return std::trunc(cmd.forwardmove) == forward && std::trunc(cmd.sidemove) == side
             && std::trunc(cmd.upmove) == up;
    };

    const auto before = input;
    auto cmd = scs_usercmd{};
    scs_build_usercmd(&input, clientMaxSpeed, msec, &cmd);
    ++result.frames;
    result.pressFrames += pressed ? 1 : 0;
    if (matches(cmd))
    {
      continue;
    }
    if (matchesTruncated(cmd))
    {
      ++result.truncated;
      continue;
    }

    auto again = before;
    auto other = scs_usercmd{};
    const auto speed = std::sqrt(forward * forward + side * side + up * up);
    scs_build_usercmd(&again, speed, msec, &other);
    if (speed > 0.0f && matches(other))
    {
      ++result.otherSpeed;
      continue;
    }

    ++result.differences;
    if (result.examples.size() < 10)
    {
      char text[256];
      std::snprintf(
        text,
        sizeof(text),
        "frame %ld%s, buttons %d: demo (%g %g %g), TbCsLib (%g %g %g)",
        result.frames,
        pressed ? " (key press)" : "",
        buttons,
        double(forward),
        double(side),
        double(up),
        double(cmd.forwardmove),
        double(cmd.sidemove),
        double(cmd.upmove));
      result.examples.emplace_back(text);
    }
  }
  return result;
}

} // namespace tb::cs::groundtruth
