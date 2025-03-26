/*
 * Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwSockCan.cpp
 *
 */

//---------------------------
#include "VCwSockCan.h"
#include "VCwFrame.h"
#include "VCwEarSet.h"
//---------------------------
#include <linux/can.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>


///////////////////////////////////////////////////////////////
// VCwSockCan - static/Class Stuff
///////////////////////////////////////////////////////////////
// singleton pointer
VCwSockCan * VCwSockCan::pTheSockCan =0;

///////////////////////////////////////////////////////////////
// VCwSockCan - Object Stuff
///////////////////////////////////////////////////////////////
//#undef  LOG_TAG
//#define LOG_TAG "CANLIB:VCwSockCan"

///////////////////////////////////////////////////////////////
VCwSockCan::VCwSockCan(uint8_t tSrc) {
    mDoReal = 0;

    ifaceAll = 0;    // so far, non-specific
    mNumSocks = 0;
    VCwSock::mTimeStampSrc = tSrc;
    // make a socket for sends, lookups, etc
    mPSendSock = VCwSock::createRaw(0);
    mPMiscSock = VCwSock::createRaw(0);
    mPReaderSock = 0;    // wait for a startReaders() call
    cPReaderSock = 0;     // wait for startReadersCan() call

}

///////////////////////////////////////////////////////////////
VCwSockCan::~VCwSockCan() {
}

///////////////////////////////////////////////////////////////
// getInstance
//
VCwSockCan * VCwSockCan::getInstance(uint8_t tSrc) {
    if (!pTheSockCan) {
        pTheSockCan = new VCwSockCan(tSrc);
        if (!pTheSockCan->mPSendSock || !pTheSockCan->mPMiscSock) {
            CAN_LOGE("Either mPSendSock or mPMiscSock or both is NULL\n");
            delete pTheSockCan;
            return NULL;
        }
        pTheSockCan->createDefaultInterfaces();
        pTheSockCan->createDefaultInterfacesCan();
    }
    return pTheSockCan;
}
///////////////////////////////////////////////////////////////
// releaseBufferedData - release buffered frames to the application.
// @param ifNo - system interface number
//        if ifNo = VCwBase::IFACE_ANY, then it matches all the available
//           system interfaces
// @returns 0 = no error
//

