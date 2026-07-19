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
#include <QMutex>
#include <QMutexLocker>

#include "AudioIO.h"
#include "noteGuesser.h"

//--------------------------------------
// Constants
//--------------------------------------
// Maximum buffer length in seconds
const double maxBuffLength = 2.0;

//--------------------------------------
// Thread-safe ring buffer for QAudioInput
// On Mac CoreAudio, QAudioInput writes from a high-priority background thread.
// Reading from the main thread while writing using QBuffer causes extreme 
// race conditions that eventually crash or freeze the stream.
//--------------------------------------
class AudioRingBuffer : public QIODevice {
public:
    AudioRingBuffer(QObject *parent = 0) : QIODevice(parent) {
        open(QIODevice::ReadWrite);
    }
    
    QByteArray buffer;
    mutable QMutex mutex;

    qint64 readData(char *data, qint64 maxlen) {
        QMutexLocker locker(&mutex);
        // Enforce 8-byte (stereo 32-bit float) alignment to prevent catastrophic float misreads
        qint64 avail = buffer.size();
        avail = (avail / 8) * 8; 
        qint64 toRead = qMin(maxlen, avail);
        if (toRead > 0) {
            memcpy(data, buffer.constData(), toRead);
            buffer.remove(0, toRead);
        }
        return toRead;
    }

    qint64 writeData(const char *data, qint64 len) {
        QMutexLocker locker(&mutex);
        buffer.append(data, len);
        // keep maximum 2 seconds of audio at 48k float (48000 * 4 * 2 = 384000 bytes)
        int maxSize = 384000;
        if (buffer.size() > maxSize) { 
             int excess = buffer.size() - maxSize;
             // CRITICAL: Ensure truncation is aligned to 8-byte frames to prevent permanent misalignment
             excess = ((excess / 8) + 1) * 8;
             buffer.remove(0, excess);
        }
        return len;
    }
    
    bool isSequential() const { return true; }
    
    qint64 bytesAvailable() const {
        QMutexLocker locker(&mutex);
        return buffer.size() + QIODevice::bytesAvailable();
    }
    
    void clear() {
        QMutexLocker locker(&mutex);
        buffer.clear();
    }
};

//--------------------------------------
AudioIO::AudioIO(int fsample, int inFrameRate, QWidget *parent)
  : QWidget(parent)
{
  started = false;

  fSample = fsample;
  frameRate = inFrameRate;

  // Replaced QBuffer with custom thread-safe ring buffer
  IODevice = new AudioRingBuffer(this);

  notifyInterval = -1;
  audioInput = NULL;
  
  // Watchdog timer: Emits notify() manually and revives broken CoreAudio streams
  pollTimer = new QTimer(this);
  connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollNotify()));

  initHW(QAudioDeviceInfo::defaultInputDevice());

  devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
}

AudioIO::~AudioIO()
{
  stop();
  if (audioInput != NULL)
    delete audioInput;

  IODevice->close();
}


void AudioIO::switchDevice(int deviceIndex)
{
  bool wasStarted = started;
  if (wasStarted)
    stop();

  if (deviceIndex >= devices.size() || deviceIndex < 0)
    {
      qWarning() << "Error: invalid device index. must be between 0 and " <<  devices.size() - 1;
      exit(1);
    }
  QAudioDeviceInfo devInfo = devices.at(deviceIndex);
  initHW(devInfo);

  if (wasStarted)
    start();
}


