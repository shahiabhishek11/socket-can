/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwFilterManager.cpp
 *
 */


#include "VCwFilterManager.h"


// singleton pointer
VCwFilterManager * VCwFilterManager::pTheFilterManager = 0;
//#undef  LOG_TAG
//#define LOG_TAG "CANLIB:VCwFilterManager"



VCwFilterManager::VCwFilterManager() {

    // get socket for sending filters, etc
    mPSockCan = VCwSockCan::getInstance();

    mInitFilter = new Filter(0, 0, 0, FRAME_FILTER_TYPE_ON_REFRESH, 0);
    mEndFilter = mInitFilter;
}


VCwFilterManager::~VCwFilterManager() {
    Filter* p = mInitFilter;
    Filter* next;

    while (p) {
        next = p->mNext;
        delete p;
        p = next;
    }
}


/***************************************************
 * getInstance
 ***************************************************/
VCwFilterManager * VCwFilterManager::getInstance() {
    if (!pTheFilterManager) {
        pTheFilterManager = new VCwFilterManager();
    }
    return pTheFilterManager;
}

void VCwFilterManager::dump()
{
    Filter* p = mInitFilter;

    while (p) {
        CAN_LOGE("filter>%p  ifNo:%i, frameId:0x%x, mask:0x%x type:%i ear:0x%x next:%p",
                p, p->mIfNo, p->mFrameId, p->mMask, p->mType, p->mEarId, p->mNext);
        p = p->mNext;
    }
}

/***********************************************************************
 * add - adds a frame filter on the requested VCAN interface
 * checking for duplicates
 * @ifNo - system interface number
 * @frameId - the frame id pattern to look for
 * @mask - the mask that selects the bits that compare
 * @type - onrefresh or onchange
 * @earId - the earId returned by earSet
 ***********************************************************************/
int VCwFilterManager::add(int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId) {
    CAN_LOGE("VCwFilterManager::add ifNo:%i, frameId:0x%x, mask:0x%x type:%i earId:0x%x",
            ifNo, frameId, mask, type, earId);

    // adding filter
    Filter* f = new Filter(ifNo, frameId, mask, type, earId);

    mEndFilter->mNext = f;
    mEndFilter = f;

    return mPSockCan->processFrameFilter(FRAME_FILTER_CMD_ADD, ifNo, frameId, mask, type);
}
/***********************************************************************
 * add - adds a frame filter on the requested CAN interface
 * checking for duplicates
 * @ifNo - system interface number
 * @frameId - the frame id pattern to look for
 * @mask - the mask that selects the bits that compare
 * @type - onrefresh or onchange
 * @earId - the earId returned by earSet
 ***********************************************************************/
int VCwFilterManager::addCAN(int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId) {
    CAN_LOGE("VCwFilterManager::add ifNo:%i, frameId:0x%x, mask:0x%x type:%i earId:0x%x",
            ifNo, frameId, mask, type, earId);

    // adding filter
    Filter* f = new Filter(ifNo, frameId, mask, type, earId);

    mEndFilter->mNext = f;
    mEndFilter = f;

    return mPSockCan->processFrameFilterCAN(FRAME_FILTER_CMD_ADD, ifNo, frameId, mask, type);
}
// /***********************************************************************
//  * add - adds a frame filter on the requested CAN interface
//  * checking for duplicates
//  * @ifNo - system interface number
//  * @frameId - the frame id pattern to look for
//  * @mask - the mask that selects the bits that compare
//  * @type - onrefresh or onchange
//  * @earId - the earId returned by earSet
//  * @flag - to differ vcan node and can nodes
//  ***********************************************************************/
// int VCwFilterManager::add(int ifNo, uint32_t frameId, uint32_t mask, uint8_t type, int earId, int flag) {
//     CAN_LOGE("VCwFilterManager::add ifNo:%i, frameId:0x%x, mask:0x%x type:%i earId:0x%x",
//             ifNo, frameId, mask, type, earId);
//     CAN_LOGE("add function in flag function");
//     // adding filter
//     Filter* f = new Filter(ifNo, frameId, mask, type, earId, flag);

//     mEndFilter->mNext = f;
//     mEndFilter = f;

//     return mPSockCan->processFrameFilter(FRAME_FILTER_CMD_ADD, ifNo, frameId, mask, type, flag);
// }


int VCwFilterManager::update(int ifNo, uint32_t frameId, uint32_t mask, uint8_t type) {
    CAN_LOGE("VCwFilterManager::update ifNo:%i, frameId:0x%x, mask:0x%x type:%i",
            ifNo, frameId, mask, type);

    Filter* p = mInitFilter;
    Filter* prev = mInitFilter;

    while (p) {
        if (p->mFrameId == frameId && p->mIfNo == ifNo) {
            p->mType = type;
            mPSockCan->processFrameFilter(FRAME_FILTER_CMD_UPDATE, p->mIfNo,
                    p->mFrameId, p->mMask, p->mType);
            CAN_LOGE("VCwFilterManager::updated ifNo:%i, frameId:0x%x, mask:0x%x type:%i",
                    p->mIfNo, p->mFrameId, p->mMask, p->mType);
            break;
        }
        prev = p;
        p = p->mNext;
    }

    return 0;
}


/**************************************************************************
 * remove - deletes a frame filter on the requested CAN interface
 * @earId - the ear that was set when creating filter
 **************************************************************************/
int VCwFilterManager::remove(int earId) {
    CAN_LOGE("VCwFilterManager::remove earId:0x%x", earId);

    Filter* p = mInitFilter;
    Filter* prev = mInitFilter;

    while (p) {
        if (p->mEarId == earId) {
            mPSockCan->processFrameFilter(FRAME_FILTER_CMD_REMOVE, p->mIfNo,
                    p->mFrameId, p->mMask, p->mType);
            CAN_LOGE("VCwFilterManager::removed ifNo:%i, frameId:0x%x, mask:0x%x type:%i",
                    p->mIfNo, p->mFrameId, p->mMask, p->mType);
            prev->mNext = p->mNext;
            if (mEndFilter == p) {
                mEndFilter = prev;
            }
            delete p;
            break;
        }
        prev = p;
        p = p->mNext;
    }

    return 0;
}






