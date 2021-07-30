//
//  prime_feature_objc_wrapper.h
//  TinyVpn
//
//  Created by Hench on 7/3/20.
//  Copyright © 2020 Hench. All rights reserved.
//

#ifndef prime_feature_objc_wrapper_h
#define prime_feature_objc_wrapper_h
@interface PrimeChecker : NSObject
- (void) simpleCall: (void(*)())function;
- (void) checkIsPrime: (NSString *)value with: (void(*)(void*)) progressCallback andWith: (void(*)(bool result, void* target)) resultCallback withTarget: (void*) target ;
- (void) mycheck: (NSInteger)value with:(NSInteger*) v;
@end


#endif /* prime_feature_objc_wrapper_h */
