//
//  DiskType.h
//  SheepShaver
//
//  Created by maximilian on 01.02.14.
//  Copyright 2014 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface DiskType : NSObject {
	NSString* _path;
	BOOL _isCDROM;
}

-(NSString*)path;
-(BOOL)isCDROM;

-(void)setPath:(NSString*)thePath;
-(void)setIsCDROM:(BOOL)cdrom;

@end
