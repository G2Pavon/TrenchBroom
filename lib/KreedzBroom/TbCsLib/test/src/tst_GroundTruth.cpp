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
#include "cs/scs.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace tb::cs::groundtruth
{
namespace
{

const auto FixtureDir = std::string{"fixture/test/ground-truth/"};

// CBasePlayer::WaterMove's drowning bookkeeping; movement never reads it and TbCsLib
// does not track it.
constexpr int FlInWater = 1 << 4;

// A command whose state the player would notice: a different position (beyond float
// noise), ground or duck state, or water level.
bool noticeablyDifferent(const Record& a, const Record& b)
{
  const auto dx = double(a.origin[0]) - double(b.origin[0]);
  const auto dy = double(a.origin[1]) - double(b.origin[1]);
  const auto dz = double(a.origin[2]) - double(b.origin[2]);
  return std::sqrt(dx * dx + dy * dy + dz * dz) > 0.01
         || ((a.flags ^ b.flags) & ~FlInWater) != 0 || a.inDuck != b.inDuck
         || a.waterLevel != b.waterLevel;
}

} // namespace

TEST_CASE("ground truth")
{
  // tbt_basic_10ms.truth was recorded on HLDS with the Steam client's cs.so (see
  // fixture/ground-truth/README.md): 16 scenarios at fps_max 100.
  auto bspFile = std::ifstream{FixtureDir + "tbt_basic.bsp", std::ios::binary};
  const auto bsp = std::vector<char>{
    std::istreambuf_iterator<char>{bspFile}, std::istreambuf_iterator<char>{}};
  scs_world* world = nullptr;
  REQUIRE(scs_world_load_bsp(bsp.data(), bsp.size(), &world) == SCS_OK);

  const auto config = recorderConfig();
  auto* ctx = scs_init(&config);
  scs_set_world(ctx, world);
  scs_world_release(world);

  auto script = std::ifstream{FixtureDir + "tbt_basic_10ms.txt"};
  auto truthLog = std::ifstream{FixtureDir + "tbt_basic_10ms.truth"};
  const auto replayed = replay(*ctx, script);
  const auto truth = readLog(truthLog);
  scs_shutdown(ctx);

  REQUIRE(replayed.size() == 16);
  REQUIRE(truth.size() == replayed.size());
  for (std::size_t i = 0; i < truth.size(); ++i)
  {
    INFO(replayed[i].name);
    REQUIRE(replayed[i].records.size() == truth[i].records.size());

    auto differences = 0;
    for (std::size_t j = 0; j < truth[i].records.size(); ++j)
    {
      if (noticeablyDifferent(truth[i].records[j], replayed[i].records[j]))
      {
        ++differences;
      }
    }

#ifdef TB_CS_X87_MATH
    CHECK(differences == 0);
#else
    // SSE rounds differently from the game's x87 math, which here finishes a duck
    // one command early.
    CHECK(differences <= 1);
#endif
  }
}

TEST_CASE("client input matches demos")
{
  // The user commands of a demo of the current Steam client at fps_max 99.5 (see
  // fixture/ground-truth/README.md): replaying its key presses must build exactly
  // the commands the game built.
  auto commands = std::ifstream{FixtureDir + "steam_99.5fps.cmds"};
  const auto result = checkClientInput(commands);
  CHECK(result.frames == 4672);
  CHECK(result.pressFrames == 33);
  CHECK(result.differences == 0);
  CHECK(result.truncated == 0);
  CHECK(result.otherSpeed == 0);
}

} // namespace tb::cs::groundtruth
