//
//  NNThread.m -Not Nextstep Thread?
//				Nigel's Nice Thread?
//
//	Revision 1.4, Tuesday May 25 2004
//
//  Created by Nigel Pearson on Tue Nov 28 2000.
//  Public Domain. No rights reserved.
//

#import "NNThread.h"
#import <objc/objc-runtime.h>	// For objc_msgSend() prototype

@implementation NNThread

// Define the wrapper first so that init knows about it without having to put it in the .h

#ifdef USE_NSTHREAD
- (void) wrapper
{
	machThread = mach_thread_self();

 	if ( object == nil || sel == (SEL) nil || suspended )
		thread_suspend (machThread);						// Suspend myself

	if ( allocPool )
		pool = [NSAutoreleasePool new];

	// [object sel] caused "cannot find method" warnings, so I do it a non-obvious way:
	objc_msgSend (object, sel);

	completed = YES;

	if ( allocPool )
		[pool release];
}
#endif

#ifdef USE_PTHREAD
void *
pthreadWrapper (void *arg)
{
	struct pthreadArgs	*args = arg;

	if ( args -> allocPool )
		args -> pool = [NSAutoreleasePool new];

	objc_msgSend (*(args->object), *(args->sel));

	*(args->completed)  = YES;

	if ( args -> allocPool )
		[args -> pool release];

	return NULL;
}

- (BOOL) wrapper
{
	int	error;

	pthreadArgs.object		= &object;
	pthreadArgs.sel			= &sel;
	pthreadArgs.allocPool	= allocPool;
	pthreadArgs.completed	= &completed;
	pthreadArgs.pool		= nil;

 	if ( object == nil || sel == (SEL) nil || suspended )
		error = pthread_create_suspended_np (&pThread, NULL, &pthreadWrapper, &pthreadArgs);
	else
		error = pthread_create (&pThread, NULL, &pthreadWrapper, &pthreadArgs);

	if ( error )
		NSLog(@"%s - pthread_create failed");
	else
		machThread = pthread_mach_thread_np (pThread);

	return ! error;
}
#endif

- (NNThread *) initSuspended: (BOOL) startSuspended
		 withAutoreleasePool: (BOOL) allocatePool
{
	self = [super init];

	allocPool = allocatePool;
	completed = NO;
	suspended = startSuspended;
	object = nil;
	sel = (SEL) nil;

#ifdef USE_NSTHREAD
	[NSThread detachNewThreadSelector:@selector(wrapper)
							 toTarget:self
						   withObject:nil];
#endif

#ifdef USE_PTHREAD
	if ( ! [self wrapper] )		// Wrapper does the thread creation
	{
		NSLog(@"%s - pthread wrapper failed", __PRETTY_FUNCTION__);
		return nil;
	}
#endif

	return self;
}

- (NNThread *) init
{
	return [self initSuspended: YES withAutoreleasePool: NO];
}

- (NNThread *) initWithAutoReleasePool
{
	return [self initSuspended: YES withAutoreleasePool: YES];
}

- (BOOL) completed
{
	return completed;
}

- (void) perform: (SEL)action	of: (id)receiver
{
	object = receiver, sel = action;
}

- (void) resume
{
	if ( suspended )
		[self start];
	else
		NSLog (@"%s - thread not suspended", __PRETTY_FUNCTION__);
}

- (BOOL) start
{
	kern_return_t	error; 

	if ( object == nil || sel == (SEL) nil )
	{
		NSLog (@"%s - cannot start thread, object or selector invalid", __PRETTY_FUNCTION__);
		return NO;
	}
	if ( ( error = thread_resume (machThread) ) != KERN_SUCCESS )
		NSLog (@"%s - thread_resume() failed, returned %d", __PRETTY_FUNCTION__, error);
	suspended = NO;
	return YES;
}

- (void) suspend
{
	if ( ! suspended )
	{
		kern_return_t	error; 

		if ( ( error = thread_suspend (machThread) ) != KERN_SUCCESS )
			NSLog (@"%s - thread_resume() failed, returned %d", __PRETTY_FUNCTION__, error);
		suspended = YES;
	}
}

- (void) terminate
{
	kern_return_t	error; 

	if ( ( error = thread_terminate (machThread) ) != KERN_SUCCESS )
		NSLog (@"%s - thread_terminate() failed, returned %d", __PRETTY_FUNCTION__, error);
}

@end

@implementation NNTimer

- (NNTimer *) init
{
	self = [super init];
	repeating = YES;
	return self;
}

- (NNTimer *) initWithAutoRelPool
{
	self = [super init];
	allocPool = YES;
	repeating = YES;
	return self;
}


- (void) changeIntervalTo: (int)number
					units: (NNTimeUnits)units
{
	switch ( units )
	{
		case NNnanoSeconds:
			delay.tv_nsec = number;
			delay.tv_sec = 0;
			break;
		case NNmicroSeconds:
			delay.tv_nsec = number * 1000;
			delay.tv_sec = 0;
			break;
		case NNmilliSeconds:
			delay.tv_nsec = number * 1000000;
			delay.tv_sec = 0;
			break;
		case NNseconds:
			delay.tv_nsec = 0;
			delay.tv_sec = number;
			break;
		default:
			NSLog (@"%s illegal units(%d)", __PRETTY_FUNCTION__, units);
	}
}

- (void) invalidate
{
	repeating = NO;
}

- (void) timerLoop
{
	// For some strange reason, Mac OS X does not have this prototype
	extern	int	nanosleep (const struct timespec *rqtp, struct timespec *rmtp);

	while ( repeating )
	{
		nanosleep(&delay, NULL);
		completed = NO;
		// This caused a few warnings:
		// [timerObject timerSel];
		// so I do it a non-obvious way:
		objc_msgSend (timerObject, timerSel);
		completed = YES;
	}
}

- (void) perform: (SEL)action    of: (id)receiver
		   after: (int)number units: (NNTimeUnits)units
{
	object = self, sel = @selector(timerLoop),
	timerObject = receiver, timerSel = action, repeating = NO;
	[self changeIntervalTo: number	units: units];
}

- (void) repeat: (SEL)action    of: (id)receiver
		  every: (int)number units: (NNTimeUnits)units
{
	object = self, sel = @selector(timerLoop),
	timerObject = receiver, timerSel = action, repeating = YES;
	[self changeIntervalTo: number	units: units];
}

@end