/*	Copyright: 	© Copyright 2004 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	AudioDevice.cpp
	
=============================================================================*/

#include "AudioDevice.h"

// macOS 10.13 headers renamed the macro to __Verify_noErr().
#if defined(MAC_OS_X_VERSION_10_13)
#define verify_noerr __Verify_noErr
#endif

void	AudioDevice::Init(AudioDeviceID devid, bool isInput)
{
	mID = devid;
	mIsInput = isInput;
	if (mID == kAudioDeviceUnknown) return;
	
	UInt32 propsize;
	
	propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertySafetyOffset, &propsize, &mSafetyOffset));
	
	propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyBufferFrameSize, &propsize, &mBufferSizeFrames));
	
	propsize = sizeof(AudioStreamBasicDescription);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyStreamFormat, &propsize, &mFormat));

}

void	AudioDevice::SetBufferSize(UInt32 size)
{
	UInt32 propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceSetProperty(mID, NULL, 0, mIsInput, kAudioDevicePropertyBufferFrameSize, propsize, &size));

	propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyBufferFrameSize, &propsize, &mBufferSizeFrames));
}

int		AudioDevice::CountChannels()
{
	OSStatus err;
	UInt32 propSize;
	int result = 0;
	
	err = AudioDeviceGetPropertyInfo(mID, 0, mIsInput, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
	if (err) return 0;

	AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);
	err = AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyStreamConfiguration, &propSize, buflist);
	if (!err) {
		for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i) {
			result += buflist->mBuffers[i].mNumberChannels;
		}
	}
	free(buflist);
	return result;
}

char *	AudioDevice::GetName(char *buf, UInt32 maxlen)
{
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyDeviceName, &maxlen, buf));
	return buf;
}
