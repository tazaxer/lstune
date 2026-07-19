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
// #include <QtMultimedia>

#include "sound.h"
#include "wheel.h"
#include "notes.h"
#include "tuner.h"
#include "AudioIO.h"
#include "LED.h"

//-----------------------------------------------------------
// Constants
//-----------------------------------------------------------
// Sample frequency and preiod
const int fsample = 48000;
const double A4_DEFAULT = 440.0;

//-------------------------------------------------------------
// Evil Global. Fix me
//-------------------------------------------------------------
notes_t notes(A4_DEFAULT);


Tuner::Tuner(QWidget *parent)
  : QWidget(parent)
{
   ui.setupUi(this);   
 
   // Wheel display object
   Wheel *sWheel = new Wheel(ui.strobeFrame);
   
   // Sharp/flat labels + LED
   LED *flatLED  = new LED(this);
   LED *sharpLED = new LED(this);
   flatLED->setState(false);
   sharpLED->setState(false);
   QLabel *flatLabel  = new QLabel("<<< FLAT",  this);
   QLabel *sharpLabel = new QLabel("SHARP >>>", this);
   sharpLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
   flatLabel->setObjectName("flatLabel");
   sharpLabel->setObjectName("sharpLabel");
   
   ui.sharpFlatLayout->addWidget(flatLED);
   ui.sharpFlatLayout->addWidget(flatLabel);
   ui.sharpFlatLayout->addWidget(sharpLabel);
   ui.sharpFlatLayout->addWidget(sharpLED);

   // HIDE unnecessary controls for a clean Bass Tuner UI
   ui.A4SpinBox->hide();
   ui.A4Label->hide();
   ui.noteSelBox->hide();
   ui.freqLabel->hide();

   // Apply Dark Mode Stage Theme
   this->setStyleSheet(
       "QWidget { background-color: #111111; color: #FFFFFF; font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif; }"
       "QLabel { font-size: 24px; font-weight: bold; }"
       "QLabel#noteDisp { font-size: 140px; color: #00E676; border: none; }"
       "QLabel#flatLabel { color: #FF3D00; font-size: 36px; }"
       "QLabel#sharpLabel { color: #FF3D00; font-size: 36px; }"
       "QLCDNumber { color: #00E676; background-color: #222222; border: 2px solid #333333; }"
   );
   
   ui.noteDisp->setAlignment(Qt::AlignCenter);

   // Create timer to process audio input
   aProc = new AudioProc(fsample, &notes);
   soundIO = aProc->getAudioIO();
   connect(aProc, SIGNAL(newAngle(float, float)), sWheel, SLOT(setAnglef(float, float)));
   connect(aProc, SIGNAL(changedNote(note_t *)),  this,   SLOT(setDisplayedNote(note_t *)));

   // hook up sharp/flat LEDs
   connect(aProc, SIGNAL(isSharp(bool)), sharpLED, SLOT(setState(bool)));
   connect(aProc, SIGNAL(isFlat (bool)), flatLED,  SLOT(setState(bool)));

   // Use check box to disable auto note detection
   connect(ui.autoDetect, SIGNAL(clicked(bool)),
	   aProc, SLOT(setNoteDetect(bool)));

   // Spin box controls A4 frequenct
   connect(ui.A4SpinBox, SIGNAL(valueChanged(double)),
	   aProc,  SLOT(setA4Freq(double)));
   
   // Noise gate spin box
   connect(ui.noiseGateSpinBox, SIGNAL(valueChanged(double)),
	   aProc,   SLOT(setHysteresis(double)));

   // Add notes to note selection box
   for (int n = 0; n < notes.total_notes; n++)
     ui.noteSelBox->addItem(notes.get_note_offset(n)->fullName, QVariant(n));
   
   // Start timer
   aProc->start();
}

Tuner::~Tuner()
{
  delete aProc;
}

void Tuner::switchDevice(int device)
{
  // qDebug() << "Switching device" << device;
  soundIO->switchDevice(device);
}

QList<QAudioDeviceInfo> Tuner::getDevices()
{
  return soundIO->getDevices();
}

void Tuner::setDisplayedNote(note_t *note)
{
  curNote = note;
  QString nname = curNote->name;
  if (nname.size() == 1)
    nname.append(QString(" "));
  ui.noteDisp->setText(nname);
  ui.freqDisp->display(curNote->freq);
  aProc->setWheelFreq(note->freq);
}

void Tuner::on_autoDetect_clicked (bool clicked)
{
  ui.noteSelBox->setEnabled(clicked);
  
  // Manual selection was just enabled. Display currently selected note
  if (clicked)
    {
      int curIndex = ui.noteSelBox->currentIndex();
      int noteIdx = ui.noteSelBox->itemData(curIndex).toInt();
      note_t *note = notes.get_note_offset(noteIdx);
      setDisplayedNote(note);
    }
  else 
    setDisplayedNote(aProc->getCurNote());
}

void Tuner::on_A4SpinBox_valueChanged (double freq)
{
  aProc->setA4Freq(freq);
  setDisplayedNote(curNote);
}

void Tuner::on_noteSelBox_currentIndexChanged(int index)
{
  int noteIdx = ui.noteSelBox->itemData(index).toInt();
  note_t *note = notes.get_note_offset(noteIdx);
  setDisplayedNote(note);
}



void Tuner::about()
{
  QMessageBox::about(this, tr("About"), 
		     tr("LS Tune Strobe Tuner, by Craig Eaton\n\n"
			"   LS Tune is free software: you can redistribute it and/or modify it under"
			" the terms of the GNU General Public License as published by the Free"
			" Software Foundation, either version 3 of the License, or (at your option)"
			" any later version."
			"\n\n"
			"   LS Tune is distributed in the hope that it will be useful, but"
			" WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY"
			" or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License"
			" for more details."
			"\n\n"
			"   You should have received a copy of the GNU General Public License along"
			" with LS Tune. If not, see http://www.gnu.org/licenses/."));
		     

}

