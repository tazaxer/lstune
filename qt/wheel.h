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

#ifndef WHEEL_H
#define WHEEL_H

#include <QtCore>
#if QT_VERSION >= 0x050000
#  include <QtWidgets>
#else
#  include <QtGui>
#endif

using namespace std;

class Wheel : public QWidget
{
  Q_OBJECT

public:
  Wheel(QWidget *parent = 0);
  ~Wheel();

public slots: 
  void setAngle(int angle);
  void setAnglef(float angle, float deltaT);

private:
  void calcBrightness(float timeDelta, bool allFalse);

  float currentAngle;
  int segments;
  int rings;
  int block_w, block_h;

  float wheelFreq; // Wheel frequency in Hz

  QVector< QVector<float> > segState;
  QList< QList< QList<bool> > > wheelPositions;

 protected:
  void paintEvent(QPaintEvent *event);
};

#endif

