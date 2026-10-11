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

#include <deque>
#include <optional>

namespace tb::kzb
{

/**
 * Measures how often something happens, e.g. frames drawn or commands simulated, over
 * the last window of time. Each event can carry a value, such as a command's msec.
 */
class PlayModeRateMeter
{
private:
  struct Sample
  {
    double timeMs = 0.0;
    double value = 0.0;
  };

  double m_windowMs;
  std::deque<Sample> m_samples;

public:
  explicit PlayModeRateMeter(double windowMs = 1000.0);

  void reset();
  void add(double timeMs, double value = 0.0);

  /** Events per second, from the first to the last event in the window. */
  std::optional<double> perSecond() const;

  /** The longest time between two events in the window. */
  std::optional<double> maxIntervalMs() const;

  /** The mean of the values of the events in the window. */
  std::optional<double> meanValue() const;
};

} // namespace tb::kzb
