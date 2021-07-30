//
//  prime_feature.h
//  TinyVpn
//
//  Created by Hench on 7/3/20.
//  Copyright © 2020 Hench. All rights reserved.
//

#ifndef prime_feature_h
#define prime_feature_h
#ifdef __cplusplus

#include <iostream>
#include <future>
#include <chrono>
#include <string>

class prime_checker {
public:
    static bool is_prime (int x) {
        for (int i = 2; i < x; ++i) if (x % i == 0) return false;
        return true;
    }
    
    void checkIsPrime(std::string value, void(*progressCallback)(void*), void(*resultCallback)(bool result, void* target), void* target) {
        std::cout << "In c++ " << value << std::endl;
        std::future<bool> fut = std::async (std::launch::async, is_prime, std::stoi(value));
        std::chrono::milliseconds span (10000);
        while (fut.wait_for(span)==std::future_status::timeout)
            progressCallback(target);
        bool x = fut.get();
        resultCallback(x, target);
        //sleep(10);
        std::cout << value << (x ? " is" : " is not") << " prime.\n";
    }
    
    void simpleCall(void(*function)()){
        std::cout << "Down to c++" << std::endl;
        function();
    }
    int mycheck(int value, int* v) {
        *v =123;
        std::cout<<"mycheck:"<<value<<std::endl;
        return 0;
    }
};


#endif /* prime_feature_h */

#endif /* prime_feature_h */
