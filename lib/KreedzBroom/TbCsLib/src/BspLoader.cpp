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

#include "BspLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tb::cs
{
namespace
{

constexpr std::int32_t kBspVersion = 30;

enum Lump
{
  LumpEntities = 0,
  LumpPlanes = 1,
  LumpNodes = 5,
  LumpClipnodes = 9,
  LumpLeafs = 10,
  LumpModels = 14,
  LumpCount = 15,
};

// On-disk structures (bspfile.h), read field by field to avoid alignment issues.
constexpr std::size_t kPlaneSize = 20; // float normal[3], dist; int type
constexpr std::size_t kNodeSize =
  24; // int planenum; short children[2], mins[3], maxs[3]; ushort face, count
constexpr std::size_t kClipnodeSize = 8; // int planenum; short children[2]
constexpr std::size_t kLeafSize = 28; // int contents, visofs; short mins[3], maxs[3]; ...
constexpr std::size_t kModelSize =
  64; // float mins[3], maxs[3], origin[3]; int headnode[4], visleafs, face, count

// Hull sizes the engine assigns in Mod_LoadClipnodes / Mod_MakeHull0.
constexpr float kHullMins[kNumHulls][3] = {
  {0, 0, 0}, {-16, -16, -36}, {-32, -32, -32}, {-16, -16, -18}};
constexpr float kHullMaxs[kNumHulls][3] = {
  {0, 0, 0}, {16, 16, 36}, {32, 32, 32}, {16, 16, 18}};

class Reader
{
public:
  Reader(const void* data, std::size_t size)
    : m_data{static_cast<const unsigned char*>(data)}
    , m_size{size}
  {
  }

  template <typename T>
  T read(std::size_t offset) const
  {
    if (offset > m_size || sizeof(T) > m_size - offset)
    {
      throw BspLoadError{"truncated .bsp"};
    }
    T value;
    std::memcpy(&value, m_data + offset, sizeof(T));
    return value;
  }

  // A lump's offset and element count, checked against the file size.
  std::pair<std::size_t, std::size_t> lump(int index, std::size_t elementSize) const
  {
    const auto offset =
      static_cast<std::size_t>(read<std::int32_t>(4 + 8 * std::size_t(index)));
    const auto length =
      static_cast<std::size_t>(read<std::int32_t>(4 + 8 * std::size_t(index) + 4));
    if (offset > m_size || length > m_size - offset || length % elementSize != 0)
    {
      throw BspLoadError{"bad .bsp lump"};
    }
    return {offset, length / elementSize};
  }

  std::string_view text(std::size_t offset, std::size_t length) const
  {
    return {reinterpret_cast<const char*>(m_data + offset), length};
  }

private:
  const unsigned char* m_data;
  std::size_t m_size;
};

std::vector<ClipPlane> loadPlanes(const Reader& bsp)
{
  const auto [offset, count] = bsp.lump(LumpPlanes, kPlaneSize);
  auto planes = std::vector<ClipPlane>(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const auto base = offset + i * kPlaneSize;
    auto& plane = planes[i];
    plane.signbits = 0;
    for (int j = 0; j < 3; ++j)
    {
      plane.normal[j] = bsp.read<float>(base + 4 * std::size_t(j));
      if (plane.normal[j] < 0.0f)
      {
        plane.signbits = static_cast<std::uint8_t>(plane.signbits | (1 << j));
      }
    }
    plane.dist = bsp.read<float>(base + 12);
    plane.type = static_cast<std::uint8_t>(bsp.read<std::int32_t>(base + 16));
  }
  return planes;
}

// Hulls 1-3 share the clipnodes lump.
std::vector<ClipNode> loadClipnodes(const Reader& bsp)
{
  const auto [offset, count] = bsp.lump(LumpClipnodes, kClipnodeSize);
  auto nodes = std::vector<ClipNode>(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const auto base = offset + i * kClipnodeSize;
    nodes[i].planenum = bsp.read<std::int32_t>(base);
    nodes[i].children[0] = bsp.read<std::int16_t>(base + 4);
    nodes[i].children[1] = bsp.read<std::int16_t>(base + 6);
  }
  return nodes;
}

// Mod_MakeHull0: hull 0 is the drawing BSP tree with the leafs replaced by their
// contents.
std::vector<ClipNode> makeHull0(const Reader& bsp)
{
  const auto [leafOffset, leafCount] = bsp.lump(LumpLeafs, kLeafSize);
  const auto [offset, count] = bsp.lump(LumpNodes, kNodeSize);
  auto nodes = std::vector<ClipNode>(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const auto base = offset + i * kNodeSize;
    nodes[i].planenum = bsp.read<std::int32_t>(base);
    for (int j = 0; j < 2; ++j)
    {
      const auto child = bsp.read<std::int16_t>(base + 4 + 2 * std::size_t(j));
      if (child >= 0)
      {
        nodes[i].children[j] = child;
      }
      else
      {
        const auto leaf = static_cast<std::size_t>(-1 - child);
        if (leaf >= leafCount)
        {
          throw BspLoadError{"bad .bsp leaf index"};
        }
        nodes[i].children[j] = bsp.read<std::int32_t>(leafOffset + leaf * kLeafSize);
      }
    }
  }
  return nodes;
}

using EntityProperties = std::map<std::string, std::string>;

// Parses the entity lump: { "key" "value" ... } blocks.
std::vector<EntityProperties> loadEntities(const Reader& bsp)
{
  const auto [offset, length] = bsp.lump(LumpEntities, 1);
  const auto text = bsp.text(offset, length);

  auto entities = std::vector<EntityProperties>{};
  auto pos = std::size_t{0};
  const auto quoted = [&]() -> std::string {
    const auto end = text.find('"', pos + 1);
    if (end == std::string_view::npos)
    {
      throw BspLoadError{"bad .bsp entity lump"};
    }
    auto value = std::string{text.substr(pos + 1, end - pos - 1)};
    pos = end + 1;
    return value;
  };

  while ((pos = text.find_first_of("{\"}", pos)) != std::string_view::npos)
  {
    if (text[pos] == '{')
    {
      entities.emplace_back();
      ++pos;
    }
    else if (text[pos] == '}')
    {
      ++pos;
    }
    else
    {
      auto key = quoted();
      pos = text.find('"', pos);
      if (pos == std::string_view::npos || entities.empty())
      {
        throw BspLoadError{"bad .bsp entity lump"};
      }
      entities.back()[std::move(key)] = quoted();
    }
  }
  return entities;
}

std::string toLower(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

// The solid / skin the game gives a brush entity (see TbUiLib's PlayModeWorld).
void applyEntity(BrushModel& model, const EntityProperties& entity)
{
  const auto classname = toLower(entity.count("classname") ? entity.at("classname") : "");
  if (classname == "func_ladder")
  {
    model.solid = 0; // SOLID_NOT
    model.skin = kContentsLadder;
  }
  else if (classname == "func_water")
  {
    const auto skin = entity.count("skin") ? std::atoi(entity.at("skin").c_str()) : 0;
    model.solid = 0;
    model.skin = skin == kContentsWater || skin == kContentsSlime || skin == kContentsLava
                   ? skin
                   : kContentsWater;
  }
  else if (
    classname.starts_with("trigger_") || classname == "func_illusionary"
    || classname == "func_buyzone" || classname == "func_bomb_target"
    || classname == "func_hostage_rescue" || classname == "func_escapezone"
    || classname == "func_vip_safetyzone" || classname == "func_weaponcheck"
    || classname == "env_bubbles")
  {
    model.solid = 0;
    model.skin = 0; // not a physent
  }
  else
  {
    model.solid = 4; // SOLID_BSP
    model.skin = 0;
  }
}

} // namespace

std::shared_ptr<World> loadBsp(const void* data, std::size_t size)
{
  const auto bsp = Reader{data, size};
  if (bsp.read<std::int32_t>(0) != kBspVersion)
  {
    throw BspLoadError{"not a GoldSrc (version 30) .bsp"};
  }

  auto planes = loadPlanes(bsp);
  auto clipnodes = loadClipnodes(bsp);
  auto hull0 = makeHull0(bsp);
  const auto entities = loadEntities(bsp);
  const auto [modelOffset, modelCount] = bsp.lump(LumpModels, kModelSize);
  if (modelCount == 0)
  {
    throw BspLoadError{".bsp without models"};
  }

  const auto checkTree = [&](const std::vector<ClipNode>& nodes) {
    for (const auto& node : nodes)
    {
      if (
        node.planenum < 0 || static_cast<std::size_t>(node.planenum) >= planes.size()
        || node.children[0] >= static_cast<std::int32_t>(nodes.size())
        || node.children[1] >= static_cast<std::int32_t>(nodes.size()))
      {
        throw BspLoadError{"bad .bsp node"};
      }
    }
  };
  checkTree(clipnodes);
  checkTree(hull0);

  auto world = std::make_shared<World>();
  for (std::size_t i = 0; i < modelCount; ++i)
  {
    auto model = std::make_unique<BrushModel>();
    const auto base = modelOffset + i * kModelSize;
    for (int a = 0; a < 3; ++a)
    {
      model->mins[a] = bsp.read<float>(base + 4 * std::size_t(a));
      model->maxs[a] = bsp.read<float>(base + 12 + 4 * std::size_t(a));
    }
    if (i == 0)
    {
      // The world model owns the shared arrays; submodels point into them.
      model->planes[0] = planes;
      model->clipnodes[0] = std::move(hull0);
      for (int h = 1; h < kNumHulls; ++h)
      {
        model->planes[std::size_t(h)] = planes;
        model->clipnodes[std::size_t(h)] = clipnodes;
      }
    }
    const auto& owner = i == 0 ? *model : *world->models.front();
    for (int h = 0; h < kNumHulls; ++h)
    {
      const auto& nodes = owner.clipnodes[std::size_t(h)];
      Hull& hull = model->hulls[std::size_t(h)];
      hull.clipnodes = const_cast<ClipNode*>(nodes.data());
      hull.planes = const_cast<ClipPlane*>(owner.planes[std::size_t(h)].data());
      hull.firstclipnode = bsp.read<std::int32_t>(base + 36 + 4 * std::size_t(h));
      hull.lastclipnode = static_cast<int>(nodes.size()) - 1;
      if (hull.firstclipnode < 0 || hull.firstclipnode > std::max(hull.lastclipnode, 0))
      {
        throw BspLoadError{"bad .bsp model headnode"};
      }
      std::copy(std::begin(kHullMins[h]), std::end(kHullMins[h]), hull.clip_mins);
      std::copy(std::begin(kHullMaxs[h]), std::end(kHullMaxs[h]), hull.clip_maxs);
    }
    model->name = i == 0 ? "world" : "*" + std::to_string(i);
    world->models.push_back(std::move(model));
  }

  for (const auto& entity : entities)
  {
    const auto it = entity.find("model");
    if (it == entity.end() || it->second.size() < 2 || it->second[0] != '*')
    {
      continue;
    }
    const auto index = static_cast<std::size_t>(std::atoi(it->second.c_str() + 1));
    if (index > 0 && index < world->models.size())
    {
      applyEntity(*world->models[index], entity);
    }
  }
  return world;
}

} // namespace tb::cs
