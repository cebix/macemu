/*
 *  MacOSX_sound_if.h
 *  BasiliskII
 *
 *  Copyright 2006 Daniel Sumorok. All rights reserved.
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
#include "MacOSX_sound_if.h"

OSXsoundOutput::OSXsoundOutput() :
  player(NULL),
  callback(NULL) {
}

void OSXsoundOutput::getMoreSamples(void *arg) {
  OSXsoundOutput *me;
        
  me = (OSXsoundOutput *)arg;
        
  if(me == NULL) {
    return;
  }
        
  if(me->callback == NULL) {
    return;
  }
        
  me->callback();
}

int OSXsoundOutput::start(int bitsPerSample, int numChannels, int sampleRate) {
  stop();
  player = new AudioBackEnd(bitsPerSample, numChannels, sampleRate);
  if(player != NULL) {
    player->setCallback(getMoreSamples, (void *)this);
    player->Start();
  }
  return 0;
}

int OSXsoundOutput::stop() {
  if(player != NULL) {
    player->Stop();
    delete player;
    player = NULL;
  }
  return 0;
}

OSXsoundOutput::~OSXsoundOutput() {
  stop();
}

void OSXsoundOutput::setCallback(audioCallback fn) {
  callback = fn;
}

unsigned int OSXsoundOutput::bufferSizeFrames() {
  if(player != NULL) {
    return player->BufferSizeFrames();
  }
        
  return 0;
}

int OSXsoundOutput::sendAudioBuffer(void *buffer, int numFrames) {
  if(player != NULL) {
    return player->sendAudioBuffer(buffer, numFrames);
  }
        
  return 0;
}
