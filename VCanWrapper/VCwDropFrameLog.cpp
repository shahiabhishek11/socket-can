/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwDropFrameLog.cpp
 * Class and functions for logging dropped count of dropped CAN frames
 * -------------------------------------------------
 *
 */

#include "VCwDropFrameLog.h"
#include "VCanWrapper.h"

#define DROP_FRAME_LOG_PERIOD   10 //in seconds

static VCwDropFrameLog *pTheDropFrameLog = 0;

///////////////////////////////////////////////////////////////
// logTimerCallback
// @brief  This function logs dropped frame count
// @param  pointer to droplog instance
// @returns none
//
void logTimerCallback(void * pArg) {
    VCwDropFrameLog *pLog = (VCwDropFrameLog *) pArg;
    if (pLog != NULL) {
        CAN_LOGE("Dropped %d frames", pLog->mDropFrameCount);
        pLog->mDropFrameCount = 0;
        pLog->mTimerRunning = false;
    }
}
///////////////////////////////////////////////////////////////
// VCwDropFrameLog - CONSTRUCTor
//
VCwDropFrameLog::VCwDropFrameLog() {
    logTimerInit();
}
///////////////////////////////////////////////////////////////
// logTimerInit
// @brief  This function creates timer
// @param none
// @returns 0 if timer is initialzed
//
int VCwDropFrameLog::logTimerInit() {
    //Create timer to log count of dropped frames
    mDropFrameCount = 0;
    mTimerRunning = false;
    struct sigevent se;
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_value.sival_ptr = this;
    se.sigev_notify_function = (void (*)(union sigval))logTimerCallback;
    se.sigev_notify_attributes = NULL;
    int status = timer_create(CLOCK_REALTIME, &se, &mTimerId);
    if (status == -1) {
        CAN_LOGE("Timer creation for logging dropped frames failed");
        return -1;
    }
    return 0;
}
//////////////////////////////////////////////////////////////////
// getInstance
// @brief This function returns pointer to VCwDropFrameLog instance
// @param ifNo - an interface index
// @return pointer at the VCwDropFrameLog singleton
//
VCwDropFrameLog * VCwDropFrameLog::getInstance() {
    if (!pTheDropFrameLog) {
        pTheDropFrameLog = new VCwDropFrameLog();
    }
    return pTheDropFrameLog;
}
///////////////////////////////////////////////////////////////
// updateDropFrameCount
// @brief This function increments counter of dropped frames
// @param none
// @returns 0 if counter is updated
// @desc timer is set if it is not running
//
int VCwDropFrameLog::updateDropFrameCount() {
    if (mTimerRunning == false) {
        struct itimerspec ts;
        ts.it_value.tv_sec = DROP_FRAME_LOG_PERIOD;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        int status = timer_settime(mTimerId, 0, &ts, 0);
        if (status == -1) {
            CAN_LOGE("Set timer error");
            return -1;
        }
        mTimerRunning = true;
    }
    mDropFrameCount++;
    return 0;
}
