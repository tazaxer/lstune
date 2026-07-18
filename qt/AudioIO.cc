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
#include "noteGuesser.h"

//--------------------------------------
// Constants
//--------------------------------------
// Maximum buffer length in seconds
const double maxBuffLength = 2.0;

//--------------------------------------
AudioIO::AudioIO(int fsample, int inFrameRate, QWidget *parent)
  : QWidget(parent)
{
  started = false;

  fSample = fsample;
  frameRate = inFrameRate;

  IODevice = new QBuffer();
  if (!IODevice->open(QIODevice::ReadWrite))
    qWarning("Unable to open IODevice buffer");
  readPointer = 0; 

  notifyInterval = -1;
  audioInput = NULL;
  initHW(QAudioDeviceInfo::defaultInputDevice());

  devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
}

AudioIO::~AudioIO()
{
  stop();
  if (audioInput != NULL)
    delete audioInput;

  IODevice->close();
  delete IODevice;
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

  audioFormat.setChannelCount(1); // mono sound
  audioFormat.setSampleRate(fSample); // mono sound
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
      qWarning() << "Default audio format is not supported. Using nearest available";
      audioFormat = info.nearestFormat(audioFormat);
      qWarning() << "Channels: " << audioFormat.channelCount();
      qWarning() << "Frequency: " << audioFormat.sampleRate();
      qWarning() << "Codec: " << audioFormat.codec();
      qWarning() << "Sample Size: " << audioFormat.sampleSize();
    }

  int sType = audioFormat.sampleType();
  if ( (sType != QAudioFormat::Float ) || (audioFormat.sampleSize() != 32)) {
      qWarning("Only floats or 32/16 bit integer samples are supported! Exiting");
      qWarning() << "Sample Type:" << sType;
      exit(1);
    }

  audioInput = new QAudioInput(info, audioFormat, this);
  if (notifyInterval > -1) 
    audioInput->setNotifyInterval(notifyInterval);
  connect(audioInput, SIGNAL(notify()), this, SIGNAL(notify()));

  // This causes Windows XP to seriously misbehave strange, but it crashed in Linux without it. Fun.
  if (BUILD_LINUX) 
    audioInput->setBufferSize(1000/frameRate);

  // Max buffer size in bytes
  maxBufSize = qint64(maxBuffLength * audioFormat.sampleRate() * audioFormat.sampleSize()/8);
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

}

void AudioIO::start()
{
  IODevice->seek(0);
  readPointer = 0; 
  audioInput->start(IODevice);
  started = true;
}

void AudioIO::stop()
{
  audioInput->stop();
  started = false;
}

qint64 AudioIO::getAudio(float *inBuffer, int maxSamples)
{
  QAudioFormat format = audioInput->format();
  qint64 readSamples;

  // Size of sample data in bytes
  const qint64 dataSize = format.sampleSize()/8;

  qint64 writePointer = IODevice->pos();
  qint64 bytesToRead = writePointer - readPointer;
  // Number of sampes to get. Max of available sampels, or requested samples
  qint64 samplesToRead = bytesToRead/dataSize;
  // qDebug() << "Samples available:" << samplesToRead << "Max:" << maxSamples << "Pos:" << writePointer;
  bool readAll = true;
  if (samplesToRead > maxSamples)
    {
      samplesToRead = maxSamples;
      bytesToRead = maxSamples * dataSize;
      readAll = false;
    }
  else
    writePointer = 0; // We're reading all available data. Reset write pointer to zero

  IODevice->seek(readPointer);
  if (format.sampleType() == QAudioFormat::Float) // Float. No conversion to do
    readSamples = IODevice->read((char *)inBuffer, bytesToRead)/dataSize;
  else // 32 or 16 bit int
    {
      // Scale factor for float conversion
      float scale = float( 1.0/(2<<(dataSize*8 - 2)) );

      // QVarLengthArray is a C++98-compatible, Qt4-available alternative
      // to a C99 VLA.  It uses the stack for small sizes and falls back to
      // the heap for large ones, avoiding undefined stack overflow.
      QVarLengthArray<qint32> intBuf(maxSamples);
      readSamples = IODevice->read((char *)intBuf.data(), bytesToRead)/dataSize;

      // Convert to float, write to buffer
      for (int i = 0; i < readSamples; i++)
    inBuffer[i] = float (intBuf[i] * scale);
    }
  
  // Reset buffer if its grown too large, or we've read all the data
  if (readAll || IODevice->size() > maxBufSize) 
    {
      IODevice->seek(0);
      readPointer = 0;
    }
  else
    {
      IODevice->seek(writePointer);
      readPointer += bytesToRead;
    }
  return readSamples;
}

