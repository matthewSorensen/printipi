#ifndef DRIVERS_AUTO_CHRONOCLOCK_H
#define DRIVERS_AUTO_CHRONOCLOCK_H

#include "common/typesettings/compileflags.h"

#ifdef PLATFORM_DRIVER_CHRONOCLOCK
    #include PLATFORM_DRIVER_CHRONOCLOCK
    typedef drv::TARGET_PLATFORM_LOWER::ChronoClock EventClockT;
#else
    #ifdef COMPILING_MAIN
        #warning "using ChronoClockPosix for EventClockT. While this does work, you will get better performance if you use a clock specific to your machine (eg make DTARGET_RPI=1 for the Raspberry Pi)"
    #endif
    #include "drivers/generic/chronoclock.h"
    typedef drv::generic::ChronoClock EventClockT;
#endif

#endif
