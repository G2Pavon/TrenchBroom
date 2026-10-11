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

#include "World.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace tb::cs
{

struct BspLoadError : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

// Loads the clip hulls of a compiled GoldSrc .bsp (version 30) the way the engine
// does (ReHLDS Mod_LoadBrushModel): hull 0 from the BSP nodes and leafs, hulls 1-3
// from the clipnodes, one BrushModel per BSP model. Brush entities get the solid and
// skin the game gives them: func_ladder and func_water carry their contents, triggers
// and func_illusionary are not solid. Throws BspLoadError on malformed data.
std::shared_ptr<World> loadBsp(const void* data, std::size_t size);

} // namespace tb::cs
