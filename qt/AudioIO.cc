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

#include "AudioIO.h"
#include "noteGuesser.h"

//--------------------------------------
// Constants
//--------------------------------------
const double maxBuffLength = 2.0;

//--------------------------------------
AudioIO::AudioIO(int fsample, int inFrameRate, QWidget *parent)
  : QWidget(parent)
{
  started    = false;
  readDevice = NULL;

  fSample   = fsample;
  frameRate = inFrameRate;

  notifyInterval = -1;
  audioInput     = NULL;

  // Poll timer drives getAudio() at the frame rate.
  // This is also the watchdog: if QAudioInput goes IdleState on Qt4/CoreAudio
  // (a known PPC bug where the stream silently stops after the buffer drains),
  // onPollTimer() performs a full stop+start to revive it.
  pollIntervalMs = qMax(1, 1000 / qMax(1, frameRate));
  pollTimer = new QTimer(this);
  pollTimer->setInterval(pollIntervalMs);
  connect(pollTimer, SIGNAL(timeout()), this, SLOT(onPollTimer()));

  initHW(QAudioDeviceInfo::defaultInputDevice());

  devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
}

AudioIO::~AudioIO()
{
  stop();
  if (audioInput != NULL)
    delete audioInput;
}

//--------------------------------------
void AudioIO::switchDevice(int deviceIndex)
{
  bool wasStarted = started;
  if (wasStarted)
    stop();

  if (deviceIndex >= devices.size() || deviceIndex < 0) {
    qWarning() << "Error: invalid device index. must be between 0 and " << devices.size() - 1;
    exit(1);
  }
  QAudioDeviceInfo devInfo = devices.at(deviceIndex);
  initHW(devInfo);

  if (wasStarted)
    start();
}

//--------------------------------------
// Initializes new audioInput object
void AudioIO::initHW(QAudioDeviceInfo inDevice)
{
  if (audioInput != NULL) {
    stop();
    delete audioInput;
    audioInput = NULL;
  }

  QAudioFormat audioFormat;
  QAudioDeviceInfo info(inDevice);
  foreach (QString codec, info.supportedCodecs())
    qDebug() << "Codec: " << codec;
  foreach (int f, info.supportedSampleRates())
    qDebug() << "Frequency: " << f;
  foreach (int ss, info.supportedSampleSizes())
    qDebug() << "Sample size: " << ss;
  foreach (QAudioFormat::SampleType st, info.supportedSampleTypes())
    qDebug() << "Sample type: " << st;
  qDebug() << "Device name" << info.deviceName();

  audioFormat.setChannelCount(1);
  audioFormat.setSampleRate(fSample);
  audioFormat.setCodec("audio/pcm");
  // Use native byte order — no byte-swapping on big-endian PowerPC.
  audioFormat.setByteOrder(
      (QSysInfo::ByteOrder == QSysInfo::LittleEndian)
          ? QAudioFormat::LittleEndian
          : QAudioFormat::BigEndian);
  audioFormat.setSampleType(QAudioFormat::Float);
  audioFormat.setSampleSize(32);

  if (!info.isFormatSupported(audioFormat)) {
    qWarning() << "Default audio format not supported. Using nearest available";
    audioFormat = info.nearestFormat(audioFormat);
    qWarning() << "Channels: "    << audioFormat.channelCount();
    qWarning() << "Frequency: "   << audioFormat.sampleRate();
    qWarning() << "Codec: "       << audioFormat.codec();
    qWarning() << "Sample Size: " << audioFormat.sampleSize();
  }

  int sType = audioFormat.sampleType();
  if ((sType != QAudioFormat::Float) || (audioFormat.sampleSize() != 32)) {
    qWarning("Only float 32-bit samples are supported! Exiting");
    qWarning() << "Sample Type:" << sType;
    exit(1);
  }

  audioInput = new QAudioInput(info, audioFormat, this);

  // Keep notify() wired — it works fine on Linux/Windows and is harmless
  // here since the pollTimer drives everything on CoreAudio/PPC.
  if (notifyInterval > -1)
    audioInput->setNotifyInterval(notifyInterval);
  connect(audioInput, SIGNAL(notify()),                    this, SIGNAL(notify()));
  connect(audioInput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(onStateChanged(QAudio::State)));

  // Set a small hardware buffer so CoreAudio delivers data at roughly frame
  // rate frequency.  Without this, the default Mac buffer can be several
  // hundred ms, causing getAudio() to return 0 on most poll ticks.
  // (Excluded on Windows XP where this call causes misbehaviour.)
  if (BUILD_LINUX || BUILD_MACX)
    audioInput->setBufferSize(1000 / frameRate);
}

