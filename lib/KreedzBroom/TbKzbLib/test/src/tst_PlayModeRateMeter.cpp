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

#include "kzb/PlayModeRateMeter.h"
#include "ui/CatchConfig.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace tb::kzb
{

TEST_CASE("PlayModeRateMeter")
{
  auto meter = PlayModeRateMeter{1000.0};

  SECTION("nothing is measured before two events")
  {
    CHECK(meter.perSecond() == std::nullopt);
    CHECK(meter.maxIntervalMs() == std::nullopt);
    CHECK(meter.meanValue() == std::nullopt);

    meter.add(0.0, 10.0);
    CHECK(meter.perSecond() == std::nullopt);
    CHECK(meter.maxIntervalMs() == std::nullopt);
    CHECK(meter.meanValue() == 10.0);
  }

  SECTION("perSecond")
  {
    // fps_max 99.5 on the Steam client: frames of 1 / 100 s
    for (auto i = 0; i <= 300; ++i)
    {
      meter.add(i * 10.0);
    }
    CHECK(*meter.perSecond() == Catch::Approx(100.0));

    // fps_max 99.5 on older clients: frames of 1 / 99.5 s
    meter.reset();
    for (auto i = 0; i <= 300; ++i)
    {
      meter.add(i * 1000.0 / 99.5);
    }
    CHECK(*meter.perSecond() == Catch::Approx(99.5));
  }

  SECTION("only the events in the window count")
  {
    // A slow second, then a fast one
    for (auto i = 0; i <= 50; ++i)
    {
      meter.add(i * 20.0);
    }
    for (auto i = 1; i <= 100; ++i)
    {
      meter.add(1000.0 + i * 10.0);
    }
    CHECK(*meter.perSecond() == Catch::Approx(100.0));
    CHECK(*meter.maxIntervalMs() == Catch::Approx(10.0));
  }

  SECTION("maxIntervalMs")
  {
    meter.add(0.0);
    meter.add(10.0);
    meter.add(35.0);
    meter.add(45.0);
    CHECK(*meter.maxIntervalMs() == Catch::Approx(25.0));
  }

  SECTION("meanValue")
  {
    // At fps_max 100 on the Steam client, one command in 20 is 9 ms.
    for (auto i = 0; i < 20; ++i)
    {
      meter.add(i * 1000.0 / 100.5, i == 19 ? 9.0 : 10.0);
    }
    CHECK(*meter.meanValue() == Catch::Approx(9.95));
  }
}

} // namespace tb::kzb
