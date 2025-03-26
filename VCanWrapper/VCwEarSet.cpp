/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwEarSet.cpp
 *
 *  Created on: Jul 17, 2014
 */
#include "VCwEarSet.h"
#include "VCwFrame.h"
#include "VCwSockCan.h"
#include <unistd.h>
#include <string.h>
#include <linux/can.h>

//#undef  LOG_TAG
//#define LOG_TAG "**CANLIB:VCwEarSet**"


///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// VCwEarSet
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

VCwEarSet * VCwEarSet::pTheEarSet = 0;

///////////////////////////////////////////////////////////////
// VCwEarSet
//
VCwEarSet::VCwEarSet() {
    mCnt = 0;
    // my test worked right when I assigned this initializer
    //mRWLock = PTHREAD_RWLOCK_INITIALIZER;
    memset(&mRWLock, 0, sizeof(pthread_rwlock_t));
    mpCanSimulationHandle = NULL;
    pthread_rwlock_init(&mRWLock, 0);
}

///////////////////////////////////////////////////////////////
// ~VCwEarSet
//
VCwEarSet::~VCwEarSet() {
    // clear our VCwEar list
    writeLock();
    VCwNode * pEar = getNext();
    while (pEar) {
        VCwNode * pTemp = pEar->getNext();
        delete pEar;
        pEar = pTemp;
    }
    pthread_rwlock_destroy(&mRWLock);
}

///////////////////////////////////////////////////////////////
// getInstance -  //$ToDo:  ThreadSafe
//
VCwEarSet * VCwEarSet::getInstance() {
    if (!pTheEarSet) {
        pTheEarSet = new VCwEarSet();
    }
    return pTheEarSet;
}

///////////////////////////////////////////////////////////////
// add - this ear to the set of ears, replace if duplicate
//        - what is a duplicate??
///@returns earId for the new object, or zero if error!
//
int VCwEarSet::add(VCwEar * pEar) {
    if (!pEar) {
        return 0;
    }
    writeLock();    // need destructive/exclusive lock
    VCwNodeSet::add(pEar);
    rwUnlock();
    return pEar->getEarId();
}

///////////////////////////////////////////////////////////////
// add - sugar for callback function
//
int VCwEarSet::add(int fid, int mask, CanCallback cb, void* userData, int nuIfNo) {
    CAN_LOGE("VCwEarSet add(fid:0x%x, mask:0x%x, userData:%p, ifNo:%x",
                        fid, mask, userData, nuIfNo);
         CAN_LOGE("add in VCwEarSet");
        // int flag = 0;              
    VCwEar * pEar = new VCwEar(fid, mask, cb, userData, nuIfNo);
    VCwSockCan *pSockCan = VCwSockCan::getInstance();
    if (pSockCan) {
        CAN_LOGE("starting readers");
        pSockCan->startReaders();
    }
    if (mpCanSimulationHandle == NULL) {
        mpCanSimulationHandle = new VCwCanSimulation();
        if (mpCanSimulationHandle) {
            mpCanSimulationHandle->startCanSimulation();
        }
    }
    return add(pEar);
}
// ///////////////////////////////////////////////////////////////
// // add - sugar for callback function for CAN nodes
// //
int VCwEarSet::addCan(int fid, int mask, CanCallback_CAN cb, void* userData, int nuIfNo) {
    CAN_LOGE("VCwEarSet add(fid:0x%x, mask:0x%x, userData:%p, ifNo:%x",
                        fid, mask, userData, nuIfNo);
         CAN_LOGE("add in VCwEarSet");               
    VCwEar * pEar = new VCwEar(fid, mask, cb, userData, nuIfNo);
    VCwSockCan *pSockCan = VCwSockCan::getInstance();
        if (pSockCan) {
        CAN_LOGE("starting readers to CAN nodes");
        pSockCan->startReadersCan();
    }
    // if (pSockCan) {
    //     CAN_LOGE("starting readers");
    //     pSockCan->startReaders();
    // }
    // if (mpCanSimulationHandle == NULL) {
    //     mpCanSimulationHandle = new VCwCanSimulation();
    //     if (mpCanSimulationHandle) {
    //         mpCanSimulationHandle->startCanSimulation();
    //     }
    // }
    return add(pEar);
}

