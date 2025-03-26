/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwEar.cpp
 * --------------------------------------------
 *  Created on: Jul 17, 2014
 * --------------------------------------------
 * Represents the data from a CanHelper::registerListener call.
 * See VCwEars for the container.
 */
#include "VCwBase.h"
#include "VCwEar.h"
#include "VCwFrame.h"
#include "VCwDropFrameLog.h"
#include <linux/can.h>

//#undef LOG_TAG
//#define  LOG_TAG    "**CANLIB::VCwEar**"


///////////////////////////////////////////////////////////////
// VCwEar - constructor
//
VCwEar::VCwEar() {
    init(0,0,0,0,0,0);
    initCAN(0,0,0,0,0);
}

///////////////////////////////////////////////////////////////
// VCwEar - sugar constructor for callback function
//
VCwEar::VCwEar(uint32_t fid, uint32_t mask, CanCallback cb, void * userData, int ifNo) {
    init(fid, mask, cb, NULL, userData,  ifNo);
    makeEarId();
}
///////////////////////////////////////////////////////////////
// VCwEar - sugar constructor for callback function CAN nodes
//
VCwEar::VCwEar(uint32_t fid, uint32_t mask, CanCallback_CAN cb, void * userData, int ifNo) {
    initCAN(fid, mask, cb, userData,  ifNo);
    makeEarId();
}


///////////////////////////////////////////////////////////////
// VCwEar - sugar constructor for blocking queue
//
VCwEar::VCwEar(uint32_t fid, uint32_t mask, VCwBlockingQueue * rxQueue,
        void * userData, int ifNo) {
    init(fid, mask, NULL, rxQueue, userData,  ifNo);
    makeEarId();
}

///////////////////////////////////////////////////////////////
// init
// for both VCAN
void VCwEar::init(uint32_t fid, uint32_t mask, CanCallback cb,
        VCwBlockingQueue * rxQueue, void* userData, int ifNo) {
    mFrameId = fid;
    mFrIdMask = mask;
    mCallback = cb;
    mRxQueue = rxQueue;
    mIfNo = ifNo;
    mUserData = userData;
    // mFlag = flag;
}
// ///////////////////////////////////////////////////////////////
// // init
// // for CAN
void VCwEar::initCAN(uint32_t fid, uint32_t mask, CanCallback_CAN cb,
        void* userData, int ifNo) {
    mFrameId = fid;
    mFrIdMask = mask;
    cCallback = cb;
    mIfNo = ifNo;
    mUserData = userData;
    // mFlag = flag;
}

///////////////////////////////////////////////////////////////
// makeEarId
//
int VCwEar::makeEarId() {
    unsigned long long tid = (unsigned long long)this;
    tid >>= 5;    // at least 32 bytes per VCwEar object
    tid &= 0x7FffFFff;    // grab 31 bits
    mEarId = (int)tid;
    return mEarId;
}

///////////////////////////////////////////////////////////////
// getIfNo
//
int VCwEar::getIfNo() {
    return mIfNo;
}

///////////////////////////////////////////////////////////////
// ~VCwEar - destructor
//
VCwEar::~VCwEar() {
    // TODO Auto-generated destructor stub
}

///////////////////////////////////////////////////////////////
// matchFrame
///@returns int 0 if they don't match
//
int VCwEar::matchFrame(VCwFrame * pFrame, int ifNo) {
    // check ids (masked) and iface - returning zero if not matching
    //
    int myID = (mFrameId & mFrIdMask);
    //CAN_LOGE("matchFrame (mFrameId:0x%x & mask:0x%x=%d",
    //                mFrameId,mFrIdMask,myID);
    int hisID = (pFrame->getId() & mFrIdMask);
    //CAN_LOGE("matchFrame (pFrame->id:0x%x & mask:0x%x=%d",
    //            pFrame->id,mFrIdMask, hisID);
    int ifaceOK = hasIface(ifNo, 0);    // not exact, match IFACE_ANY also
    int x = (myID == hisID) && ifaceOK;
    //CAN_LOGE("matchFrame: hasIface(nuifNo:%d)=%d - returning..%d",
    //                    nuifNo, ifaceOK, x);
    return x;
}

