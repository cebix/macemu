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

#include "AudioBackEnd.h"

#pragma mark ---Public Methods---

AudioBackEnd::AudioBackEnd(int bitsPerSample, int numChannels, int sampleRate):
  mBitsPerSample(bitsPerSample),
  mSampleRate(sampleRate),
  mNumChannels(numChannels),
  mCallback(NULL),
  mCallbackArg(NULL),
  mBufferSizeFrames(0),
  mFramesProcessed(0),
  mAudioBuffer(NULL),
  mAudioBufferWriteIndex(0),
  mAudioBufferReadIndex(0),
  mBytesPerFrame(0),
  mAudioBufferSize(0) {
  OSStatus err = noErr;
  err = Init();
  if(err) {
    fprintf(stderr,"AudioBackEnd ERROR: Cannot Init AudioBackEnd");
    exit(1);
  }
}

AudioBackEnd::~AudioBackEnd() {   //clean up
  Stop();

  AUGraphClose(mGraph);
  DisposeAUGraph(mGraph);
        
  if(mAudioBuffer != NULL) {
    delete mAudioBuffer;
    mAudioBuffer = NULL;
    mAudioBufferSize = 0;
  }
}

OSStatus AudioBackEnd::Init() {
  OSStatus err = noErr;

  err = SetupGraph();   
  checkErr(err);

  err = SetupBuffers();
  checkErr(err);

  err = AUGraphInitialize(mGraph); 
  checkErr(err);

  return err;   
}

#pragma mark --- Operation---

OSStatus AudioBackEnd::Start()
{
  OSStatus err = noErr;
  if(!IsRunning()) {
    mFramesProcessed = 0;
    mAudioBufferWriteIndex = 0;         
    mAudioBufferReadIndex = 0;
                
    err = AUGraphStart(mGraph);
  }
  return err;   
}

OSStatus AudioBackEnd::Stop() {
  OSStatus err = noErr;

  if(IsRunning()) {
    err = AUGraphStop(mGraph);
  }
  return err;
}

Boolean AudioBackEnd::IsRunning() {     
  OSStatus err = noErr;
  Boolean graphRunning;

  err = AUGraphIsRunning(mGraph,&graphRunning);
        
  return (graphRunning);        
}

#pragma mark -
#pragma mark --Private methods---
OSStatus AudioBackEnd::SetupGraph() {
  OSStatus err = noErr;
  AURenderCallbackStruct output;
  UInt32 size;
  ComponentDescription outDesc;
  AudioDeviceID out;

  //Make a New Graph
  err = NewAUGraph(&mGraph);  
  checkErr(err);

  //Open the Graph, AudioUnits are opened but not initialized    
  err = AUGraphOpen(mGraph);
  checkErr(err);
  
  outDesc.componentType = kAudioUnitType_Output;
  outDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
  outDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
  outDesc.componentFlags = 0;
  outDesc.componentFlagsMask = 0;
        
  //////////////////////////
  ///MAKE NODES
  //This creates a node in the graph that is an AudioUnit, using
  //the supplied ComponentDescription to find and open that unit        
  err = AUGraphNewNode(mGraph, &outDesc, 0, NULL, &mOutputNode);
  checkErr(err);
        
  //Get Audio Units from AUGraph node
  err = AUGraphGetNodeInfo(mGraph, mOutputNode, NULL, NULL, NULL, &mOutputUnit);         
  checkErr(err);
        
  err = AUGraphUpdate(mGraph, NULL);
  checkErr(err);

  size = sizeof(AudioDeviceID);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                 &size, &out);
  checkErr(err);
  mOutputDevice.Init(out, false);
        
  //Set the Current Device to the Default Output Unit.
  err = AudioUnitSetProperty(mOutputUnit,
                             kAudioOutputUnitProperty_CurrentDevice, 
                             kAudioUnitScope_Global, 
                             0, 
                             &out, 
                             sizeof(out));
  checkErr(err);
                        
  output.inputProc = OutputProc;
  output.inputProcRefCon = this;
        
  err = AudioUnitSetProperty(mOutputUnit, 
                             kAudioUnitProperty_SetRenderCallback, 
                             kAudioUnitScope_Input,
                             0,
                             &output, 
                             sizeof(output));
  checkErr(err);                                          
  return err;
}

