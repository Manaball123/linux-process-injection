#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/syscall.h>
#include "../shared/utils.h"




void __attribute__((constructor)) init() {
    std::cout << "hello from injected module!\n";

    return; 
}