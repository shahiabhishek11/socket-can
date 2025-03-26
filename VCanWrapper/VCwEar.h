/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwEar.h
 * --------------------------------------------
 *  Created on: Jul 17, 2014
 * --------------------------------------------
 * Represents the data from a VCanWrapper::registerListener call.
 * See VCwEars for the container.
 */

#ifndef VCwEar_H_
#define VCwEar_H_

#include <inttypes.h>
#include "VCwBase.h"
#include "VCwNode.h"
#include "VCanWrapper.h"

///////////////////////////////////////////////////////////////
// VCwEar - a Can Helper Listener definition
//
class VCwEar : public VCwNode {
public:
    VCwEar();
    VCwEar(uint32_t fid, uint32_t mask, CanCallback callback,
          void* userData = NULL, int nuIfNo = VCwBase::IFACE_ANY);
    VCwEar(uint32_t fid, uint32_t mask, CanCallback_CAN callback,
          void* userData = NULL, int nuIfNo = VCwBase::IFACE_ANY);
    // VCwEar(uint32_t fid, uint32_t mask, CanCallback callback,
    //       void* userData = NULL, int nuIfNo = VCwBase::IFACE_ANY, int flag);
    VCwEar(uint32_t fid, uint32_t mask, VCwBlockingQueue * rxQueue,
            void* userData = NULL, int nuIfNo = VCwBase::IFACE_ANY);
    void init(uint32_t fid, uint32_t mask, CanCallback callback,
            VCwBlockingQueue * mRxQueue, void * userData = NULL,
            int nuIfNo = VCwBase::IFACE_ANY);
    void initCAN(uint32_t fid, uint32_t mask, CanCallback_CAN callback,
            void * userData = NULL, int nuIfNo = VCwBase::IFACE_ANY);
    // void init(uint32_t fid, uint32_t mask, CanCallback callback,
    //         VCwBlockingQueue * mRxQueue, void * userData = NULL,
    //         int nuIfNo = VCwBase::IFACE_ANY, int flag);     //flag method
    virtual ~VCwEar();

    int matchFrame(VCwFrame * pFrame, int ifNo);
    // match and do the mCallback (if it matches)
    // NOTE!  assumes ownership of pFrame!!
    int callBackFrame(VCwFrame * pFrame, int ifNo, unsigned int * queuedState);
    int callBackFrameCAN(VCwFrame * pFrame, int ifNo, unsigned int * queuedState);

    int hasIface(int ifNo, int exact =0);    // return 1 if this ear is listening for this interface

    int isId(uint32_t nid, uint32_t nmask)
        {return ((nid & nmask) == (mFrameId && mFrIdMask));};

    int getEarId() {return mEarId;};
    int makeEarId();

    int getIfNo(); // return mIfNo
protected:
    int mEarId;  // unique id of this listener
    // the basic data of a listener
    int mFrameId;        // frame id
    int mFrIdMask;    // frame id mask
    CanCallback mCallback;  // call back to client
    CanCallback_CAN cCallback;  //call back to client for CAN nodes
    VCwBlockingQueue * mRxQueue;
    int mIfNo;    // interface
    void* mUserData;
    int mFlag = 1;  //flag for test

};

#endif /* VCwEar_H_ */
