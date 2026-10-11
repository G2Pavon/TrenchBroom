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

#include "kzb/PlayMode.h"

#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "base/PreferenceManager.h"
#include "kzb/PlayModeController.h"
#include "kzb/PlayModeRawMouse.h"
#include "kzb/Preferences.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace tb::kzb
{

PlayMode::PlayMode(
  ui::MapView3D& view, ui::MapDocument& document, gl::PerspectiveCamera& camera)
  : m_view{view}
  , m_controller{std::make_unique<PlayModeController>(document, camera)}
  , m_rawMouse{std::make_unique<PlayModeRawMouse>(
      [&](const double dx, const double dy, const double timestampMs) {
        if (m_controller->active())
        {
          m_controller->rawLook(dx, dy, timestampMs);
        }
      })}
{
  m_frameTimer.setSingleShot(true);
  m_frameTimer.setTimerType(Qt::PreciseTimer);
  connect(&m_frameTimer, &QTimer::timeout, this, [this]() { m_view.update(); });

  // Like the game: draw each command's frame as soon as it is due.
  connect(&m_view, &QOpenGLWidget::frameSwapped, this, [this]() {
    if (active())
    {
      m_frameTimer.start(static_cast<int>(std::ceil(m_controller->msUntilNextFrame())));
    }
  });

  m_view.installEventFilter(this);
}

PlayMode::~PlayMode()
{
  stop();
}

bool PlayMode::active() const
{
  return m_controller->active();
}

void PlayMode::start(const bool fromCamera)
{
  if (active())
  {
    return;
  }

  m_controller->start(fromCamera);
  if (!active())
  {
    return;
  }

  m_mouseTrackingBeforePlay = m_view.hasMouseTracking();
  m_view.setMouseTracking(true);
  m_view.setCursor(Qt::BlankCursor);
  m_view.setFocus(Qt::OtherFocusReason);

  // Capture the mouse like the game: the windowing system keeps the pointer in place and
  // motion is read as relative movement, raw (m_rawinput 1) or accelerated. The cursor is
  // captured either way so the real cursor never leaves the view. Without the required
  // protocols, fall back to cursor deltas and warp the cursor back to the center (on
  // Wayland, Qt warps through the pointer-warp protocol where the compositor offers it).
  const auto raw = pref(Preferences::PlayModeRawInput);
  const auto backend = m_rawMouse->start(m_view, raw);
  m_warpCursor = true;
  m_grabbedMouse = !m_rawMouse->confinesPointer();
  if (m_grabbedMouse)
  {
    m_view.grabMouse();
  }

  const auto motion = std::string{raw ? "raw" : "accelerated"};
  const auto confined = m_rawMouse->confinesPointer() ? "" : ", not confined";
  switch (backend)
  {
  case PlayModeRawMouse::Backend::Wayland:
    m_controller->setInputDescription(motion + ", Wayland locked pointer");
    break;
  case PlayModeRawMouse::Backend::XInput2:
    m_controller->setInputDescription(motion + ", XInput2" + confined);
    break;
  case PlayModeRawMouse::Backend::RawInput:
    m_controller->setInputDescription(motion + ", Raw Input" + confined);
    break;
  case PlayModeRawMouse::Backend::None:
    m_controller->setInputDescription(
      std::string{"cursor warping (accelerated)"}
      + (m_rawMouse->confinesPointer() ? ", confined" : ""));
    break;
  }

  m_wheelAccumulator = 0;
  m_lastMousePos = QCursor::pos() - m_view.mapToGlobal(QPoint{0, 0});
  m_awaitingCursorWarp = false;
  recenterCursor();
  m_view.update();
}

void PlayMode::stop()
{
  if (!active())
  {
    return;
  }

  m_controller->stop();
  m_frameTimer.stop();
  m_rawMouse->stop();
  if (m_grabbedMouse)
  {
    m_view.releaseMouse();
    m_grabbedMouse = false;
  }
  m_view.unsetCursor();
  m_view.setMouseTracking(m_mouseTrackingBeforePlay);
  m_view.update();
}

void PlayMode::preRender()
{
  m_rawMouse->poll();
  m_controller->update();
}

void PlayMode::render(
  render::RenderContext& renderContext, render::RenderBatch& renderBatch)
{
  m_controller->render(renderContext, renderBatch);
}

bool PlayMode::eventFilter(QObject* /* watched */, QEvent* event)
{
  if (!active())
  {
    return false;
  }

  switch (event->type())
  {
  case QEvent::ShortcutOverride:
    // While playing, every key belongs to the game: accepting the override delivers it
    // as a key press instead of triggering editor shortcuts (E, R, Space, ...).
    event->accept();
    return true;
  case QEvent::KeyPress: {
    const auto& keyEvent = static_cast<const QKeyEvent&>(*event);
    if (
      !keyEvent.isAutoRepeat()
      && (keyEvent.key() == Qt::Key_Escape || keyEvent.key() == Qt::Key_F7))
    {
      stop();
    }
    else
    {
      m_controller->keyDown(keyEvent);
    }
    return true;
  }
  case QEvent::KeyRelease:
    m_controller->keyUp(static_cast<const QKeyEvent&>(*event));
    return true;
  case QEvent::MouseMove:
    mouseMove(static_cast<const QMouseEvent&>(*event));
    return true;
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseButtonDblClick:
    return true;
  case QEvent::Wheel:
    wheel(static_cast<const QWheelEvent&>(*event));
    event->accept();
    return true;
  case QEvent::FocusOut:
    stop();
    return false;
  default:
    return false;
  }
}

void PlayMode::mouseMove(const QMouseEvent& event)
{
  const auto position = event.position();
  const auto center = QPointF{m_view.rect().center()};
  const auto recenterDistance =
    0.25 * static_cast<double>(std::min(m_view.width(), m_view.height()));
  const auto distanceToCenter = [&]() {
    const auto d = position - center;
    return std::sqrt(d.x() * d.x() + d.y() * d.y());
  };

  if (m_awaitingCursorWarp)
  {
    // Motion events queued before the cursor was warped are relative to the old cursor
    // position; using them would count that movement twice.
    if (distanceToCenter() > 0.5 * recenterDistance)
    {
      return;
    }
    m_awaitingCursorWarp = false;
  }

  const auto delta = position - m_lastMousePos;
  m_lastMousePos = position;
  if (!delta.isNull() && m_rawMouse->backend() == PlayModeRawMouse::Backend::None)
  {
    // Mouse counts correspond to device pixels.
    const auto scale = m_view.devicePixelRatioF();
    m_controller->look(
      event,
      static_cast<float>(delta.x() * scale),
      static_cast<float>(delta.y() * scale));
  }

  // Warp rarely: only when the cursor gets close to the edge of the view.
  if (distanceToCenter() > recenterDistance)
  {
    recenterCursor();
  }
}

void PlayMode::wheel(const QWheelEvent& event)
{
  // Every wheel notch (120 units) is one +action/-action pair, like a wheel bind in the
  // game. High resolution wheels report fractions of a notch, which are accumulated.
  constexpr auto Notch = 120;
  m_wheelAccumulator += event.angleDelta().y();
  while (std::abs(m_wheelAccumulator) >= Notch)
  {
    const auto up = m_wheelAccumulator > 0;
    m_controller->wheel(event, up);
    m_wheelAccumulator -= up ? Notch : -Notch;
  }
}

void PlayMode::recenterCursor()
{
  if (m_warpCursor)
  {
    QCursor::setPos(m_view.mapToGlobal(m_view.rect().center()));
    m_lastMousePos = QPointF{m_view.rect().center()};
    m_awaitingCursorWarp = true;
  }
}

} // namespace tb::kzb
