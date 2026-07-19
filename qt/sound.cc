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
 * with LSTune. If ot, see http://www.gnu.org/licenses/.
 */ 

#include <math.h>
#include <stdio.h>
#include <QtDebug>
// #include <QtMultimedia>

#include "AudioIO.h"
#include "sound.h"

#include "noteGuesser.h"

//--------------------------------------
// Constants
//--------------------------------------
const double DEFAULT_HYSTERESIS = 0.002;
const int FRAMERATE = 30; // The frequency of getting a frame
const int SAFTEYMARGIN = 2;

//--------------------------------------
AudioProc::AudioProc(int fsample, notes_t *inotes, QWidget *parent)
  : QWidget(parent)
{
  fSample = fsample;
  tSample = double(1.0/fSample);
  
  notes = inotes;
  
  strobeState = false;
  fTime = prevRise = 0;
  
  guesser = new noteGuesser(notes, fSample);
  curNote = notes->get_note_offset(0);

  noteDetect = 1;
  hysteresis = DEFAULT_HYSTERESIS;

  //------------------------------------------------
  // Init the sound hardware
  //------------------------------------------------  
  soundIO = new AudioIO(fsample, FRAMERATE);
  frames = qint64(fsample/(FRAMERATE));
  maxFrames = frames * SAFTEYMARGIN;

  // Set up timer
  soundIO->setNotifyInterval(1000*2/(3*FRAMERATE));
  connect(soundIO, SIGNAL(notify()), this, SLOT(readAudio()));
  
  //------------------------------------------------
  filter = new dtFilter(int(maxFrames));
}

AudioProc::~AudioProc()
{
  stop();
  delete guesser;
  delete soundIO;
  delete filter;
}

AudioIO *AudioProc::getAudioIO()
{
  return soundIO;
}

void AudioProc::start()
{
  soundIO->start();
}

void AudioProc::stop()
{
  soundIO->stop();
}

void AudioProc::setWheelFreq(double newFreq)
{
  wheelFreq = newFreq;
}

void AudioProc::setNoteDetect(bool state)
{
  noteDetect = !state;
}

void AudioProc::setHysteresis(double newValue)
{
  hysteresis = newValue;
}

void AudioProc::setA4Freq(double newValue)
{
  notes->set_a4freq(float(newValue));
  setWheelFreq(double(curNote->freq));
}

note_t *AudioProc::getCurNote()
{
  return curNote;
}

void AudioProc::readAudio()
{
  float *inBuffer = filter->get_inbuf(); // Filter input buffer
  float *outBuffer = filter->get_outbuf(); // Filter input buffer

  // Read the audio 
  // qDebug() << "Bytes ready " << audioInput->bytesReady();
  qint64 readFrames = soundIO->getAudio(inBuffer, maxFrames);
  if (readFrames <= 0) 
    return;

  // qDebug() << "Got" << readFrames << "samples" << readBytes << "Bytes";
  filter->filter(readFrames);  // Process buffer

  // Maximum number of samples that are allowed to pass before an update to the screen.
  // The prevents if from staying lit up with zero input. 
  // Changed to fSample/5 (200ms) to better capture slow-moving low bass frequencies.
  int maxNoUpdate = fSample/5;
  float peakVal = -1e6;
  int peakTime = -1;
  // Process the results
  for (qint64 i = 0; i < readFrames; i++)
    {
      double val = outBuffer[i];
      // qDebug() << "val = " << val;
      if (strobeState && val <= -hysteresis)
	{
	  strobeState = false;

	  // Record and reset peak
	  if (peakTime > -1)
	    guesser->addFoundPeak(peakTime, peakVal);
	  peakVal = -1e6;
	  peakTime = -1;
	}
      else if (val >= hysteresis)
	{
	  if (val >= peakVal) {
	    peakVal = val;
	    peakTime = fTime +i;
	  }
	  if (strobeState)
	    continue;

	  strobeState = true;
	  int t = fTime + i;

	  double wheelRemainder = fmod((double)t * tSample, 1.0/wheelFreq);
	  // Wheel postion in radians
	  double wheelPos = wheelRemainder * wheelFreq * 2*M_PI;

	  // Time elapses since last rise
	  float deltaT = (float)(t - prevRise) * tSample; 
	  emit newAngle((float)wheelPos, deltaT);

	  prevRise = lastUpdate = t;
	}
    } // END  for (qint64 i = 0; i < readFrames; i++)
  fTime += readFrames;
  // Reset counter every so often to prevent eventual loss of floating point precission
  if (fTime > 10000000)
    fTime = 0;

  // Guess notes
  note_t *guessedNote = guesser->guessNotes(curNote, !noteDetect);
  if (guessedNote != NULL) 
    {
      emit isFlat(guesser->isFlat());
      emit isSharp(guesser->isSharp());
    }
  if (noteDetect && guessedNote != NULL && guessedNote != curNote)
    {
      curNote = guessedNote;
      setWheelFreq(curNote->freq);
      emit changedNote(curNote);
      //qDebug() << "Switched note to" << curNote->fullName;
    }
  
  // Refresh the wheel display if we haven't gotten a strobe in a while
  if ((fTime - lastUpdate) > maxNoUpdate) {
    emit newAngle(-100, (float)(fTime - lastUpdate) * tSample);
    lastUpdate = fTime;
  }

  //qDebug() << "Done: "  << ready << "\n";

}

