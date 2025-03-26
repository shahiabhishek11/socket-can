/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwFilterManager.h
 * ------------------------------------------
 * This module passes frame filter to CAN chip.
 *
 */

#ifndef VCwFilterManager_H_
#define VCwFilterManager_H_

#include "VCwSockCan.h"
#include "VCanWrapper.h"

#define MAX_FILTERS 8192


class VCwFilterManager {

    class Filter {
    public:
        int mIfNo;
        uint32_t mFrameId;
        uint32_t mMask;
        uint8_t mType;
        int mEarId;
        int mFlag;
        Filter* mNext;
    public:
        Filter(int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId) {
            mIfNo = ifNo;
            mFrameId = frameId;
            mMask = mask;
            mType = type;
            mEarId = earId;
            mNext = NULL;
        }
        ~Filter() {}
    };

public:
    VCwFilterManager();
    ~VCwFilterManager();

    static VCwFilterManager * getInstance();
    int add (int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId);
    int addCAN (int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId);
    // int add (int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId, int flag);
    int remove (int earId);
    int update(int ifNo, uint32_t frameId, uint32_t mask, uint8_t type);
    void dump ();

protected:
    static VCwFilterManager * pTheFilterManager;  // singleton pointer
    Filter* mInitFilter;
    Filter* mEndFilter;
    VCwSockCan * mPSockCan;    // filter forwarding socket layer

};
#endif /* VCwFilterManager_H_ */