int VCwSockCan::releaseBufferedData(int ifNo)
{
    int rc = 0;
    char ifName[6];
    bool releaseAll = false;

    if (ifNo == VCwBase::IFACE_ANY) {
        ifNo = -1;
        releaseAll = true;
        do {
            if (releaseAll) {
                ifNo = mPReaderSock->getNextIfNo(ifNo);
                if (ifNo < 0)
                    break;
            }
            snprintf(ifName, sizeof(ifName), "can%d", ifNo);
            rc = mPReaderSock->releaseCanBuffer(ifName);
            if (rc < 0) {
                return rc;
            }
        } while(releaseAll);
    } else if (ifNo == (uint8_t) VCwBase::IFACE_UNDEF) {
        CAN_LOGE("VCwSock::releaseBufferedData - undefined interface number\n");
        return -1;
    } else {
        snprintf(ifName, sizeof(ifName), "can%d", ifNo);
        return mPReaderSock->releaseCanBuffer(ifName);
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// startReaders - start readers. as needed,  to feed into this listener
// @param pEar - at new VCwEar to satisfy
// @returns 0 = no error
// @desc have a single VCwSock set up for the ear's interface
//
int VCwSockCan::startReaders() {
    CAN_LOGE("startReaders");
    if (!mPReaderSock && mDoReal) {
        // At this point we only have 1 reader for all interfaces
        mPReaderSock = VCwSock::createRaw(VCwSock::IFACE_ANY);

        // initialize one reader
        if (mPReaderSock) {
            mPReaderSock->initReaders(1);
        } else {
            CAN_LOGE("mPReaderSock is NULL\n");
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// startReadersCan - start readers. as needed,  to feed into this listener
// @param pEar - at new VCwEar to satisfy
// @returns 0 = no error
// @desc have a single VCwSock set up for the ear's interface
//
int VCwSockCan::startReadersCan() {
    CAN_LOGE("startReadersCan");
    if (!cPReaderSock && mDoReal) {
        // At this point we only have 1 reader for all interfaces
        cPReaderSock = VCwSock::createRaw(VCwSock::IFACE_ANY);

        // initialize one reader
        if (cPReaderSock) {
            cPReaderSock->initReadersCan(1);
        } else {
            CAN_LOGE("cPReaderSock is NULL\n");
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// createDefaultInterfaces
//
int VCwSockCan::createDefaultInterfaces() {
    CAN_LOGE("createDefaultInterfaces");
    mPSendSock = getSendSock(0);
    // build the interface number mapping table (Relative to System)
    if (mPSendSock) {
        mPSendSock->buildIfno2sys();
    } else {
        CAN_LOGE("mPSendSock is NULL\n");
    }
    return 0;
}
///////////////////////////////////////////////////////////////
// createDefaultInterfacesCan for CAN 
//
int VCwSockCan::createDefaultInterfacesCan() {
    CAN_LOGE("createDefaultInterfacesCan");
    mPSendSock = getSendSock(0);
    // build the interface number mapping table (Relative to System)
    if (mPSendSock) {
        mPSendSock->buildIfno2sysCan();
    } else {
        CAN_LOGE("mPSendSock is NULL\n");
    }
    return 0;
}
///////////////////////////////////////////////////////////////
// getCanInterface - build Ifr Struct for an interface lookup
// return the system unique interface number, or 0 = error
//
int VCwSockCan::getCanInterface(char * pname){
    return mPMiscSock->lookupInterface(pname);
}


///////////////////////////////////////////////////////////////
// findSockIface
///@oaram ifNo = system interface number
///@param exact = true means MUST be set for this ifNo, e.g. not IFACE_ANY
///@returns pSock = pointer at VCwSock object for that interface
//
VCwSock * VCwSockCan::findSockIface(int ifNo, int exact) {
    lock();
    VCwSock * pSock = 0;
    bool found = false;
    for (VCwNode * p = getNext(); !found && p; p = p->getNext()) {
        pSock = (VCwSock *)p;
        found = pSock->handlesIface(ifNo, exact);
    }
    unlock();
    return found ? pSock : 0;
}

int VCwSockCan::enableBuffering(uint8_t ifNo, uint32_t frameId, uint32_t mask) {
    CAN_LOGE("enableBuffering test ifNo:%d, frameId:0x%x, mask:0x%x\n", ifNo, frameId, mask);
    return mPMiscSock->enableBuffering(ifNo, frameId, mask);
}

/****************************************************************************
 * processFrameFilter for VCAN interface
 * @cmd     FRAME_FILTER_CMD_REMOVE FRAME_FILTER_CMD_ADD FRAME_FILTER_CMD_UPDATE
 * @ifNo    the can bus number
 * @frameId the frame pattern to filter
 * @mask    the bits that count in comparison
 * @type    FRAME_FILTER_TYPE_ON_REFRESH or FRAME_FILTER_TYPE_ON_CHANGE
*****************************************************************************/
int VCwSockCan::processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
        uint8_t type) {
    CAN_LOGE("processFrameFilter %s ifNo:%d, frameId:0x%x, mask:0x%x type:%i\n",
            cmd ? "add/update" : "remove", ifNo, frameId, mask, type);
    return mPMiscSock->processFrameFilter(cmd, ifNo, frameId, mask, type);
}
/****************************************************************************
 * processFrameFilter for CAN interface
 * @cmd     FRAME_FILTER_CMD_REMOVE FRAME_FILTER_CMD_ADD FRAME_FILTER_CMD_UPDATE
 * @ifNo    the can bus number
 * @frameId the frame pattern to filter
 * @mask    the bits that count in comparison
 * @type    FRAME_FILTER_TYPE_ON_REFRESH or FRAME_FILTER_TYPE_ON_CHANGE
*****************************************************************************/
int VCwSockCan::processFrameFilterCAN(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
        uint8_t type) {
    CAN_LOGE("processFrameFilter %s ifNo:%d, frameId:0x%x, mask:0x%x type:%i\n",
            cmd ? "add/update" : "remove", ifNo, frameId, mask, type);
    return mPMiscSock->processFrameFilterCAN(cmd, ifNo, frameId, mask, type);
}
// /****************************************************************************
//  * processFrameFilter flag wala
//  * @cmd     FRAME_FILTER_CMD_REMOVE FRAME_FILTER_CMD_ADD FRAME_FILTER_CMD_UPDATE
//  * @ifNo    the can bus number
//  * @frameId the frame pattern to filter
//  * @mask    the bits that count in comparison
//  * @type    FRAME_FILTER_TYPE_ON_REFRESH or FRAME_FILTER_TYPE_ON_CHANGE
// *****************************************************************************/
// int VCwSockCan::processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
//         uint8_t type, int flag) {
//     CAN_LOGE("processFrameFilter %s ifNo:%d, frameId:0x%x, mask:0x%x type:%i\n",
//             cmd ? "add/update" : "remove", ifNo, frameId, mask, type);
//     return mPMiscSock->processFrameFilter(cmd, ifNo, frameId, mask, type, flag);
// }

///////////////////////////////////////////////////////////////
// getIface
// return it's num if it's already here, or 0 if not
//
int VCwSockCan::getIface(char * pname) {
    CAN_LOGE("getIface param char");
    if (!pname) return 0;
    int ifNo = mPMiscSock->lookupInterface(pname);
    return getIface(ifNo);
}

///////////////////////////////////////////////////////////////
// getIface
// return 0 if not exist, else return ifNo
//
int VCwSockCan::getIface(int ifNo) {
    int found = 0;
    CAN_LOGE("getIface param int");
    if (ifNo) {
        lock();        // use base class's lock, only VCwSockCan accesses the list
        for (VCwSock * p = (VCwSock *)getNext(); p && !found; p = (VCwSock *)(p->getNext())) {
            if (p->handlesIface(ifNo)) {
                found = ifNo;
            }
        }
        unlock();
    }
    return found;
}

///////////////////////////////////////////////////////////////
// openInterface - open the interface Linux-named this
///@returns interface number or IFACE_UNDEF
//
int VCwSockCan::openInterface(char * pIfName) {
    //
    CAN_LOGE("openInterface");
    int i = getIface(pIfName);
    VCwSock * ps;
    if (!i) {
        i = mPMiscSock->lookupInterface(pIfName);
        if (i != VCwBase::IFACE_UNDEF) { // found the interface in socketCAN
            // have we created it yet?
            if (!getIface(i)) { // no, we need to create it
                ps = VCwSock::createRaw(i);
                add(ps);
            }
        }
    }
    return i;
}

///////////////////////////////////////////////////////////////
// send - send a VCwFrame out the requested CAN interface
///@pFrame - the VCwFrame as presented to VCanWrapper by the client
///@desc - we use a single send socket and which uses sendTo
//
int VCwSockCan::send(VCwFrame * pFrame) {
    CAN_LOGE("send method in VCwSockCan class");
    mPSendSock->send(pFrame);
    return 0;
}

///////////////////////////////////////////////////////////////
// send_node - send a VCwFrame out the requested CAN interface
///@pFrame - the VCwFrame as presented to VCanWrapper by the client
///@desc - we use a single send socket and which uses sendTo
//
int VCwSockCan::send_node(VCwFrame * pFrame) {
    mPSendSock->send_name(pFrame);
    return 0;
}

///////////////////////////////////////////////////////////////
// getSendSock - get a socket which can send to interface i
//
VCwSock * VCwSockCan::getSendSock(int ifNo, int createAndAdd) {    //
    CAN_LOGE("getSendSock");
    VCwSock * ps = (VCwSock *)getNext();
    while(ps && !ps->handlesIface(ifNo))
        ps = (VCwSock *)(ps->getNext());
    if(!ps && createAndAdd) {
        ps = VCwSock::createRaw(ifNo);
    }
    return ps;
}