///////////////////////////////////////////////////////////////
// add - sugar for blocking queue
//
int VCwEarSet::add(int fid, int mask, VCwBlockingQueue *rxQueue,
        void* userData, int nuIfNo) {
    CAN_LOGE("VCwEarSet add(fid:0x%x, mask:0x%x, userData:%p, ifNo:%x",
                        fid, mask, userData, nuIfNo);
    VCwEar * pEar = new VCwEar(fid, mask, rxQueue, userData, nuIfNo);
    VCwSockCan *pSockCan = VCwSockCan::getInstance();
    if (pSockCan) {
        pSockCan->startReaders();
    }
    if (mpCanSimulationHandle == NULL) {
        mpCanSimulationHandle = new VCwCanSimulation();
        if (mpCanSimulationHandle) {
            mpCanSimulationHandle->startCanSimulation();
        }
    }
    return add(pEar);
}

///////////////////////////////////////////////////////////////
// rem
///@param xEarId the value created when this listener was created<br>
///        - note earId was returned in registerListener
//
int VCwEarSet::rem(int xEarId) {
    writeLock();    // need destructive/exclusive lock
    VCwEar * pe;
    int ret = 1;
    for (pe = (VCwEar*)getNext(); pe; pe = (VCwEar*)(pe->getNext())) { // for each in our list
        if (pe->getEarId() == xEarId) {
            VCwNodeSet::rem(pe);    // unlink it
            delete pe;        // trash it
            ret = 0;        // set success return code
            break;  // that's it - we allow one value per VCwEar
        }
    }
    rwUnlock();
    return ret;

}

///////////////////////////////////////////////////////////////
// walk -
//    - CHANGE(9/9): WE own the VCwFrame, so we can pass it to multiple callbacks!
///@param pFrame - pointer at a filled out and ready to pass on VCwFrame
///@param ifNo - interface, this should be within the VCwFrame!?!
//
void VCwEarSet::walk(VCwFrame * pFrame, int ifNo) {
    CAN_LOGE("walk");
    if (!pFrame) return;    //--need a frame to do anything --->
    readLock();     // shared read lock
    VCwNode * tEar;    // C++ fudge (why'd he leave this power on the table??)
    int cbCount = 0;
    unsigned int frameQueued = 0;

    //CAN_LOGE("walk(frid:0x%x, ifNo:%d)", pFrame->getId(), nuIfNo);
    // check each Ear (listener) for callbackness
    for (tEar = getNext(); tEar; tEar = tEar->getNext()) {
        VCwEar* pEar =((VCwEar*)tEar);
        if (pEar->callBackFrame(pFrame, ifNo, &frameQueued) == 0)
            cbCount++;
    }
    rwUnlock();
    if (frameQueued == 0) {
        delete pFrame;
    }
}
///////////////////////////////////////////////////////////////
// walk - CAN nodes
//    - CHANGE(9/9): WE own the VCwFrame, so we can pass it to multiple callbacks!
///@param pFrame - pointer at a filled out and ready to pass on VCwFrame
///@param ifNo - interface, this should be within the VCwFrame!?!
//
void VCwEarSet::walkCAN(VCwFrame * pFrame, int ifNo) {
    CAN_LOGE("walkCAN");
    if (!pFrame) return;    //--need a frame to do anything --->
    readLock();     // shared read lock
    VCwNode * tEar;    // C++ fudge (why'd he leave this power on the table??)
    int cbCount = 0;
    unsigned int frameQueued = 0;

    //CAN_LOGE("walk(frid:0x%x, ifNo:%d)", pFrame->getId(), nuIfNo);
    // check each Ear (listener) for callbackness
    for (tEar = getNext(); tEar; tEar = tEar->getNext()) {
        VCwEar* pEar =((VCwEar*)tEar);
        if (pEar->callBackFrameCAN(pFrame, ifNo, &frameQueued) == 0)
            cbCount++;
    }
    rwUnlock();
    if (frameQueued == 0) {
        delete pFrame;
    }
}

///////////////////////////////////////////////////////////////
// count ears on an interface
//
int VCwEarSet::count(int ifNo) {
    int c = 0;
    readLock();
    VCwEar * p = (VCwEar *)getNext();
    while (p) {
        if (p->hasIface(ifNo)) {
            c++;
        }
        p = (VCwEar*)p->getNext();
    }
    rwUnlock();
    return c;
}
