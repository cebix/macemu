//
//  NNThread.h -Not Nextstep Thread?
//				Nigel's Nice Thread?
//
// 	Revision 1.2, Tuesday May 25 2004
//
//  Created by Nigel Pearson on Tue Nov 28 2000.
//  Public Domain. No rights reserved.
//

// Define what flavour of threading to use:
#define USE_NSTHREAD
//#define USE_PTHREAD

#import <Cocoa/Cocoa.h>

#import <mach/mach.h>
#import <mach/kern_return.h>

#ifdef USE_PTHREAD
#include <pthread.h>

struct pthreadArgs		// This duplicates most of the stuff in the NNThread object
{
	id					*object;
	SEL					*sel;

	NSAutoreleasePool	*pool;	
	BOOL				allocPool,
						*completed;
};
#endif

@interface NNThread : NSObject
{
	id					object;
	SEL					sel;
	thread_t			machThread;
#ifdef USE_PTHREAD
	pthread_t			pThread;
	struct pthreadArgs	pthreadArgs;
#endif
	NSAutoreleasePool	*pool;
	BOOL				allocPool,
						completed,
						suspended;
}

- (NNThread *) initWithAutoReleasePool;
- (NNThread *) initSuspended: (BOOL) startSuspended
		 withAutoreleasePool: (BOOL) allocatePool;

- (void) perform: (SEL)action	of: (id)receiver;
- (void) resume;
- (BOOL) start;
- (void) suspend;
- (void) terminate;

@end

typedef enum _NNTimeUnits
{
	NNnanoSeconds  = 1,
	NNmicroSeconds = 2,
	NNmilliSeconds = 3,
	NNseconds = 4
}
NNTimeUnits;

#import <sys/time.h>

@interface NNTimer : NNThread
{
	struct timespec	delay;
	BOOL			repeating;
	id				timerObject;
	SEL				timerSel;
}

- (NNTimer *) initWithAutoRelPool;

- (void) changeIntervalTo: (int)number	units: (NNTimeUnits)units;
- (void) invalidate;

- (void) perform: (SEL)action     of: (id)receiver
		   after: (int)number  units: (NNTimeUnits)units;

- (void) repeat: (SEL)action      of: (id)receiver
		  every: (int)number   units: (NNTimeUnits)units;

@end