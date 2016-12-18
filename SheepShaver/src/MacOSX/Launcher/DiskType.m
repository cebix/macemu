//
//  DiskType.m
//  SheepShaver
//
//  Created by maximilian on 01.02.14.
//  Copyright 2014 __MyCompanyName__. All rights reserved.
//

#import "DiskType.h"


@implementation DiskType
-(NSString*)path
{
	return _path;
}
-(BOOL)isCDROM
{
	return _isCDROM;
}

-(void)setPath:(NSString*)thePath
{
	_path = [thePath copy];
}
-(void)setIsCDROM:(BOOL)cdrom
{
	_isCDROM=cdrom;
}

-(NSString*)description {
	return [NSString stringWithFormat:@"DiskType, path:%@ isCDROM:%hhd", _path, _isCDROM];
}

@end
