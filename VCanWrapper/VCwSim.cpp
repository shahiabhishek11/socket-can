/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwSim.cpp
 *
 *  Created on: Aug 15, 2014
 */

#include "VCwSim.h"
#include <unistd.h>
#include "VCwEarSet.h"
#include "VCwFrame.h"

//#undef  LOG_TAG
//#define LOG_TAG "**CANLIB:CSEntry**"

///////////////////////////////////////////////////////////////
// canSimPlaylist is an array of CSEntry objects
// which describe each can frame we construct
// submit into the system
//
CSEntry VCwSim::canSimPlaylist[] = {
    //        FrId, Flag, ifNo,len,[ 0123        4567    ]
    CSEntry(0x10, 0x00, 0x00,  8, 0x01010203, 0x04050607),
    CSEntry(0x11, 0x00, 0x00,  8, 0x02010203, 0x04050607),
    CSEntry(0x10, 0x00, 0x00,  8, 0x01020203, 0x04050607),
    CSEntry(0x11, 0x00, 0x00,  8, 0x02020203, 0x04050607),
    CSEntry(0x10, 0x00, 0x00,  8, 0x03010203, 0x04050607),
    CSEntry(0x11, 0x00, 0x00,  8, 0x04010203, 0x04050607),
    CSEntry(0x10, 0x00, 0x00,  8, 0x03020203, 0x04050607),
    CSEntry(0x11, 0x00, 0x00,  8, 0x04020203, 0x04050607),
    // FrID = -1 means end of list
    CSEntry(-1, -1, -1,        0,   -1,-1)
};

///////////////////////////////////////////////////////////////
CSEntry::CSEntry(int inFrameId, int inFlags, int interfaceNum, int datalen, int d1, int d2) {
    frameId = inFrameId;
    flags = inFlags;
    ifNo = interfaceNum;
    dlen = datalen;
    int i;
    // treat the can bus as big endian
    for (i=7; i>-1; i--) {
        data[i] = d2 & 0xFF;
        d2 >>= 8;
        if (i == 4) d2 = d1;    // load the 1st uint
    }
}
///////////////////////////////////////////////////////////////
// blog - blog current entry, id with pStr
//
void CSEntry::blog(char * pStr) {

    CAN_LOGE("CSEntry(%s): ifNo=%d, id=0x%x, data(%d)",pStr, ifNo, frameId, dlen);
    CAN_LOGE("CB data1:[%2x,%2x,%2x,%2x,",  data[0],data[1],data[2],data[3]);
    CAN_LOGE("CB data2: %2x,%2x,%2x,%2x]\n",data[4],data[5],data[6],data[7]);

}
///////////////////////////////////////////////////////////////
// blogPlaylist - STATIC - blog -1 terminated array of CSEntries
//
void CSEntry::blogPlaylist(CSEntry *p, char * pStr) {  //
    if (!p) {
        CAN_LOGE("Playlist(%s) is empty!!", pStr);
        return;        //=============================>
    }
    while(p->frameId != -1) {
        p->blog(pStr);
        p++;
    }
}

///////////////////////////////////////////////////////////////
// VCwSim
///////////////////////////////////////////////////////////////
//#undef  LOG_TAG
//#define LOG_TAG "**CANLIB:VCwSim**"

VCwSim * VCwSim::pTheVCwSim = 0;


////////////////////////////////////////
// simThreadStart - takes the VCwSim object pointer as an arg
//
static void * simThreadStart(void * parg) {
    ((VCwSim*)parg)->mainSimLoop();
    return 0;
}

///////////////////////////////////////////////////////////////
// VCwSim CONSTRUCTOR
//
VCwSim::VCwSim(int simMode, int playBackDelayMS) {
    mMode = simMode;
    mDelayMS = playBackDelayMS;
    //
    mThreadId = 0;
    mThreadRunning = 0;
    mThreadStop = 0;
}

///////////////////////////////////////////////////////////////
VCwSim::~VCwSim() {
    if (mThreadRunning) {
        VCwSim::stop();
    }
}

///////////////////////////////////////////////////////////////
// getInstance
//
VCwSim * VCwSim::getInstance() {
    if (!pTheVCwSim) {
        pTheVCwSim = new VCwSim();
    }
    return pTheVCwSim;
}

///////////////////////////////////////////////////////////////
// start
//
int VCwSim::start(int simMode, int playBackDelayMS) {
    CAN_LOGE("VCwSim::start ===============================================");
    if (simMode) {
        mMode = simMode;
    }
    if (playBackDelayMS) {
        mDelayMS = playBackDelayMS;
    }
    createThread();
    CAN_LOGE("VCwSim::called createThread =================================");
    return 0;
}

///////////////////////////////////////////////////////////////
// stop
//    - set the stop flag and poll every 500 microseconds
//    - exit when we see he has stopped or we've waited our mDelayMS
//
int VCwSim::stop() {
    int cnt = 2 * (pTheVCwSim->mDelayMS + 2);
    CAN_LOGE("Attempting to stop VCwSim");
    while(mThreadRunning && cnt--) {
        mThreadStop = 1;
        usleep(500);
    }
    return !mThreadRunning;
}

///////////////////////////////////////////////////////////////
// create -  - the indirect constructor
//
int VCwSim::createThread() {
    //
    mThreadStop = 0;
    pthread_create(
            &mThreadId,            // pthreadT thread id
            0,                // thread attributes???  no idea what these are
            simThreadStart,    // entry point
            this            // sole arg to entry point
            );
    return mThreadId;
}

///////////////////////////////////////////////////////////////
void VCwSim::mainSimLoop() {
    CAN_LOGE("VCwSim::entered mainSimLoop");
    mThreadRunning = 1;
    sleep(2);        // let's wait a bit before starting, JIC
    //
    VCwEarSet * pEarSet = VCwEarSet::getInstance();

    while (!mThreadStop) {
        CSEntry * p = canSimPlaylist;
        //
        while ((p->frameId != -1)&&(!mThreadStop)) {
            VCwFrame * pFrame = new VCwFrame(
                    p->frameId | p->flags,
                    p->dlen,
                    p->data
                    // default the frame type for now...
                    );
            //CAN_LOGE("VCwSim::calling Walk. item: 0x%x", (uint32_t)p);
            //CAN_LOGE("&&&&& FAKE WALKING &&&&&&");
            //CAN_LOGE("CSEntry: ifNo=%d, id=0x%x, data(%d)",p->ifNo, p->frameId, p->dlen);
            //CAN_LOGE("CB data1:[%2x,%2x,%2x,%2x,",  p->data[0],p->data[1],p->data[2],p->data[3]);
            //CAN_LOGE("CB data2: %2x,%2x,%2x,%2x]\n",p->data[4],p->data[5],p->data[6],p->data[7]);
            pEarSet->walk(pFrame, p->ifNo);
            //CAN_LOGE("VCwSim::returned from Walk");
            p++;
            usleep(mDelayMS * 1000);
            //CAN_LOGE("Next Playlist item:0x%x, mThreadStop: %d", (uint32_t)p, mThreadStop);
        }
        //CAN_LOGE("Resetting Playlist: mThreadStop: %d", mThreadStop);
    }
    CAN_LOGE("VCwSim play thread STOPPED!");
    mThreadRunning = 0;
}

