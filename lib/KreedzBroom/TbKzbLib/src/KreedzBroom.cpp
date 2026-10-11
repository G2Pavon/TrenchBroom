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

#include "kzb/KreedzBroom.h"

#include <QSurfaceFormat>

#include "kzb/KreedzBroomPreferencePane.h"
#include "kzb/PlayMode.h"
#include "ui/MapView3D.h"
#include "ui/PreferenceDialog.h"

#include <memory>

namespace tb::kzb
{

void initialize()
{
  // Do not wait for vsync: play mode draws like the game, one frame per command at
  // fps_max (100 fps on a 60 Hz display), each showing the command just simulated.
  // Views that draw continuously pace themselves (see MapView3D::updateFlyMode).
  auto format = QSurfaceFormat::defaultFormat();
  format.setSwapInterval(0);
  QSurfaceFormat::setDefaultFormat(format);

  ui::MapView3D::setPlayModeFactory(
    [](auto& view, auto& document, auto& camera)
      -> std::unique_ptr<ui::MapView3DPlayMode> {
      return std::make_unique<PlayMode>(view, document, camera);
    });

  ui::PreferenceDialog::addPane(
    "KreedzBroomPreferences.svg", "KreedzBroom", []() -> ui::PreferencePane* {
      return new KreedzBroomPreferencePane{};
    });
}

} // namespace tb::kzb
