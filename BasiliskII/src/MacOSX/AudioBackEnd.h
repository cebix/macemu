/*
 *
 * This is based on Apple example software AudioBackEnd.cpp
 * 
 * Copyright © 2004 Apple Computer, Inc., All Rights Reserved
 * Original Apple code modified by Daniel Sumorok
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */ 

#ifndef __AudioBackEnd_H__
#define __AudioBackEnd_H__

#define checkErr( err) \
if(err) {\
        OSStatus error = static_cast<OSStatus>(err);\
           fprintf(stderr, "AudioBackEnd Error: %ld ->  %s:  %d\n",  error,\
                           __FILE__, \
                           __LINE__\
                           );\
                                   fflush(stdout);\
}         

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <pthread.h>
#include "AudioDevice.h"

typedef void (*playthruCallback)(void *arg);

class AudioBackEnd  {
 public:
  AudioBackEnd(int bitsPerSample, int numChannels, int sampleRate);
  ~AudioBackEnd();
  OSStatus Init();
  OSStatus Start();
  OSStatus Stop();
  Boolean IsRunning();
  void setCallback(playthruCallback func, void *arg);
  UInt32 BufferSizeFrames();
  int sendAudioBuffer(void *buffer, int numFrames);
 private:
  OSStatus SetupGraph();
  OSStatus CallbackSetup();
  OSStatus SetupBuffers();

  static OSStatus OutputProc(void *inRefCon,
                             AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *inTimeStamp,
                             UInt32 inBusNumber,
                             UInt32 inNumberFrames,
                             AudioBufferList *  ioData);

  AudioDevice mOutputDevice;

  AUGraph mGraph;
  AUNode mOutputNode;
  AudioUnit mOutputUnit;
  int mBitsPerSample;
  int mSampleRate;
  int mNumChannels;     
  playthruCallback mCallback;
  void *mCallbackArg;
  UInt32 mBufferSizeFrames;
  UInt32 mFramesProcessed;
  UInt8 *mAudioBuffer;
  UInt32 mAudioBufferWriteIndex;
  UInt32 mAudioBufferReadIndex;
  UInt32 mBytesPerFrame;
  UInt32 mAudioBufferSize;
};

#endif //__AudioBackEnd_H__
