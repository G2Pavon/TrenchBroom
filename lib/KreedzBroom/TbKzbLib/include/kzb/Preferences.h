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

#include <Qt>

#include "base/Preference.h"

// KreedzBroom's play mode settings (embedded Counter-Strike 1.6 movement). They share
// TrenchBroom's preference namespace. The defaults are the CS 1.6 client and server
// defaults.
namespace tb::Preferences
{

inline auto PlayModeMouseSensitivity =
  Preference<float>{"Controls/Play mode/Mouse sensitivity", 2.5f};
inline auto PlayModeRawInput =
  Preference<bool>{"Controls/Play mode/Raw mouse input", true};

// Keys are single Qt key codes so that modifiers (Ctrl to duck) can be bound.
inline auto PlayModeKeyForward =
  Preference<int>{"Controls/Play mode/Key forward", Qt::Key_W};
inline auto PlayModeKeyBack = Preference<int>{"Controls/Play mode/Key back", Qt::Key_S};
inline auto PlayModeKeyMoveLeft =
  Preference<int>{"Controls/Play mode/Key move left", Qt::Key_A};
inline auto PlayModeKeyMoveRight =
  Preference<int>{"Controls/Play mode/Key move right", Qt::Key_D};
inline auto PlayModeKeyJump =
  Preference<int>{"Controls/Play mode/Key jump", Qt::Key_Space};
inline auto PlayModeKeyDuck =
  Preference<int>{"Controls/Play mode/Key duck", Qt::Key_Control};
inline auto PlayModeKeyWalk =
  Preference<int>{"Controls/Play mode/Key walk", Qt::Key_Shift};
inline auto PlayModeKeyUse = Preference<int>{"Controls/Play mode/Key use", Qt::Key_E};
inline auto PlayModeKeyTurnLeft =
  Preference<int>{"Controls/Play mode/Key turn left", Qt::Key_Left};
inline auto PlayModeKeyTurnRight =
  Preference<int>{"Controls/Play mode/Key turn right", Qt::Key_Right};
inline auto PlayModeYawSpeed =
  Preference<float>{"Controls/Play mode/cl_yawspeed", 210.0f};
// What a mouse wheel notch does: one +action/-action pair, like "bind mwheelup +jump".
inline constexpr auto PlayModeWheelNothing = 0;
inline constexpr auto PlayModeWheelJump = 1;
inline constexpr auto PlayModeWheelDuck = 2;
inline auto PlayModeWheelUp =
  Preference<int>{"Controls/Play mode/Mouse wheel up", PlayModeWheelJump};
inline auto PlayModeWheelDown =
  Preference<int>{"Controls/Play mode/Mouse wheel down", PlayModeWheelDuck};

inline auto PlayModeAirAccelerate =
  Preference<float>{"Controls/Play mode/sv_airaccelerate", 10.0f};
inline auto PlayModeAccelerate =
  Preference<float>{"Controls/Play mode/sv_accelerate", 5.0f};
inline auto PlayModeFriction = Preference<float>{"Controls/Play mode/sv_friction", 4.0f};
inline auto PlayModeStopSpeed =
  Preference<float>{"Controls/Play mode/sv_stopspeed", 75.0f};
inline auto PlayModeGravity = Preference<float>{"Controls/Play mode/sv_gravity", 800.0f};
inline auto PlayModeMaxSpeed =
  Preference<float>{"Controls/Play mode/sv_maxspeed", 320.0f};
inline auto PlayModeEdgeFriction =
  Preference<float>{"Controls/Play mode/edgefriction", 2.0f};
inline auto PlayModeStepSize = Preference<float>{"Controls/Play mode/sv_stepsize", 18.0f};

// Whose hlcsg builds the clip hulls (SCS_COMPILER_*).
inline constexpr auto PlayModeCompilerVhlt = 0;
inline constexpr auto PlayModeCompilerZhlt = 1;
inline auto PlayModeCompiler =
  Preference<int>{"Controls/Play mode/Compiler", PlayModeCompilerVhlt};

// hlcsg -cliptype used to build the clip hulls, with VHLT's values (SCS_CLIPTYPE_*).
inline constexpr auto PlayModeClipTypeSmallest = 0;
inline constexpr auto PlayModeClipTypeNormalized = 1;
inline constexpr auto PlayModeClipTypeSimple = 2;
inline constexpr auto PlayModeClipTypePrecise = 3;
inline constexpr auto PlayModeClipTypeLegacy = 4;
inline auto PlayModeClipType =
  Preference<int>{"Controls/Play mode/Cliptype", PlayModeClipTypeLegacy};
// Collide with the map's compiled .bsp when it is up to date instead.
inline auto PlayModeUseCompiledBsp =
  Preference<bool>{"Controls/Play mode/Use compiled bsp", false};

inline auto PlayModeWeaponSpeed =
  Preference<float>{"Controls/Play mode/Weapon speed", 250.0f};
inline auto PlayModeFrameRate = Preference<float>{"Controls/Play mode/fps_max", 100.0f};
// How fps_max becomes usercmd msec: the current Steam client or older ones.
inline constexpr auto PlayModeFrameTimingSteam = 0;
inline constexpr auto PlayModeFrameTimingLegacy = 1;
inline auto PlayModeFrameTiming =
  Preference<int>{"Controls/Play mode/Frame timing", PlayModeFrameTimingSteam};

inline auto PlayModeFov = Preference<float>{"Controls/Play mode/Field of view", 90.0f};
inline auto PlayModeShowHud = Preference<bool>{"Controls/Play mode/Show HUD", true};
inline auto PlayModeShowFallDamage =
  Preference<bool>{"Controls/Play mode/Show fall damage", true};
// Like the game's "volume" cvar; 0 mutes the footsteps and other movement sounds.
inline auto PlayModeSoundVolume = Preference<float>{"Controls/Play mode/Volume", 0.8f};
inline auto PlayModeShowCrosshair =
  Preference<bool>{"Controls/Play mode/Show crosshair", true};

} // namespace tb::Preferences