//Allocate Audio Buffer List(s) to hold the data from input.
OSStatus AudioBackEnd::SetupBuffers() {
  OSStatus err = noErr;
  UInt32 safetyOffset;
  AudioStreamBasicDescription asbd;
  UInt32 propertySize;

  propertySize = sizeof(mBufferSizeFrames);
  err = AudioUnitGetProperty(mOutputUnit, kAudioDevicePropertyBufferFrameSize, 
                             kAudioUnitScope_Global, 0, &mBufferSizeFrames, 
                             &propertySize);

  propertySize = sizeof(safetyOffset);
  safetyOffset = 0;
  err = AudioUnitGetProperty(mOutputUnit, kAudioDevicePropertySafetyOffset, 
                             kAudioUnitScope_Global, 0, &safetyOffset, 
                             &propertySize);
                             

  asbd.mFormatID = 0x6c70636d; // 'lpcm'
  asbd.mFormatFlags = (kAudioFormatFlagIsSignedInteger |
                       kAudioFormatFlagIsBigEndian |
                       kAudioFormatFlagIsPacked);
  asbd.mChannelsPerFrame = mNumChannels;
  asbd.mSampleRate = mSampleRate;
        
  if(asbd.mFormatFlags & kAudioFormatFlagIsSignedInteger) {
    asbd.mBitsPerChannel = mBitsPerSample;
  } else if(asbd.mFormatFlags & kAudioFormatFlagIsFloat)        {
    asbd.mBitsPerChannel = 32;
  } else {
    asbd.mBitsPerChannel = 0;
  }

  asbd.mFramesPerPacket = 1;
  asbd.mBytesPerFrame = (asbd.mBitsPerChannel / 8) * asbd.mChannelsPerFrame;
  asbd.mBytesPerPacket = asbd.mBytesPerFrame * asbd.mFramesPerPacket;

  asbd.mReserved = 0;

  mBytesPerFrame = asbd.mBytesPerFrame;
  if((mBytesPerFrame & (mBytesPerFrame - 1)) != 0) {
    printf("Audio buffer size must be a power of two!\n");
    return -1;
  }

  propertySize = sizeof(asbd);
  err = AudioUnitSetProperty(mOutputUnit, kAudioUnitProperty_StreamFormat, 
                             kAudioUnitScope_Input, 0, &asbd, propertySize);
  checkErr(err);

  if(mAudioBuffer != NULL) {
    delete mAudioBuffer;
    mAudioBufferSize = 0;
  }

  mAudioBufferSize = mBytesPerFrame * mBufferSizeFrames * 2;
  mAudioBuffer = new UInt8[mAudioBufferSize];
  bzero(mAudioBuffer, mAudioBufferSize);
  return err;
}

#pragma mark -
#pragma mark -- IO Procs --
OSStatus AudioBackEnd::OutputProc(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *TimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList * ioData) {
  OSStatus err = noErr;
  AudioBackEnd *This = (AudioBackEnd *)inRefCon;
  UInt8 *dstPtr;
  UInt32 bytesToCopy;
  UInt32 bytesUntilEnd;

  This->mFramesProcessed += inNumberFrames;

  dstPtr = (UInt8 *)ioData->mBuffers[0].mData;
  if(This->mAudioBuffer == NULL) {
    bzero(dstPtr, inNumberFrames * This->mBytesPerFrame);
    return noErr;
  }

  bytesToCopy = inNumberFrames * This->mBytesPerFrame;
  bytesUntilEnd = This->mAudioBufferSize - This->mAudioBufferReadIndex;
  if(bytesUntilEnd < bytesToCopy) {
    memcpy(dstPtr, &This->mAudioBuffer[This->mAudioBufferReadIndex], 
           bytesUntilEnd);
    memcpy(dstPtr, This->mAudioBuffer, bytesToCopy - bytesUntilEnd);

    This->mAudioBufferReadIndex = bytesToCopy - bytesUntilEnd;
  } else {
    memcpy(dstPtr, &This->mAudioBuffer[This->mAudioBufferReadIndex], 
           bytesToCopy);
    This->mAudioBufferReadIndex += bytesToCopy;
  }


  while(This->mFramesProcessed >= This->mBufferSizeFrames) {
    This->mFramesProcessed -= This->mBufferSizeFrames;
    if(This->mCallback != NULL) {
      This->mCallback(This->mCallbackArg);
    }
  }

  return err;
}

void AudioBackEnd::setCallback(playthruCallback func, void *arg) {
  mCallback = func;
  mCallbackArg = arg;
}

UInt32 AudioBackEnd::BufferSizeFrames() {
  return mBufferSizeFrames;
}

int AudioBackEnd::sendAudioBuffer(void *buffer, int numFrames) {
  UInt8 *dstBuffer;
  int totalBytes;
        
  mAudioBufferWriteIndex += (mAudioBufferSize / 2);
  mAudioBufferWriteIndex &= (mAudioBufferSize - 1);

  dstBuffer = &mAudioBuffer[mAudioBufferWriteIndex];
  totalBytes = mBytesPerFrame * numFrames;
  memcpy(dstBuffer, buffer, totalBytes);

  dstBuffer += totalBytes;
  bzero(dstBuffer, (mBufferSizeFrames * mBytesPerFrame) - totalBytes);

  return numFrames;
}
