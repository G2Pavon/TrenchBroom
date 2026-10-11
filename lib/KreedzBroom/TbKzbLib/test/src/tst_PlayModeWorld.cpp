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

#include "cs/scs.h"
#include "fs/TestEnvironment.h"
#include "kzb/PlayModeWorld.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushNode.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityProperties.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_NodeVisibility.h"
#include "mdl/Map_Nodes.h"
#include "mdl/WorldNode.h"
#include "ui/CatchConfig.h"

#include "kd/result.h"

#include "vm/approx.h"
#include "vm/bbox.h"
#include "vm/vec.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <variant>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::kzb
{
namespace
{

struct ContextDeleter
{
  void operator()(scs_context* context) const { scs_shutdown(context); }
};

using ContextPtr = std::unique_ptr<scs_context, ContextDeleter>;

ContextPtr createContext(const PlayModeWorld& world)
{
  auto context = ContextPtr{scs_init(nullptr)};
  scs_set_world(context.get(), world.world.get());
  return context;
}

mdl::BrushNode* createCuboid(
  const mdl::Map& map, const vm::bbox3d& bounds, const std::string& material)
{
  const auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
  return new mdl::BrushNode{builder.createCuboid(bounds, material) | kdl::value()};
}

mdl::EntityNode* createBrushEntity(
  mdl::Map& map, const std::string& classname, const vm::bbox3d& bounds)
{
  auto* entityNode =
    new mdl::EntityNode{mdl::Entity{{{mdl::EntityPropertyKeys::Classname, classname}}}};
  mdl::addNodes(map, {{&mdl::parentForNodes(map), {entityNode}}});
  mdl::addNodes(map, {{entityNode, {createCuboid(map, bounds, "wall")}}});
  return entityNode;
}

// Sweeps the standing hull along +x at walking height and returns where it stops.
float blockedAtX(scs_context* context, const float toX)
{
  const float start[3] = {0, 0, 37};
  const float end[3] = {toX, 0, 37};
  auto trace = scs_trace{};
  scs_trace_hull(context, start, end, SCS_HULL_STANDING, &trace);
  return trace.endpos[0];
}

} // namespace

TEST_CASE("buildPlayModeWorld")
{
  auto fixture = mdl::MapFixture{};
  auto& map = fixture.create();

  // A floor whose top is at z = 0.
  mdl::addNodes(
    map,
    {{&mdl::parentForNodes(map),
      {createCuboid(map, vm::bbox3d{{-512, -512, -16}, {512, 512, 0}}, "floor")}}});

  SECTION("the player stands on world brushes")
  {
    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    CHECK(world.worldBrushCount == 1u);
    CHECK(world.entityCount == 0u);

    auto context = createContext(world);
    const float spawn[3] = {0, 0, 40};
    scs_spawn(context.get(), spawn, nullptr);
    auto cmd = scs_usercmd{};
    cmd.msec = 10;
    for (int i = 0; i < 50; ++i)
    {
      scs_step(context.get(), &cmd);
    }

    auto state = scs_player_state{};
    scs_get_state(context.get(), &state);
    CHECK((state.flags & SCS_FL_ONGROUND) != 0);
    CHECK(state.origin[2] == vm::approx{36.03125f, 0.001f});
  }

  SECTION("CLIP blocks the player but not hull 0, ORIGIN and HINT brushes vanish")
  {
    auto* clip = createCuboid(map, vm::bbox3d{{100, -64, 0}, {116, 64, 128}}, "CLIP");
    auto* origin = createCuboid(map, vm::bbox3d{{-200, -8, 0}, {-184, 8, 16}}, "ORIGIN");
    auto* hint = createCuboid(map, vm::bbox3d{{200, -64, 0}, {216, 64, 128}}, "HINT");
    mdl::addNodes(map, {{&mdl::parentForNodes(map), {clip, origin, hint}}});

    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    CHECK(world.worldBrushCount == 2u);
    CHECK(world.skippedBrushCount == 2u);

    auto context = createContext(world);
    CHECK(
      blockedAtX(context.get(), 300.0f) == vm::approx{100.0f - 16.0f - 0.03125f, 0.01f});

    const float insideClip[3] = {108, 0, 64};
    CHECK(scs_point_contents(context.get(), insideClip) == SCS_CONTENTS_EMPTY);
  }

  SECTION("NOCLIP and CLIPHULLn decide which hulls a brush is in")
  {
    auto* noclip = createCuboid(map, vm::bbox3d{{100, -64, 0}, {116, 64, 128}}, "NOCLIP");
    auto* duckClip =
      createCuboid(map, vm::bbox3d{{150, -64, 0}, {166, 64, 128}}, "CLIPHULL3");
    mdl::addNodes(map, {{&mdl::parentForNodes(map), {noclip, duckClip}}});

    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    auto context = createContext(world);

    // standing walks through both, only the ducked hull meets CLIPHULL3
    CHECK(blockedAtX(context.get(), 300.0f) == vm::approx{300.0f});
    const float inNoclip[3] = {108, 0, 64};
    CHECK(scs_point_contents(context.get(), inNoclip) == SCS_CONTENTS_SOLID);
    const float nearDuckClip[3] = {140, 0, 37};
    CHECK(scs_position_is_free(context.get(), nearDuckClip, SCS_HULL_STANDING) != 0);
    CHECK(scs_position_is_free(context.get(), nearDuckClip, SCS_HULL_DUCKED) == 0);

    // ZHLT has neither: NOCLIP is a plain solid texture, CLIPHULL3 a plain CLIP
    const auto zhltWorld = buildPlayModeWorld(map, {.compiler = SCS_COMPILER_ZHLT});
    REQUIRE(zhltWorld.world);
    CHECK(
      blockedAtX(createContext(zhltWorld).get(), 300.0f)
      == vm::approx{100.0f - 16.0f - 0.03125f, 0.01f});
  }

  SECTION("cliptype comes from the options")
  {
    const auto world = buildPlayModeWorld(map, {.clipType = SCS_CLIPTYPE_SIMPLE});
    CHECK(world.clipType == SCS_CLIPTYPE_SIMPLE);
  }

  SECTION("liquid materials become water contents")
  {
    mdl::addNodes(
      map,
      {{&mdl::parentForNodes(map),
        {createCuboid(map, vm::bbox3d{{-64, -64, 0}, {64, 64, 64}}, "!waterblue")}}});

    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);

    auto context = createContext(world);
    const float inside[3] = {0, 0, 32};
    CHECK(scs_point_contents(context.get(), inside) == SCS_CONTENTS_WATER);
    CHECK(blockedAtX(context.get(), 300.0f) == vm::approx{300.0f});
  }

  SECTION("triggers are not solid, func_wall is")
  {
    createBrushEntity(map, "trigger_multiple", vm::bbox3d{{100, -64, 0}, {116, 64, 128}});

    auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    CHECK(world.entityCount == 0u);
    CHECK(blockedAtX(createContext(world).get(), 300.0f) == vm::approx{300.0f});

    createBrushEntity(map, "func_wall", vm::bbox3d{{150, -64, 0}, {166, 64, 128}});
    world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    CHECK(world.entityCount == 1u);
    CHECK(
      blockedAtX(createContext(world).get(), 300.0f)
      == vm::approx{150.0f - 16.0f - 0.03125f, 0.01f});
  }

  SECTION("func_ladder can be climbed but does not block")
  {
    createBrushEntity(map, "func_ladder", vm::bbox3d{{100, -32, 0}, {108, 32, 512}});

    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    CHECK(world.entityCount == 1u);

    auto context = createContext(world);
    const float spawn[3] = {60, 0, 36};
    scs_spawn(context.get(), spawn, nullptr);

    auto cmd = scs_usercmd{};
    cmd.msec = 10;
    cmd.viewangles[0] = -60.0f; // look up
    cmd.forwardmove = 250.0f;
    cmd.buttons = SCS_IN_FORWARD;
    for (int i = 0; i < 150; ++i)
    {
      scs_step(context.get(), &cmd);
    }

    auto state = scs_player_state{};
    scs_get_state(context.get(), &state);
    CHECK(state.movetype == SCS_MOVETYPE_FLY);
    CHECK(state.origin[2] > 200.0f);
  }

  SECTION("hidden brushes are ignored")
  {
    auto* wall = createCuboid(map, vm::bbox3d{{100, -64, 0}, {116, 64, 128}}, "wall");
    mdl::addNodes(map, {{&mdl::parentForNodes(map), {wall}}});
    mdl::hideNodes(map, {wall});

    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.world);
    CHECK(world.worldBrushCount == 1u);
    CHECK(blockedAtX(createContext(world).get(), 300.0f) == vm::approx{300.0f});
  }

  SECTION("spawn points are collected")
  {
    auto* spawn = new mdl::EntityNode{mdl::Entity{{
      {mdl::EntityPropertyKeys::Classname, "info_player_start"},
      {mdl::EntityPropertyKeys::Origin, "10 20 30"},
      {"angle", "90"},
    }}};
    mdl::addNodes(map, {{&mdl::parentForNodes(map), {spawn}}});

    const auto world = buildPlayModeWorld(map);
    REQUIRE(world.spawnPoints.size() == 1u);
    CHECK(world.spawnPoints[0].classname == "info_player_start");
    CHECK(world.spawnPoints[0].origin == vm::vec3f{10, 20, 30});
    CHECK(world.spawnPoints[0].angles == vm::vec3f{0, 90, 0});
  }
}

