/*   Copyright 2010 Craig Eaton
 *
. *   This file is part of LSTune.
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

#include "tuner.h"

//-----------------------------------------------------------

int main(int argc, char **argv)
{
  // Create main widget, and run
  QApplication *app = new QApplication(argc, argv);

  QMainWindow *mainWin = new QMainWindow;
  Tuner *mWidget = new Tuner(mainWin);

  mainWin->setWindowTitle(QObject::tr("LSTune"));
  mainWin->setGeometry(100, 100, 650, 380);
  mainWin->setCentralWidget(mWidget);

  QMenu *fileMenu = mainWin->menuBar()->addMenu(QObject::tr("&File"));
  QMenu *inputMenu = fileMenu->addMenu(QObject::tr("&Audio Input"));
  fileMenu->addAction(QObject::tr("&Quit"), app, SLOT(quit()));

  // List of input devices in file menu
  QSignalMapper *signalMapper = new QSignalMapper();
  QList<QAudioDeviceInfo> devices = mWidget->getDevices();
  for(int i = 0; i < devices.size(); ++i)
    {
      QAction *action = inputMenu->addAction(devices.at(i).deviceName(), signalMapper, SLOT(map()));
      signalMapper->setMapping(action, i);
    }
  mainWin->connect(signalMapper, SIGNAL(mapped(int)), mWidget, SLOT(switchDevice(int)));


  QMenu *helpMenu = mainWin->menuBar()->addMenu(QObject::tr("&Help"));
  helpMenu->addAction(QObject::tr("&About"), mWidget, SLOT(about()));
  
  mainWin->show();
  return app->exec();
}
