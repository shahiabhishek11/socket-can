/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwFrame.h
 *
 *  Created on: Jul 14, 2014
 */

#ifndef VNCFRAME_H_
#define VNCFRAME_H_

struct can_frame;
struct canfd_frame;

#include "VCwBase.h"
#include "VCwNode.h"
#include <linux/can.h>
/////////////////////////////////////////////////////////////
// TODO
// - malloc the frame data so it's only as long as need be
//

///////////////////////////////////////////////////////////////
// VCwFrame encapsulates all 4 "types" of frames
/// Key Concepts;
///    - this class is used to build_up, convey, & unpack an outgoing or incoming frame
/// - a frame is composed of: id, flags, data, data len,  and some inferred types
/// - VCwFrame helps its client access those components
///-----------------------------------------------------------
/// Upper Layer User
/// - Receiving Data
///   - getDataLen() will tell you the length of the incoming data
///   - getData(ptr, maxlen) will copy up to maxlen bytes into @ptr
///        - and returns length transferred
///   - isErr(), isExt, or isRtr - return the state of those flags
/// - Transmitting Data
///   - setData and set<flag> calls are providing to build a VCwFrame for transmission
//
class VCwFrame : public VCwNode, public VCwBase {
public:
    VCwFrame();
    VCwFrame(uint32_t frameId, int nlen=0, uint8_t * pndata=0,
            bool extFlag = false, bool rtrFlag = false, uint32_t timestamp = 0);
    VCwFrame(can_frame * pFrame, uint32_t timestamp, int ifNo);
    VCwFrame(canfd_frame * pFrame, uint64_t timestamp, int ifNo);

    VCwFrame * clone(int deep =1);

    virtual ~VCwFrame();
public:
    //----------------------------------------
    // Upper Level Friendly Routines
    //
    // query/fetch incoming info
    //
    // id fields
    uint32_t getId() {return mId;}  // return Frame mId
    uint32_t getMask() { return mMask; }
    int getType() {return mFrType;}         // VCwBase::ID_UNDEF,_ERR,_NORM,_EXT,_FD
    // the data
    int getDataLen() {return mDataLen;}     // get length of data portion of frame
    int getData(uint8_t * pData, int maxlen); // copy out the data portion of frame
    uint64_t getTimestamp() {return mTimestamp;}
    // the interface ("canX")
    int getIface() {return mIfNo;};  // return interface number (e.g 0 for "can0")
    int getIfaceFlag() { return mFlag;}   //return interace name given by application (e.g vcan1)
    int isIface(int ifNo) {return mIfNo == ifNo;}
    //
    // the flags (usually Type is all you need, these detail it)
    int isErr() {return mErr != 0;}
    int isExt() {return mExt != 0;}
    int isRtr() {return ((mId & CAN_RTR_FLAG) != 0);}
    //
    // set methods for preparing an outgoing frame
    //
    // setData copies in the data, caller retains ownership of pData
    void setData(uint8_t * pData, int len); // sets both data and dataLen
    //
    // set the flags
    void setErr(int on=1);
    void setExt(int on=1);
    void setRtr() { mId |= CAN_RTR_FLAG; }
    void clearRtr() { mId &= ~CAN_RTR_FLAG; }
    void setMaskByType(uint32_t id, int typ = VCwBase::ID_NORM);
    //
    // set the ID field, also set the mFrType field
    void setIdByType(uint32_t newid, int typ = VCwBase::ID_NORM);

    // set the frame mFrType field
    void setType(int newtype = VCwBase::ID_NORM);

    // set the interface for this frame
    void setIface(int ifNo) {mIfNo = ifNo;}
    // set the interface flag for this frame    //to differ VCAN and CAN nodes
    void setIfaceFlag(int cflag){mFlag = cflag;}

    void logFrame(char * tag);    // send frame details to the log
    //
    //---------------------------------------------
    // the primary fields in the packet
protected:
    int mId;
    int mDataLen;
    uint8_t mData[VCwBase::MAX_DATA];
    uint64_t mTimestamp;
    int mFrType; // VCwBase::ID_UNDEF, _NORM, _EXT, _FD
    // flags
    int mExt;    // extended addressing frame (aka EFF)
    int mErr;    // error frame
    // useful adjunct info
    int mIfNo;
    int mFlag;
    uint32_t mMask;  // mask for id for this frame type (2^11-1 or 2^29-1)

    //---------------------------------------------------------------
    // INTERNAL USE ONLY Follows....
    //---------------------------------------------------------------
    // Lower Level (wire-side) friendly and internally used routines
    //
    // setFrame - will copy in the contents of the Frame
    //    - caller retains ownership and should delete it
public:
    // use the following methods internally and with caution
    void setFrame(can_frame * pFrame);      //obsolete
    void setFrame(canfd_frame * pFrame);
    //
    // getFrame - return a pointer at linux can_Frame structure,
    //    - VCwFrame owns (and will delete) this structure
    canfd_frame * getFrame();

    int getFrameLen();    // length of completed frame

    //
    void setFlagsFromId(uint32_t canId);    // set ext, rtr, err flags

    void setMask(uint32_t m)  {mMask = m;}
    void setIdMask(uint32_t newid, uint32_t nmask =VCwBase::MASK29);

    void setId(uint32_t id) { mId = id; }

    uint8_t * getDataPtr() {return mData;}
protected:
    canfd_frame * mpTempFrame;  // ownership is retained with this VCwFrame

private:
    void init(uint32_t nid = 0, int nlen = 0, uint8_t * pndata = 0,
            bool extFlag = false, bool rtrFlag = false,
            uint32_t timestamp = 0);
};

#endif /* VNCFRAME_H_ */