///////////////////////////////////////////////////////////////
// hasIface
///@param ifNo - an interface index
///@param exact - if 1 then don't count IFACE_ANY
///@returns 1 if this ear is listening for this interface
//
int VCwEar::hasIface(int ifNo, int exact) {
    return (mIfNo == ifNo)||(!exact && (mIfNo == VCwBase::IFACE_ANY));
}

//////////////////////////////////////////////////////////////////////
// callBackFrame - match and do the mCallback (if it matches)
///@returns int 0 if it did the mCallback or enqueue frame for worker
///             -1 if the match failed or enqueue of the frame failed
///                or neither callback or queue is not registered
int VCwEar::callBackFrame(VCwFrame * pFrame, int ifNo,  unsigned int * frameQueued) {
    CAN_LOGE("callBackFrame");
    int rc = 0;
    VCwFrame *pTempFrame = NULL;
    if (matchFrame(pFrame, ifNo)) {
        uint8_t data[8];
        pFrame->getData(data, 8);
        CAN_LOGE("Callback: ifNo: %d, id:0x%x, data: 0x%x, 0x%x", ifNo,
                pFrame->getId(), data[0], data[1]);
        if (mCallback != NULL) {
            //Since the client is unaware of this ID manipulation with CAN_RTR_FLAG for
            //buffered frame, remove this CAN_RTR_FLAG bit before sending the frame
            //to the client.
            pFrame->clearRtr();
            (*mCallback)(pFrame, mUserData, ifNo);
        } else if (mRxQueue != NULL && frameQueued != NULL) {
            if (*frameQueued == 1) {
                //if frame is already queued, clone it
                pTempFrame = pFrame->clone(1);
            } else {
                pTempFrame = pFrame;
            }
            if (pTempFrame != NULL) {
                if (mRxQueue->enqueueNode(pTempFrame) != 0) {
                    VCwDropFrameLog *pLog = VCwDropFrameLog::getInstance();
                    if (pLog != NULL) {
                        pLog->updateDropFrameCount();
                    }
                    if (*frameQueued == 1) {
                        delete pTempFrame;
                    }
                } else {
                   *frameQueued = 1;
                }
            } else {
                rc = -1;
            }
        } else {
            rc = -1;
        }
    } else {
        rc = -1;
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// callBackFrame - match and do the mCallback (if it matches)
///@returns int 0 if it did the mCallback or enqueue frame for worker
///             -1 if the match failed or enqueue of the frame failed
///                or neither callback or queue is not registered
int VCwEar::callBackFrameCAN(VCwFrame * pFrame, int ifNo,  unsigned int * frameQueued) {
    CAN_LOGE("callBackFrameCAN");
    int rc = 0;
    VCwFrame *pTempFrame = NULL;
    if (matchFrame(pFrame, ifNo)) {
        uint8_t data[8];
        pFrame->getData(data, 8);
        CAN_LOGE("callBackFrameCAN: ifNo: %d, id:0x%x, data: 0x%x, 0x%x", ifNo,
                pFrame->getId(), data[0], data[1]);
        if (cCallback != NULL) {
            //Since the client is unaware of this ID manipulation with CAN_RTR_FLAG for
            //buffered frame, remove this CAN_RTR_FLAG bit before sending the frame
            //to the client.
            pFrame->clearRtr();
            //as this is for CAN nodes to differ, we may send flag
            CAN_LOGE("before sending callback to clients");
            (*cCallback)(pFrame, mUserData, ifNo, mFlag);
            CAN_LOGE("after notifying client");
        } else if (mRxQueue != NULL && frameQueued != NULL) {
            if (*frameQueued == 1) {
                //if frame is already queued, clone it
                CAN_LOGE("if frame is already queued, clone it");
                pTempFrame = pFrame->clone(1);
            } else {
                pTempFrame = pFrame;
                CAN_LOGE("pFrame is pTempFrame ");
            }
            if (pTempFrame != NULL) {
                if (mRxQueue->enqueueNode(pTempFrame) != 0) {
                    CAN_LOGE("enqueue node pTempFrame");
                    VCwDropFrameLog *pLog = VCwDropFrameLog::getInstance();
                    if (pLog != NULL) {
                        pLog->updateDropFrameCount();
                    }
                    if (*frameQueued == 1) {
                        delete pTempFrame;
                    }
                } else {
                   *frameQueued = 1;
                }
            } else {
                rc = -1;
            }
        } else {
            rc = -1;
        }
    } else {
        rc = -1;
    }
    return rc;
}
