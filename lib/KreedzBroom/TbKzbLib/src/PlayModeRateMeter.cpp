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

#include <algorithm>

namespace tb::kzb
{

PlayModeRateMeter::PlayModeRateMeter(const double windowMs)
  : m_windowMs{windowMs}
{
}

void PlayModeRateMeter::reset()
{
  m_samples.clear();
}

void PlayModeRateMeter::add(const double timeMs, const double value)
{
  m_samples.push_back({timeMs, value});
  while (m_samples.front().timeMs < timeMs - m_windowMs)
  {
    m_samples.pop_front();
  }
}

std::optional<double> PlayModeRateMeter::perSecond() const
{
  if (m_samples.size() < 2)
  {
    return std::nullopt;
  }
  const auto spanMs = m_samples.back().timeMs - m_samples.front().timeMs;
  if (spanMs <= 0.0)
  {
    return std::nullopt;
  }
  return static_cast<double>(m_samples.size() - 1) * 1000.0 / spanMs;
}

std::optional<double> PlayModeRateMeter::maxIntervalMs() const
{
  if (m_samples.size() < 2)
  {
    return std::nullopt;
  }
  auto maxInterval = 0.0;
  for (auto i = size_t(1); i < m_samples.size(); ++i)
  {
    maxInterval = std::max(maxInterval, m_samples[i].timeMs - m_samples[i - 1].timeMs);
  }
  return maxInterval;
}

std::optional<double> PlayModeRateMeter::meanValue() const
{
  if (m_samples.empty())
  {
    return std::nullopt;
  }
  auto sum = 0.0;
  for (const auto& sample : m_samples)
  {
    sum += sample.value;
  }
  return sum / static_cast<double>(m_samples.size());
}

} // namespace tb::kzb
