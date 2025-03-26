/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwFrame.cpp
 *
 *  Created on: Jul 14, 2014
 */
///////////////////////////////////////////////////////////////
#include "VCwFrame.h"
//-----------------------------------------
#include <linux/types.h>
#include <linux/can.h>  // need <linux/types.h> for this to work!!
#include <string.h>

//#undef LOG_TAG
//#define LOG_TAG "CANLIB:VCwFrame"
/*
 * We expect the following fields to be available in the linux can_frame structure
 *    canid_t can_id -  32 bit CAN_ID + EFF/RTR/ERR flags
 *    uint8_t    can_dlc -  frame payload length in byte (0 .. CAN_MAX_DLEN)
 *    uint8_t    data[CAN_MAX_DLEN] an 8 byte array
*/
//

///////////////////////////////////////////////////////////////
VCwFrame::VCwFrame() {
    init();
}

///////////////////////////////////////////////////////////////
VCwFrame::VCwFrame(can_frame * p, uint32_t timestamp, int ifNo) {
    init();
    setFrame(p);
    mTimestamp = timestamp;
    mIfNo = ifNo;
}

///////////////////////////////////////////////////////////////
VCwFrame::VCwFrame(canfd_frame * p, uint64_t timestamp, int ifNo) {
    init();
    setFrame(p);
    mTimestamp = timestamp;
    mIfNo = ifNo;
}

///////////////////////////////////////////////////////////////
VCwFrame::VCwFrame(uint32_t id, int dataLen, uint8_t * pData,
        bool extFlag, bool rtrFlag, uint32_t timestamp) {
    init(id, dataLen, pData, extFlag, rtrFlag, timestamp);
}


///////////////////////////////////////////////////////////////
VCwFrame::~VCwFrame() {
    if (mpTempFrame) {
        delete mpTempFrame;
        mpTempFrame = 0;
    }
}

///////////////////////////////////////////////////////////////
// clone
//
VCwFrame * VCwFrame::clone(int deep) {
    VCwFrame * pf = new VCwFrame();
    if (deep) {
        // do the loops first and use diff index vars
        // ... to facilitate multi-threading (core style)
        for (int i=0; i<mDataLen; i++)
            pf->mData[i] = mData[i];
        pf->mId = mId;
        pf->mDataLen = mDataLen;
        pf->mTimestamp = mTimestamp;
        pf->mFrType = mFrType;
        pf->mIfNo = mIfNo;
        pf->mMask = mMask;
        pf->mExt = mExt;
        pf->mErr = mErr;
        if (mpTempFrame) {
            pf->mpTempFrame = new canfd_frame;
            memcpy(pf->mpTempFrame, mpTempFrame, sizeof(canfd_frame));
        } else {
            pf->mpTempFrame = 0;
        }
    }
    return pf;
}

///////////////////////////////////////////////////////////////
// init - this is sugary for from-scratch Normal frame creation,
//         - but you need to make extra calls for RTR
///@param id - the frame id, without the flags, big enough for ntype
///@param dataLen - length of data to copy from the following pointer
///@param pData - data array at least 'ndataLen' bytes long
///@param extFlag - flag indicating extended frame
///@param rtrFlag - flag indicating rtr frame
///@param timestamp - timestamp of when frame was created
//
void VCwFrame::init(uint32_t id, int dataLen, uint8_t * pData,
        bool extFlag, bool rtrFlag, uint32_t timestamp) {

    mErr = mExt = 0;
    mId = id;
    if (extFlag) mExt = CAN_EFF_FLAG;
    if (rtrFlag) setRtr();

    setIdByType(id, extFlag ? VCwBase::ID_EXT : VCwBase::ID_NORM);
    setData(pData, dataLen);
    mpTempFrame = 0;
    mTimestamp = timestamp;
//    logFrame("Init Frame");
}

///////////////////////////////////////////////////////////////
// logFrame - log details of this frame
//
#if 0
void VCwFrame::logFrame(char * tag) {
    // CAN_LOGE("%s: id:0x%x, len: %d, flags: %x FD: %s ",
    //         tag, mId, mDataLen, mExt|mErr,
    //         mDataLen > 8 ? "yes" : "no");
    // CAN_LOGE("...%s 2 data words (hex): %8x, %8x", tag,
    //     (mData[0]<<24)+(mData[1]<<16)+(mData[2]<<8)+(mData[3]),
    //     (mData[4]<<24)+(mData[5]<<16)+(mData[6]<<8)+(mData[7]));
    //CAN_LOGE("...%s Data : %2x, %2x,%2x, %2x, %2x, %2x, %2x,%2x", tag,
    //    mData[0],mData[1], mData[1], mData[2], mData[3], mData[4], mData[5], mData[6], mData[7] );
}
#endif


