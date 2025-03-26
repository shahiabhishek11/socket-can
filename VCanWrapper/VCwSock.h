/*
 * Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwSockCan.h
 * ------------------------------------------
 *  Created on: Jul 18, 2014
 * ------------------------------------------
 * This module smoothes some of the socketCAN wrinkles.
 * It is composed of 4 parts:
 * 1) VCwSock - abstraction wrapping and maintaining a SocketCan Socket
 * 2) static VCwSock - useful calls interfacing SocketCan
 * 3) VCwSockCan - set of CAN Sockets and useful routines
 * -------------------------------------------
 * Sends:   use * pSendSock
 */

#ifndef CHSOCK_H_
#define CHSOCK_H_

#include "VCwNodeSet.h"
#include "VCwSettings.h"

#define FRAME_FILTER_CMD_REMOVE 0
#define FRAME_FILTER_CMD_ADD    1
#define FRAME_FILTER_CMD_UPDATE 2

class VCwEarSet;
class VCwFrame;


//////////////////////////////////////////////////////////
// VCwSock - A VCanWrapper Socket
//
class VCwSock : public VCwBase, public VCwNodeSet {
    VCwSock(int sock = 0, int ifNo = 0);
    virtual ~VCwSock();
public:
    static VCwSock * createRaw(int ifNo);
    //
    // reader control
    //
    int initReaders(int numThreads =1);
    int initReadersCan(int numThreads =1);
    int stopReader();
    //
    // ifNo and interface calls
    //
    void buildIfno2sys();    // build the ifNo conversion table
    int getSysIfNo(int ifNo);    // return system ifNo for this logical ifNo
    int getRelativeInterfaceNumber(int sysifNo); // return canX relative ifNo for this system ifNo
    int lookupInterface(char * pname);    // lookup this interface and return ifNo or -1=not_found
    int getNextIfNo(int prevIfNo);  // return the next Relative IfNo AFTER prevIfNo
    // return true if this socket handles that ifNo
    // - note that ifNo can be one of the "special" VCwBase::IFACE_WOW
    bool handlesIface(int ifNo, int exact =0);
    //
    // ifNo and CAN interface calls
    //
    void buildIfno2sysCan();    // build the ifNo CAN conversion table
    int getSysIfNoCan(int ifNo);    // return system ifNo for this logical ifNo
    int getRelativeInterfaceNumberCan(int sysifNo); // return canX relative ifNo for this system ifNo
    int lookupInterfaceCan(char * pname);    // lookup this interface and return ifNo or -1=not_found
    // int getNextIfNo(int prevIfNo);  // return the next Relative IfNo AFTER prevIfNo
    // return true if this socket handles that ifNo
    // - note that ifNo can be one of the "special" VCwBase::IFACE_WOW
    // bool handlesIface(int ifNo, int exact =0);
    //
    // THE BREAD and BUTTER calls
    //
    int send(VCwFrame * pFrame); // write the frame to this socket

    //
    int send_name(VCwFrame * pFrame); // write the frame to this socket

    int releaseCanBuffer(char * ifaceName);
    static void real(int is) {mDoReal = is;};
    int enableBuffering(uint8_t ifNo, uint32_t frameId, uint32_t mask);
    int processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
            uint8_t type);
    int processFrameFilterCAN(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
            uint8_t type);
    // int processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
    //         uint8_t type, int flag);
private:
    int enableBufferingIoctl(uint8_t ifNo, uint32_t frameId, uint32_t mask);

protected:
    static int mDoReal;
    int mSock;

    VCwEarSet * mpEarSet;
    static const int MAX_NUM_CAN_IFS = 10;
    static const int MAX_NUM_VCAN_IFS = 10;
    static int mIfNo2Sys[MAX_NUM_CAN_IFS];
    static int mVIfNo2Sys[MAX_NUM_VCAN_IFS];

public:
    int mIfNo;        // this sock's relative interface number
    int mSysIfNo;    // this sock's system interface number
    static uint8_t mTimeStampSrc;// Timestamp source
protected:
    //
    // reader control
    //
    VCwSettings *mpVCwSettings;
    int mReadersRunning;    //
    int mCanReadersRunning;
    int mCanReadersStopping;
    int mReadersStopping;
    static const int MAX_READERS = 10;
    long int readerThreadID[MAX_READERS];
    long int CanreaderThreadID[MAX_READERS];
    int readerNum;        // number of readers, will equal mReadersRunning when fully init'd
    int readerNumCan;
    int startReader(int num);    // start up a reader
    int startReaderCan(int num);
    int readerLoop();
    int readerLoopCan();    //reader loop for CAN nodes
    friend void * startVCwSockThread(void * parg);
    friend void * startCANCwSockThread(void * arg);
};

#endif // CHSOCK_H_
