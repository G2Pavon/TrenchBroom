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

#include <QObject>
#include <QPointF>
#include <QTimer>

#include "ui/MapView3D.h"

#include <memory>

class QMouseEvent;
class QWheelEvent;

namespace tb
{
namespace gl
{
class PerspectiveCamera;
}

namespace ui
{
class MapDocument;
}

namespace kzb
{
class PlayModeController;
class PlayModeRawMouse;

/**
 * Play mode in one 3D map view: captures the mouse like the game, takes the view's
 * input through an event filter while playing, and draws one frame per command.
 */
class PlayMode : public QObject, public ui::MapView3DPlayMode
{
private:
  ui::MapView3D& m_view;
  std::unique_ptr<PlayModeController> m_controller;
  std::unique_ptr<PlayModeRawMouse> m_rawMouse;
  // The view does not wait for vsync, so play mode paces its own frames.
  QTimer m_frameTimer;

  // Mouse capture
  bool m_warpCursor = true;
  bool m_mouseTrackingBeforePlay = false;
  bool m_awaitingCursorWarp = false;
  bool m_grabbedMouse = false;
  int m_wheelAccumulator = 0;
  QPointF m_lastMousePos;

public:
  PlayMode(ui::MapView3D& view, ui::MapDocument& document, gl::PerspectiveCamera& camera);
  ~PlayMode() override;

  bool active() const override;
  void start(bool fromCamera) override;
  void stop();

  void preRender() override;
  void render(
    render::RenderContext& renderContext, render::RenderBatch& renderBatch) override;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void mouseMove(const QMouseEvent& event);
  void wheel(const QWheelEvent& event);
  void recenterCursor();
};

} // namespace kzb
} // namespace tb
