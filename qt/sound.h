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

#ifndef SOUND_H
#define SOUND_H

#include <QWidget>
#include "noteGuesser.h"
#include "filter.h"
#include "notes.h"

class AudioIO;

class AudioProc : public QWidget
{
  Q_OBJECT;

public:
  AudioProc(int fsample, notes_t *inotes, QWidget *parent = 0);
  ~AudioProc();
  AudioIO *getAudioIO();
  note_t *getCurNote();

public slots:
  void readAudio();
  void setWheelFreq(double newFreq);
  void setNoteDetect(bool state);
  void setHysteresis(double newValue);
  void setA4Freq(double newValue);
  void start();
  void stop();

signals:
  void newAngle(float angle, float deltaT);
  // void changedFreq(double freq);
  void changedNote(note_t *note);
  void isFlat(bool flat);
  void isSharp(bool sharp);
  
private:
  AudioIO *soundIO;

  notes_t *notes;
  note_t *curNote;

  noteGuesser *guesser;

  bool noteDetect;
  double hysteresis;

  qint64 frames, maxFrames;

  // Timer and filter
  // QTimer *timer;
  dtFilter *filter;

  bool strobeState; // State of strobe
  qint64 fTime;       // Counts number of samples since beginning of time
  qint64 lastUpdate;  // Last time wheel was refreshed
  qint64 prevRise;    // Time of previous rise
  
  double wheelFreq; // Wheel frequency in Hz

  int fSample;
  double tSample;
};

#endif