//--------------------------------------
QList<QAudioDeviceInfo> AudioIO::getDevices()
{
  return devices;
}

//--------------------------------------
void AudioIO::setNotifyInterval(int ms)
{
  notifyInterval = ms;
  // Use the requested interval for the poll timer too.
  if (ms > 0) {
    pollIntervalMs = ms;
    pollTimer->setInterval(ms);
  }
  if (audioInput != NULL && notifyInterval > -1)
    audioInput->setNotifyInterval(notifyInterval);
}

//--------------------------------------
// Pull-mode start: audioInput->start() returns a QIODevice we read() from.
// No QBuffer, no seek(), no pos() arithmetic — QAudioInput owns the buffer.
void AudioIO::start()
{
  readDevice = audioInput->start(); // pull mode
  if (!readDevice) {
    qWarning("AudioIO::start() — audioInput->start() returned NULL");
    return;
  }
  started = true;
  pollTimer->start();
  qDebug() << "AudioIO: started (pull mode), poll interval" << pollIntervalMs << "ms";
}

//--------------------------------------
void AudioIO::stop()
{
  pollTimer->stop();
  if (audioInput)
    audioInput->stop();
  readDevice = NULL;
  started    = false;
}

//--------------------------------------
// doRestart: stop + start the stream without rebuilding the QAudioInput.
// Called by onPollTimer() when IdleState is detected — equivalent to what
// the user was doing manually by switching inputs.
void AudioIO::doRestart()
{
  qDebug() << "AudioIO: restarting stream to recover from IdleState";
  audioInput->stop();
  readDevice = audioInput->start(); // pull mode restart
  if (!readDevice)
    qWarning("AudioIO::doRestart() — audioInput->start() returned NULL");
}

//--------------------------------------
// onPollTimer — the main driver slot
//--------------------------------------
// Fires every pollIntervalMs.  Emits notify() to trigger readAudio() in
// AudioProc.  Also detects IdleState and does a full stream restart — this
// is the fix for the Qt4/CoreAudio PPC bug where the stream silently stops.
void AudioIO::onPollTimer()
{
  if (!started)
    return;

  QAudio::State st = audioInput->state();
  if (st == QAudio::IdleState || st == QAudio::StoppedState) {
    doRestart();
  }

  emit notify();
}

//--------------------------------------
void AudioIO::onStateChanged(QAudio::State state)
{
  switch (state) {
    case QAudio::ActiveState:
      qDebug() << "AudioIO: stream Active";
      break;
    case QAudio::IdleState:
      qDebug() << "AudioIO: stream Idle — watchdog will restart on next tick";
      break;
    case QAudio::StoppedState:
      if (audioInput->error() != QAudio::NoError)
        qWarning() << "AudioIO: stream stopped with error" << audioInput->error();
      break;
    default:
      break;
  }
}

//--------------------------------------
// getAudio — pull-mode read
//--------------------------------------
// Simply reads whatever bytes are available from readDevice.
// No seeking, no write-pointer tracking — all managed by QAudioInput internally.
qint64 AudioIO::getAudio(float *inBuffer, int maxSamples)
{
  if (!readDevice || !started)
    return 0;

  QAudioFormat format = audioInput->format();
  const qint64 dataSize = format.sampleSize() / 8; // bytes per sample

  qint64 bytesAvail = readDevice->bytesAvailable();
  if (bytesAvail <= 0)
    return 0;

  qint64 samplesToRead = bytesAvail / dataSize;
  if (samplesToRead > maxSamples)
    samplesToRead = maxSamples;
  qint64 bytesToRead = samplesToRead * dataSize;

  qint64 readSamples;
  if (format.sampleType() == QAudioFormat::Float) {
    readSamples = readDevice->read((char *)inBuffer, bytesToRead) / dataSize;
  } else {
    // Integer samples — convert to float
    float scale = float(1.0 / (2 << (dataSize * 8 - 2)));
    QVarLengthArray<qint32> intBuf(samplesToRead);
    readSamples = readDevice->read((char *)intBuf.data(), bytesToRead) / dataSize;
    for (int i = 0; i < readSamples; i++)
      inBuffer[i] = float(intBuf[i] * scale);
  }

  return readSamples;
}
