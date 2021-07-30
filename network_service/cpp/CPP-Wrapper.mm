//
//  CPP-Wrapper.m
//  TinyVpn
//
//  Created by Hench on 7/3/20.
//  Copyright © 2020 Hench. All rights reserved.
//


#import "CPP-Wrapper.h"
#include "CPP.hpp"
CPP cpp;
@implementation CPP_Wrapper
- (void)hello_cpp_wrapped:(NSString *)name {
    cpp.hello_cpp([name cStringUsingEncoding:NSUTF8StringEncoding]);
}
@end
