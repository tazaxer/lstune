/*   Copyright 2010 Craig Eaton
 *
 *   This file is part of LSTune.
 *
 *    LSTune is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 *   LSTune is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 * with LSTune. If not, see http://www.gnu.org/licenses/.
 */

#include <QtCore>
#if QT_VERSION >= 0x050000
#  include <QtWidgets>
#else
#  include <QtGui>
#endif
#include <QDebug>

#include "LED.h"

// --------------------------------------------
const float DIAMETER_INCHES = 0.2; // LED diameter in inches
// --------------------------------------------

LED::LED(QWidget *parent) : QWidget(parent)
{
  state = false;

  width = (int) (QPaintDevice::logicalDpiX() * DIAMETER_INCHES);
  height = (int) (QPaintDevice::logicalDpiY() * DIAMETER_INCHES);

  setFixedSize(width, height);
  setAutoFillBackground(false);

  onColor = QColor(0, 255, 0); // Green for on
  offColor = QColor(0, 0, 0); // Black for on
}

LED::~LED()
{
}

void LED::setState(bool newState)
{
  if (state != newState) 
    {
      state = newState;
      update();
    }
}

void LED::toggle()
{
  setState(!state);
}


void LED::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (state) 
      painter.setBrush(onColor);
    else
      painter.setBrush(offColor);
    // Offset to account for border width
    painter.drawEllipse (1, 1, width-2, height-2);
}

void LED::mousePressEvent(QMouseEvent *event) 
{
  Q_UNUSED(event);
  emit clicked();
}
 
