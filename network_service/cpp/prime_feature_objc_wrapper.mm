//
//  prime_feature_objc_wrapper.m
//  TinyVpn
//
//  Created by Hench on 7/3/20.
//  Copyright © 2020 Hench. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "prime_feature_objc_wrapper.h"
#include "prime_feature.hpp"
@interface PrimeChecker() {
    prime_checker* checker;
}
@end

@implementation PrimeChecker
- (instancetype)init {
    if (self = [super init]) {
        checker = new prime_checker();
    }
    return self;
}

- (void)dealloc {
    delete checker;
}

- (void)simpleCall: (void(*)())function  {
    checker->simpleCall(function);
}

- (void) checkIsPrime: (NSString *)value with: (void(*)(void*)) progressCallback andWith: (void(*)(bool result, void* target)) resultCallback withTarget: (void*) target{
    checker->checkIsPrime([value UTF8String], progressCallback, resultCallback, target);
}
- (int) mycheck: (NSInteger) value with: (NSInteger*) v {
    return checker->mycheck(value, (int*)v);
}
@end
