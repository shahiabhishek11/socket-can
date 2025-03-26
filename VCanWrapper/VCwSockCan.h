/*
 * Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwSockCan.h
 * ------------------------------------------
 * This module, and VCwSock.h, smooth some of the socketCAN wrinkles.
 * It is composed of these parts:
 * 1) VCwSock - abstraction wrapping and maintaining a single SocketCan Socket
 *           - a VCwSock may also host a set of reader pThreads
 * 2) static VCwSock - useful calls interfacing SocketCan
 * 3) VCwSockCan - set of CAN Sockets and useful routines
 * -------------------------------------------
 * Sends:   use * pSendSock
 */

#ifndef CHSOCKCAN_H_
#define CHSOCKCAN_H_

#include "VCwBase.h"
#include "VCwNodeSet.h"
#include "VCwSock.h"

class VCwEarSet;
class VCwEar;

//////////////////////////////////////////////////////////
// VCwSockCan - this is a container for the VCwSocks
//
class VCwSockCan : public VCwNodeSet {
protected:
    VCwSockCan(uint8_t tSrc);
    virtual ~VCwSockCan();
    static VCwSockCan * pTheSockCan;  // singleton pointer

    int start(); // auto-called by the constructor

    int ifaceAll;    // set true if all interfaces used
public:
    // construction stuff
    //
    static VCwSockCan * getInstance(uint8_t tSrc = 0);

    void real(int is=1) {mDoReal = is; VCwSock::real(is);};
    //
    int startReaders();
    int startReadersCan();
    //
    // lookup stuff
    //
    int lookupInterface(char * pName) { return mPMiscSock->lookupInterface(pName);};
    int openInterface(char * pName);
    // return CAN interface number
    int getCanInterface(char * pname);
    //
    // sender
    //
    int send(VCwFrame * pFrame);
    int send_node(VCwFrame * pFrame);   //for custom can interface like vcan0 can10
    int releaseBufferedData(int ifNo = VCwBase::IFACE_ANY);

    VCwSock * findSockIface(int ifNo, int exact=0);    // return VCwSock which has this interface
    int enableBuffering(uint8_t ifNo, uint32_t frameId, uint32_t mask);
    int processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
            uint8_t type);
    int processFrameFilterCAN(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
            uint8_t type);
    // int processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
    //         uint8_t type, int flag);
protected:
    int mDoReal;

    int getIface(char * pname);    // return its num if it's already here
    int getIface(int ifNo);        // return zero if does not exist, else return 1
    //
    VCwSock * getSendSock(int ifNo, int createAndAdd=1); //get a socket which can send to interface ifNo
    //
    int mNumSocks;
//    virtual int add(VCwSock * pSock) {VCwBase::add(pSock); return ++mNumSocks;};
//    virtual int rem(VCwSock * pSock) {VCwBase::rem(pSock); return --mNumSocks;};

    int createDefaultInterfaces();    // $ToDO should we do this???
    int createDefaultInterfacesCan();   //added for creating can interfaces

    VCwSock * mPSendSock;    // for sendtos
    VCwSock * mPMiscSock;    // general purpose socket

    VCwSock * mPReaderSock;    // phase 1: the only reader sock
    VCwSock * cPReaderSock;
};
#endif /* CHSOCKCAN_H_ */