///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// USER SUGAR - Upper Interface
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// getData - copy out the data
///@param pData - points at array at least "maxlen" size.
///@param maxlen - max number of bytes we will copy
///@returns int - number of bytes filled into @pData
//
int VCwFrame::getData(uint8_t * pData, int maxlen) {
    if (!pData) {
        return 0;   // no pointer no data
    }
    int cnt = mDataLen;
    if (cnt > maxlen) cnt = maxlen;
    uint8_t * pd = mData;
    int i = 0; // i tracks how many we've copied
    while(i < cnt) {
        if (i < (cnt - 3)) { // we can copy a word at once
            *((uint32_t *)pData) = *((uint32_t*)pd);
            pData += 4;
            pd += 4;
            i+=4;
        } else {  // fill in up to 3 bytes
            *pData = *pd;
            pData++;
            pd++;
            i++;
        }
    }
    return cnt;
}



///////////////////////////////////////////////////////////////
// setFrame - standard can frame, fill us in from it
void VCwFrame::setFrame(can_frame * pCanFrame) {
    int i = pCanFrame->can_dlc;
    if (i < 0) {
        i = 0;    // force it right,
    }
    if (i > CAN_MAX_DLEN) {
        i = CAN_MAX_DLEN;
    }
    mDataLen = i;
    setData(pCanFrame->data, i);
    setFlagsFromId(pCanFrame->can_id);
    int type = mExt != 0 ? VCwBase::ID_EXT : VCwBase::ID_NORM;
    setIdByType(pCanFrame->can_id, type);
}

///////////////////////////////////////////////////////////////
/// setFrame - FD can frame, fill us in from it
///@param p - points at an FD frame to fill in
//
void VCwFrame::setFrame(canfd_frame * pCanFrame) {
    int i = pCanFrame->len;
    if (i < 0) {
        i = 0;    // force it right,
    }
    if (i > CANFD_MAX_DLEN) {
        i = CANFD_MAX_DLEN;
    }
    mDataLen = i;
    setData(pCanFrame->data, i);
    setFlagsFromId(pCanFrame->can_id);
    int type = mExt != 0 ? VCwBase::ID_EXT : VCwBase::ID_NORM;
    setIdByType(pCanFrame->can_id, type);
}


///////////////////////////////////////////////////////////////
// getFrameLen
//
int VCwFrame::getFrameLen() {
    switch (mFrType) {    // varies by type I fear
    case ID_NORM:
    case ID_EXT:
        break;
//    case ID_FD:
//        return sizeof(canfd_frame);
    }
    return sizeof(canfd_frame);
}

///////////////////////////////////////////////////////////////
// getFrame - construct the frame into a malloc'd area and return it
///
///@returns - filled in can frame ready for the wire
//
canfd_frame * VCwFrame::getFrame() {
    if (!mpTempFrame) {
        mpTempFrame = new canfd_frame;
    }
    mpTempFrame->can_id = mId | mExt;
    mpTempFrame->len = mDataLen;
    memcpy(mpTempFrame->data, mData, mDataLen);
    return mpTempFrame;
}

///////////////////////////////////////////////////////////////
// setFlagsFromId - set ext, rtr, err flags
///@param canId - raw can id from the "wire"
//
void VCwFrame::setFlagsFromId(uint32_t canId) {
    mExt = canId & CAN_EFF_FLAG;  // top bit means extended address
    mErr = canId & CAN_ERR_FLAG;     // now what!!
}


///////////////////////////////////////////////////////////////
void VCwFrame::setIdMask(uint32_t id, uint32_t nmask) {
    mMask = nmask;
    mId = id & mMask;
}

void VCwFrame::setMaskByType(uint32_t id, int typ) {
    mMask=VCwBase::typeToMask(typ);
    if (id & CAN_RTR_FLAG) {
        mMask |= CAN_RTR_FLAG;
    }
}

///////////////////////////////////////////////////////////////
void VCwFrame::setIdByType(uint32_t id, int typ) {
    mFrType=typ;

    setMaskByType(id, typ);
    mId = id & mMask;
}

///////////////////////////////////////////////////////////////
void VCwFrame::setData(uint8_t * pdata, int len){
    // lower bound
    if (len < 0) return;
    // upper bound our input
    if (len > VCwBase::MAX_DATA) len = VCwBase::MAX_DATA;
    mDataLen = len;    // save the result
    // copy in the data
    for (int i = len - 1; i >= 0; i--) {
        mData[i] = pdata[i];
    }
}

///////////////////////////////////////////////////////////////
void VCwFrame::setType(int ntype) {
    mFrType = ntype;
    mMask = VCwBase::typeToMask(mFrType);
}

///////////////////////////////////////////////////////////////
void VCwFrame::setErr(int on) {
    mErr = on ? CAN_ERR_FLAG : 0;
}

///////////////////////////////////////////////////////////////
void VCwFrame::setExt(int on) {
    mExt = on ? CAN_EFF_FLAG : 0;
}