TEST_CASE("loadCompiledPlayModeWorld")
{
  // A saved map with its .bsp next to it.
  const auto fixtureDir =
    std::filesystem::current_path() / "fixture/test/kzb/PlayModeWorld";
  auto env = fs::TestEnvironment{};
  const auto mapPath = env.dir() / "tbt_basic.map";
  const auto bspPath = env.dir() / "tbt_basic.bsp";
  std::filesystem::copy_file(fixtureDir / "tbt_basic.map", mapPath);
  std::filesystem::copy_file(fixtureDir / "tbt_basic.bsp", bspPath);
  const auto mapTime = std::filesystem::last_write_time(mapPath);

  CHECK(compiledBspPath(mapPath) == bspPath);

  auto fixture = mdl::MapFixture{};

  SECTION("loads a .bsp that is newer than the map")
  {
    std::filesystem::last_write_time(bspPath, mapTime + std::chrono::seconds{10});
    auto& map = fixture.load(mapPath, mdl::QuakeFixtureConfig);

    const auto result = loadCompiledPlayModeWorld(map);
    REQUIRE(std::holds_alternative<ScsWorldPtr>(result));

    auto* context = scs_init(nullptr);
    scs_set_world(context, std::get<ScsWorldPtr>(result).get());
    const float above[3] = {0, 0, 100};
    const float below[3] = {0, 0, -100};
    auto trace = scs_trace{};
    scs_trace_hull(context, above, below, SCS_HULL_STANDING, &trace);
    CHECK(trace.endpos[2] == vm::approx{36.03125f});
    scs_shutdown(context);
  }

  SECTION("not a .bsp that is older than the map")
  {
    std::filesystem::last_write_time(bspPath, mapTime - std::chrono::seconds{10});
    auto& map = fixture.load(mapPath, mdl::QuakeFixtureConfig);
    CHECK(
      std::get<std::string>(loadCompiledPlayModeWorld(map))
      == "tbt_basic.bsp is older than the map");
  }

  SECTION("not while the map has unsaved changes")
  {
    std::filesystem::last_write_time(bspPath, mapTime + std::chrono::seconds{10});
    auto& map = fixture.load(mapPath, mdl::QuakeFixtureConfig);
    createBrushEntity(map, "func_wall", vm::bbox3d{{0, 0, 0}, {64, 64, 64}});
    CHECK(
      std::get<std::string>(loadCompiledPlayModeWorld(map))
      == "the map has unsaved changes");
  }

  SECTION("not without a .bsp")
  {
    std::filesystem::remove(bspPath);
    auto& map = fixture.load(mapPath, mdl::QuakeFixtureConfig);
    CHECK(
      std::get<std::string>(loadCompiledPlayModeWorld(map))
      == "tbt_basic.bsp does not exist");
  }

  SECTION("not for a map that was never saved")
  {
    auto& map = fixture.create(mdl::QuakeFixtureConfig);
    CHECK(
      std::get<std::string>(loadCompiledPlayModeWorld(map))
      == "the map has not been saved");
  }
}

