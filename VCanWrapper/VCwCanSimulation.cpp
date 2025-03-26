/*
*Copyright (c) 2015 Qualcomm Technologies, Inc.
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "VCwCanSimulation.h"
#include "VCwEarSet.h"
#include "VCwFrame.h"

#include <linux/can.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include <sys/time.h>

//#undef  LOG_TAG
//#define LOG_TAG "**CAN-SIMULATION**"

#define CAN_SIMULATION_THREAD_ID 0xFF

#define CHECK_TOKEN_AND_BREAK_IF_NULL(token) {\
    if (token == 0) {\
        break;\
    }\
}
int64_t getCurrentTimeInMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}

void* canSimulationThreadEntry(void* pArg) {
    if (pArg) {
            ((VCwCanSimulation *)pArg)->canSimulationFileReaderLoop();
    }
    return 0;
}

VCwCanSimulation::VCwCanSimulation() {
    mEarSet = VCwEarSet::getInstance();
    mThreadId = CAN_SIMULATION_THREAD_ID;
    mbThreadCreated = false;
}

void VCwCanSimulation::startCanSimulation() {
    lock();
    if (!mbThreadCreated) {
        pthread_create((pthread_t *)&mThreadId, 0, canSimulationThreadEntry, this);
        mbThreadCreated = true;
    }
    unlock();
}

VCwCanSimulation::~VCwCanSimulation() {
}

int VCwCanSimulation::canSimulationFileReaderLoop() {
    //read from the file, if available and simulate CAN traffic..
    canfd_frame frame;
    FILE *fp;
    fp=fopen("/system/etc/vnw/candump.txt", "r");
    char line[256];
    if (fp == NULL) {
        CAN_LOGE("No candump.txt found....");
    } else {
        CAN_LOGE("candump.txt opened successfully...");
    }
    bool bContinue = true;
    int ifNo = 0;
    int64_t delay = -1;
    long nCumulativeDelta = 0;
    long nLastMsgTime = 0;
    int64_t nInitClockTime = 0;
    while (bContinue && fp)
    {
        bool bGotFrame = false;
        char* retstr = fgets(line, 256, fp);
        if (retstr == NULL) {
            char value[PROPERTY_VALUE_MAX];
            property_get("vnw.can.simulation.stop", value, "0");
            if (atoi(value) != 0) {
                bContinue = false;
            } else {
                rewind(fp);
                ifNo = 0;
                delay = -1;
                nCumulativeDelta = 0;
                nLastMsgTime = 0;
                nInitClockTime = 0;
                CAN_LOGE("rewinding and starting all over again...");
            }
            continue;
        }
        /*sample candump output; For time stamp, run candump with -ta
        * "(0000000191.551596) can0  07C   [3]  00 05 01"
        * "  can0  07C   [3]  00 05 01"
        */
        char* pch = strtok_r(line," ",&retstr);
        while (pch != NULL) {
            if ( pch[0] == '(') {
                char* time;
                char* tsLine = pch;
                pch = strtok_r(tsLine, "(.)", &time);
                long sec = 0;
                if (pch != NULL) {
                  sec = strtol(pch, NULL, 10);
                }
                pch = strtok_r(NULL, "(.)", &time);
                long usec = 0;
                if (pch != NULL) {
                  usec = strtol(pch, NULL, 10);
                }
                if (delay == -1) {
                    nLastMsgTime = (sec * 1000) + (usec / 1000);
                    nInitClockTime = getCurrentTimeInMs();
                    /*CAN_LOGE("Initializing nInitClockTime %lld nLastMsgTime %ld",
                            nInitClockTime, nLastMsgTime);*/
                    delay = 0;
                } else {
                    long nThisMsgTime = (sec * 1000) + (usec / 1000);
                    nCumulativeDelta += (nThisMsgTime - nLastMsgTime);
                    int64_t nCurrTime = getCurrentTimeInMs();
                    delay  = (nInitClockTime + nCumulativeDelta) - nCurrTime;
                    nLastMsgTime = nThisMsgTime;
                    if (delay > 0) {
                        usleep(delay * 1000);
                    }
                }
                pch = strtok_r(NULL, " ", &retstr);
                CHECK_TOKEN_AND_BREAK_IF_NULL(pch);
                ifNo = (int)strtol(pch + strlen("can"), NULL, 10);
            } else {
                ifNo = (int)strtol(line + strlen("can"), NULL, 10);
            }
            pch = strtok_r(NULL, " ", &retstr);
            CHECK_TOKEN_AND_BREAK_IF_NULL(pch);
            frame.can_id = (int)strtol(pch, NULL, 16);

            pch = strtok_r(NULL, "[ ]", &retstr);
            CHECK_TOKEN_AND_BREAK_IF_NULL(pch);
            frame.len = (int)strtol(pch, NULL, 10);

            pch = strtok_r(NULL, " ", &retstr);
            int index = 0;
            while ((pch != NULL) && (index < frame.len)) {
                CHECK_TOKEN_AND_BREAK_IF_NULL(pch);
                uint8_t dataByte = (uint8_t)strtol(pch, NULL, 16);
                memcpy(frame.data + index, &dataByte, 1);
                index++;
                pch = strtok_r(NULL, " ", &retstr);
            }
            if (index > 0) {
                bGotFrame = true;
            }
            //done with this line, move to next one...
            pch = NULL;
        }
        if (bGotFrame) {
            int64_t timestamp = (int64_t)getCurrentTimeInMs();
            VCwFrame * pFrame = new VCwFrame(&frame, timestamp, ifNo);
            mEarSet->walk(pFrame, ifNo);
        }
    }
    if (fp) {
        fclose(fp);
    }
    CAN_LOGE("canSimulationFileReaderLoop Thread exiting...");
    return 0;
}
