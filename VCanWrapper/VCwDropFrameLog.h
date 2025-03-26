/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwDropFrameLog.h
 * This is used to log count of dropped frames
 */

#ifndef VCwDropFrameLog_H_
#define VCwDropFrameLog_H_
#include "VCwBase.h"

#include <time.h>
#include <signal.h>

class VCwDropFrameLog {
private:
    int mDropFrameCount;
    bool mTimerRunning;
    timer_t mTimerId;
    VCwDropFrameLog();
    int logTimerInit();
public:
    int updateDropFrameCount();
    static VCwDropFrameLog * getInstance();
protected:
    friend void logTimerCallback(void * pArg); //timer handler to log dropped frames
};
#endif /* VCwDropFrameLog_H_ */