TEST_CASE("buildPlayModeWorld matches compiled clip hulls")
{
  // tbt_hulls has slopes, rotated
  // boxes, cylinders, pyramids, thin plates and small gaps, compiled with ZHLT's
  // default cliptype (legacy, what most CS 1.6 maps use) and VHLT's (simple). Around
  // where the compiled hulls stop random traces, play mode's hulls must agree on what
  // is solid. Exactly where a trace stops can differ by more: it depends on the
  // compiler's BSP tree, which play mode does not reproduce.
  using T = std::tuple<std::string, int, int>;
  const auto [bspName, compiler, clipType] = GENERATE(
    values<T>({
      {"tbt_hulls_zhlt_legacy.bsp", SCS_COMPILER_ZHLT, SCS_CLIPTYPE_LEGACY},
      {"tbt_hulls_vhlt_simple.bsp", SCS_COMPILER_VHLT, SCS_CLIPTYPE_SIMPLE},
    }));
  CAPTURE(bspName);

  const auto fixtureDir =
    std::filesystem::current_path() / "fixture/test/kzb/PlayModeWorld";
  auto fixture = mdl::MapFixture{};
  auto& map = fixture.load(fixtureDir / "tbt_hulls.map", mdl::QuakeFixtureConfig);
  const auto world =
    buildPlayModeWorld(map, {.compiler = compiler, .clipType = clipType});
  REQUIRE(world.world);

  auto file = std::ifstream{fixtureDir / bspName, std::ios::binary};
  const auto bsp = std::vector<char>{
    std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  scs_world* compiled = nullptr;
  REQUIRE(scs_world_load_bsp(bsp.data(), bsp.size(), &compiled) == SCS_OK);

  auto* playContext = scs_init(nullptr);
  auto* compiledContext = scs_init(nullptr);
  scs_set_world(playContext, world.world.get());
  scs_set_world(compiledContext, compiled);
  scs_world_release(compiled);
  // Traces offset the hulls by the player's bounding boxes, which spawning sets up.
  const float origin[3] = {0, 0, 0};
  scs_spawn(playContext, origin, nullptr);
  scs_spawn(compiledContext, origin, nullptr);

  for (const auto hull : {SCS_HULL_STANDING, SCS_HULL_DUCKED})
  {
    CAPTURE(hull);
    auto rng = std::mt19937{1};
    auto coordinate = std::uniform_real_distribution<float>{-2040.0f, 2040.0f};
    auto height = std::uniform_real_distribution<float>{0.0f, 1020.0f};

    // Free in only one world at 1 / 0.05 units inside and outside the compiled surface.
    auto farFromSurface = 0;
    auto nearSurface = 0;
    for (int i = 0; i < 20000; ++i)
    {
      const float start[3] = {coordinate(rng), coordinate(rng), height(rng)};
      const float end[3] = {start[0], start[1], -16.0f};
      auto trace = scs_trace{};
      scs_trace_hull(compiledContext, start, end, hull, &trace);
      if (trace.startsolid || trace.fraction == 1.0f)
      {
        continue;
      }
      for (const auto offset : {-1.0f, -0.05f, 0.05f, 1.0f})
      {
        const float point[3] = {
          trace.endpos[0] + trace.plane_normal[0] * offset,
          trace.endpos[1] + trace.plane_normal[1] * offset,
          trace.endpos[2] + trace.plane_normal[2] * offset};
        if (
          scs_position_is_free(playContext, point, hull)
          != scs_position_is_free(compiledContext, point, hull))
        {
          ++(std::abs(offset) == 1.0f ? farFromSurface : nearSurface);
        }
      }
    }
    CHECK(farFromSurface == 0);
    CHECK(nearSurface <= 8);
  }

  scs_shutdown(playContext);
  scs_shutdown(compiledContext);
}

} // namespace tb::kzb