// Initializes new audioInput object
void AudioIO::initHW(QAudioDeviceInfo inDevice)
{
  if (audioInput != NULL) {
    stop();
    delete audioInput;
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
    qDebug() << "Sample type: " << st; // 3 == Float
  qDebug() << "Device name" << info.deviceName();

  audioFormat.setChannelCount(2); // Ask for stereo so we don't miss Input 2
  audioFormat.setSampleRate(fSample);
  audioFormat.setCodec("audio/pcm");
  // Use native byte order so audio samples are not byte-swapped on
  // big-endian hosts (PowerPC).  QSysInfo::ByteOrder reflects the
  // actual endianness of the CPU at compile time.
  audioFormat.setByteOrder(
      (QSysInfo::ByteOrder == QSysInfo::LittleEndian)
          ? QAudioFormat::LittleEndian
          : QAudioFormat::BigEndian);
  audioFormat.setSampleType(QAudioFormat::Float);
  audioFormat.setSampleSize(32);

  if (!info.isFormatSupported(audioFormat)) 
    {
      // Safely fall back to 1 channel before asking nearestFormat
      audioFormat.setChannelCount(1);
      if (!info.isFormatSupported(audioFormat)) {
          qWarning() << "Requested audio format is not supported. Using nearest available";
          audioFormat = info.nearestFormat(audioFormat);
          qWarning() << "Channels: " << audioFormat.channelCount();
          qWarning() << "Frequency: " << audioFormat.sampleRate();
          qWarning() << "Codec: " << audioFormat.codec();
          qWarning() << "Sample Size: " << audioFormat.sampleSize();
      }
    }

  int sType = audioFormat.sampleType();
  int sSize = audioFormat.sampleSize();
  bool isFloat32 = (sType == QAudioFormat::Float && sSize == 32);
  bool isInt16or32 = (sType == QAudioFormat::SignedInt && (sSize == 16 || sSize == 32));
  
  if (!isFloat32 && !isInt16or32) {
      qWarning("Only 32-bit float or 16/32-bit signed int samples are supported! Exiting");
      qWarning() << "Sample Type:" << sType << "Size:" << sSize;
      exit(1);
    }

  audioInput = new QAudioInput(info, audioFormat, this);
  if (notifyInterval > -1) 
    audioInput->setNotifyInterval(notifyInterval);
  
  connect(audioInput, SIGNAL(notify()), this, SIGNAL(notify()));

  // This causes Windows XP to seriously misbehave strange, but it crashed in Linux without it. Fun.
  if (BUILD_LINUX) 
    audioInput->setBufferSize(1000/frameRate);
}

QList<QAudioDeviceInfo>  AudioIO::getDevices()
{
  return devices;
}

void AudioIO::setNotifyInterval(int ms)
{
  notifyInterval = ms;
  if (audioInput != NULL && notifyInterval > -1) 
    audioInput->setNotifyInterval(notifyInterval);
  
  if (ms > 0)
    pollTimer->setInterval(ms);
}

void AudioIO::start()
{
  ((AudioRingBuffer*)IODevice)->clear();
  audioInput->start(IODevice);
  pollTimer->start();
  started = true;
}

void AudioIO::stop()
{
  pollTimer->stop();
  if (audioInput)
    audioInput->stop();
  started = false;
}

void AudioIO::pollNotify()
{
  if (started && audioInput) {
    // If CoreAudio stalls and enters IdleState when it shouldn't, forcefully revive it
    if (audioInput->state() == QAudio::IdleState) {
        qDebug() << "AudioIO: Stream went idle unexpectedly, forcing restart...";
        audioInput->stop();
        ((AudioRingBuffer*)IODevice)->clear();
        audioInput->start(IODevice);
    }
  }
  emit notify();
}

qint64 AudioIO::getAudio(float *inBuffer, int maxFrames)
{
  if (!started || !audioInput) return 0;

  QAudioFormat format = audioInput->format();
  int channels = format.channelCount();
  int sampleSize = format.sampleSize();
  const qint64 bytesPerSample = sampleSize / 8;
  const qint64 bytesPerFrame = bytesPerSample * channels;

  qint64 bytesAvailable = IODevice->bytesAvailable();
  qint64 bytesToRead = maxFrames * bytesPerFrame;
  
  if (bytesToRead > bytesAvailable)
      bytesToRead = (bytesAvailable / bytesPerFrame) * bytesPerFrame;
      
  if (bytesToRead <= 0) return 0;

  QByteArray rawData = IODevice->read(bytesToRead);
  qint64 actualBytes = rawData.size();
  qint64 framesRead = actualBytes / bytesPerFrame;
  
  if (framesRead <= 0) return 0;
  
  if (format.sampleType() == QAudioFormat::Float && sampleSize == 32) 
    {
      float *src = (float *)rawData.constData();
      for (int i = 0; i < framesRead; i++) {
          float sum = 0;
          for (int c = 0; c < channels; c++) {
              sum += src[i * channels + c];
          }
          inBuffer[i] = sum / channels;
      }
    }
  else if (sampleSize == 16) // 16-bit integer
    {
      qint16 *src = (qint16 *)rawData.constData();
      float scale = 1.0f / 32768.0f;
      for (int i = 0; i < framesRead; i++) {
          float sum = 0;
          for (int c = 0; c < channels; c++) {
              sum += src[i * channels + c] * scale;
          }
          inBuffer[i] = sum / channels;
      }
    }
  else if (sampleSize == 32) // 32-bit integer
    {
      qint32 *src = (qint32 *)rawData.constData();
      float scale = 1.0f / 2147483648.0f; // 2^31
      for (int i = 0; i < framesRead; i++) {
          float sum = 0;
          for (int c = 0; c < channels; c++) {
              sum += src[i * channels + c] * scale;
          }
          inBuffer[i] = sum / channels;
      }
    }
  
  return framesRead;
}
