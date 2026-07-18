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
#include <math.h>

#include "wheel.h"

const int OFFSET = 3;

using namespace std;

void init_wheel(QList<bool> &wheel, int segments, int pos, int offset)
{
  for (int n = 0; n < segments; n++)
    {
      int str_pos = (n+pos) % segments;
      int idx = str_pos + offset;
      if (2*n < segments+1)
        wheel.replace(idx, true);
      else
        wheel.replace(idx, false);
    }
}

Wheel::Wheel(QWidget *parent) : QWidget(parent)
{
  //  segments = 64;
  //segments = 2 * 4 * 6 * 2; //96
  segments = 2 * 2 * 2 * 3 * 5; // 120
  rings = 4;
  currentAngle = 0.0;
  block_w = 5;
  block_h = 20;

  // Set widget size
  setFixedSize(QSize(block_w * segments + OFFSET, block_h * rings + OFFSET));

  // This is a normalized float between zero and 1.0
  // Create 2d QVector, initialized to zero
  segState = QVector< QVector<float> > (rings, QVector<float>(segments, 0.0));

  setPalette(QPalette(QColor(250, 250, 200)));
  setAutoFillBackground(false);

  // Wheel[ring][pos][idx]
  wheelPositions = QList< QList< QList<bool> > >();
  for(int r = 0; r < rings; r++)
    {
      wheelPositions.append(QList< QList<bool> >());
      for(int p = 0; p < segments; p++)
      {
         wheelPositions[r].append(QList<bool>());
         for (int s = 0; s < segments; s++)
              wheelPositions[r][p].append(false);
      }
    }

  // Initialize the array
  for (int ring = 0; ring < rings; ring++)
    for (int pos = 0; pos < segments; pos++)
      {
	// This make wheel move the the right when note is sharp
	int idx = segments - 1 - pos;
	int ring_divisions = ring + 2;
	int division_segments = segments/ring_divisions;

	for (int division = 0; division < ring_divisions; division++)
	  {
	    int offset = division * division_segments;
	    init_wheel(wheelPositions[ring][idx],  division_segments, pos, offset);
	  }
      }
}

Wheel::~Wheel()
{
}

void Wheel::calcBrightness(float timeDelta, bool allFalse)
{
  // how much to incr/decr each element per time step
  const float lightTC = (float)-10e-3; // Time constant for display lights in seconds
  const float strobeLength = (float)750e-6; // Length of strobe "light flash" in seconds

  // Snap angle to display element
  int wheelPos = int( floor(segments * currentAngle/(2*M_PI)) );
  
  for (int ring = 0; ring < rings; ring++)
    for (int n = 0; n < segments; n++)
      {
	bool state = wheelPositions[ring][wheelPos][n];
	float b0 = segState[ring][n];

	// Model light brightnses as exponential decay with a single time constant
	if (!allFalse && state) // Calculate extra brightness
        segState[ring][n] = 1;
      //segState[ring][n] = (1.0 - b0) * (1.0 - exp(strobeLength / lightTC)) + b0;
	else // Calculate time decay since last update
        segState[ring][n] = 0;
      //segState[ring][n] = b0 * exp(timeDelta / lightTC);
      }
}

void Wheel::setAngle(int angle)
{
  double angle_radians = (double)angle/(double)segments * 2*M_PI;
  currentAngle = fmod(angle_radians, 2*M_PI);
  calcBrightness((float)1/100.0, false);
  update(); // Doesn't do an imediate repaint
}

void Wheel::setAnglef(float angle, float deltaT)
{
  bool allFalse = (angle < -2 * M_PI);
  if (!allFalse)
    currentAngle = fmodf(angle, (float)2*M_PI);
  calcBrightness(deltaT, allFalse);
  update(); // Doesn't do an imediate repaint
}

void Wheel::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);

  // Pen object. Color, width, fillstyle, cap, join
  // QPen pen(Qt::darkGrey, 0);
  // painter.setPen(pen);

  for (int ring = 0; ring < rings; ring++)
    for (int n = 0; n < segments; n++)
      {
	int green = int(255.0 * segState[ring][n]);
	if (green > 255 || green < 0) {
	  qDebug() << "Error: Green = " << green << "state=" << segState[ring][n];
	  segState[ring][n] = 0.5;
	}

	int x = block_w * n + OFFSET;
	int y = block_h * ring + OFFSET;
	painter.setBrush(QColor(0, green, 0));
	painter.drawRect(QRect(x, y, block_w, block_h));
      }
}
