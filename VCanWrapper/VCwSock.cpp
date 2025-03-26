/*
 * Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwSock.cpp
 *
 *  Created on: Jul 18, 2014
 *  ---------------------------------------
 *  see "VCwSockCan.h" for overview info ...
 *  ---------------------------------------
 *  Note this tutorial on the select() [psuedo-]system call
 *  http://man7.org/linux/man-pages/man2/select_tut.2.html
 *  --------------------------------------
 *  Each VCwSock can have 0 to max reader threads.
 *  The number of threads are decided by VCwSockCan when the lib is started.
 *  initReaders(num) is called to start them.
 */

//---------------------------
#include "VCwSock.h"
#include "VCwEarSet.h"
#include "VCwFrame.h"
#include "VCwNodeSet.h"

//---------------------------
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>   // for struct ifreq needed for lookupInterface()
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

#include <cutils/log.h>
#include <cutils/klog.h>

//#define KPI_DEBUG
#ifdef KPI_DEBUG
#define KPI_DEBUG_START_PID     0x7E
#define KPI_DEBUG_TEST_PID      0x18A
#define KPI_DEBUG_END_PID       0x81
#endif

//---------------------------------
//#undef  LOG_TAG
//#define LOG_TAG "**CANLIB:VCwSock**"
//---------------------------------

#define IOCTL_RELEASE_CAN_BUFFER        (SIOCDEVPRIVATE + 0)
#define IOCTL_ENABLE_BUFFERING    (SIOCDEVPRIVATE + 1)
#define IOCTL_ADD_FRAME_FILTER    (SIOCDEVPRIVATE + 2)
#define IOCTL_REMOVE_FRAME_FILTER    (SIOCDEVPRIVATE + 3)
#define IOCTL_UPDATE_FRAME_FILTER    (SIOCDEVPRIVATE + 4)

#define LIB_CAN_TRANSLATOR "libcantranslator.so"

#ifdef USE_GLIB
#define __packed __attribute__((packed))
#include <glib.h>
#define strlcpy g_strlcpy
#endif

struct sam4e_add_to_buffer {
    uint8_t can;
    uint32_t mid;
    uint32_t mask;
} __packed;

struct sam4e_frame_filter {
    uint8_t can;
    uint32_t mid;
    uint32_t mask;
    uint8_t type;
} __packed;


///////////////////////////////////////////////
// statics
//
// mIfNo2Sys - relative canX ifNo to system iface conversion table
int VCwSock::mIfNo2Sys[MAX_NUM_CAN_IFS] = {0};
int VCwSock::mVIfNo2Sys[MAX_NUM_VCAN_IFS] = {0};

///////////////////////////////////////////////
// beginthread - entry for the reader thread
//
void * startVCwSockThread(void * pArg) {
    CAN_LOGE("startVCwSockThread");
    if (pArg) {
        ((VCwSock *)pArg)->readerLoop();
    }
    return 0;
}
///////////////////////////////////////////////
// beginthread - entry for the reader thread for CAN interfaces
//
void * startCANCwSockThread(void * pArg) {
    CAN_LOGE("startCANCwSockThread");
    if (pArg) {
        ((VCwSock *)pArg)->readerLoopCan();
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// VCwSock - object methods  (VCwSockCan defined below)
///////////////////////////////////////////////////////////////

int VCwSock::mDoReal = 1;
uint8_t VCwSock::mTimeStampSrc = TIMESTAMP_SRC_SYS_BOOT_TIME; // Default from CAN controller

///////////////////////////////////////////////////////////////
// VCwSock - CONSTRUCTOR
//
VCwSock::VCwSock(int sock, int /*ifNo*/) {
    mSock = sock;
    mIfNo = mSysIfNo = 0;
    mReadersRunning = 0;
    mCanReadersRunning = 0;
    mCanReadersStopping = 0;
    mReadersStopping = 0;
    for (int i= 0; i<MAX_READERS; i++)
        readerThreadID[i] = 0;

    mpEarSet = VCwEarSet::getInstance();
    mpVCwSettings = VCwSettings::getInstance();
}

///////////////////////////////////////////////////////////////
// ~VCwSock - destroy this VCwSock object
//
VCwSock::~VCwSock() {
    mReadersStopping = 1;
    mCanReadersStopping = 1;
    close(mSock);

    for (int i=10; i && mReadersRunning; i--)
        sleep(100);    // give it a short moment to exit
}

///////////////////////////////////////////////////////////////
// initReaders
///@param num - number of readers to launch, should be <= MAX_READERS
///@returns num readers actually started
//
int VCwSock::initReaders(int num) {
    if (mReadersRunning) return 1;    // error

    readerNum = (num > MAX_READERS) ? MAX_READERS : num;

    for (int i=0; i<readerNum; i++) {
        startReader(i);
    }
    return mReadersRunning;
}
///////////////////////////////////////////////////////////////
// initReadersCan for can interfaces
///@param num - number of readers to launch, should be <= MAX_READERS
///@returns num readers actually started
//
int VCwSock::initReadersCan(int num) {
    if (mCanReadersRunning) return 1;    // error

    readerNumCan = (num > MAX_READERS) ? MAX_READERS : num;

    for (int i=0; i<readerNumCan; i++) {
        startReaderCan(i);
    }
    return mCanReadersRunning;
}

///////////////////////////////////////////////////////////////
// startReader - launch the reader thread
///@returns 1 = successfully running, else 0
//
int VCwSock::startReader(int idx) {
    CAN_LOGE("startReader");
    int x = mReadersRunning;
    pthread_create((pthread_t *)&readerThreadID[idx], 0, startVCwSockThread, this);
    return mReadersRunning - x;    // our delta hopefully
}
///////////////////////////////////////////////////////////////
// startReaderCan - launch the reader thread
///@returns 1 = successfully running, else 0
//
int VCwSock::startReaderCan(int idx) {
    CAN_LOGE("startReaderCan");
    int x = mCanReadersRunning;
    pthread_create((pthread_t *)&CanreaderThreadID[idx], 0, startCANCwSockThread, this);
    return mReadersRunning - x;    // our delta hopefully
}

///////////////////////////////////////////////////////////////
// runReader - read loop for this Socket
//
//$ToDo$ - how do I handle FDs and Extended Addresses?
//
int VCwSock::readerLoop() {
    CAN_LOGE("readerLoop");
    canfd_frame frame;
    int timestamp = 0;
    struct sockaddr_can addr;
    socklen_t len = sizeof(sockaddr_can);
    int lenCanFrame = sizeof(struct canfd_frame);
    //
    memset(&frame, 0, sizeof(canfd_frame));
    lock();
    mReadersRunning++;
    unlock();
    CAN_LOGE("Reader %d running, sizeof can_frame=%d", mReadersRunning, lenCanFrame);
    char controlMessageBuffer[CMSG_SPACE(sizeof(struct timeval))];
    struct iovec ioVec;
    struct msghdr message;
    struct cmsghdr *controlMessageHeader;
    ioVec.iov_base = &frame;
    message.msg_name = &addr;
    message.msg_iov = &ioVec;
    message.msg_iovlen = 1;
    message.msg_control = &controlMessageBuffer;
#ifdef KPI_DEBUG
    int rcount = 0;
#endif
    //
    while (!mReadersStopping) {
        CAN_LOGE("readerLoop");
        ioVec.iov_len = sizeof(frame);
        message.msg_namelen = sizeof(addr);
        message.msg_controllen = sizeof(controlMessageBuffer);
        message.msg_flags = 0;
        int tranferredBytes = recvmsg(mSock, &message, 0);
        // int ifNo = getRelativeInterfaceNumber(addr.can_ifindex);
        int ifNo = getRelativeInterfaceNumber(addr.can_ifindex);
        //$ToDo$ what do I look at?
        if (tranferredBytes > 0) { // good read?
#ifdef KPI_DEBUG
            if (ifNo == 0) {
                if (frame.can_id == KPI_DEBUG_START_PID) {
                    rcount = 0;
                    CAN_LOGE("START PID %x data:[%2x,%2x] rcount=%d txbytes=%d \n",
                        frame.can_id, frame.data[0],
                        frame.data[1],rcount, tranferredBytes);
                }
                rcount ++;
                if (frame.can_id == KPI_DEBUG_TEST_PID) {
                    CAN_LOGE("TEST PID %x data:[%2x,%2x] rcount=%d txbytes=%d \n",
                            frame.can_id, frame.data[0],
                            frame.data[1],rcount, tranferredBytes);
                    printMarker("Cwrapper:TestPID");
                }
                if (frame.can_id == KPI_DEBUG_END_PID) {
                    CAN_LOGE("END PID %x data:[%2x,%2x] rcount=%d txbytes=%d \n",
                    frame.can_id, frame.data[0],
                    frame.data[1],rcount, tranferredBytes);
                }
            }
#endif
            CAN_LOGE("Reader got rc:%d, len:%d {frid %" PRIx32", len:%" PRIu8"}",
                    tranferredBytes, len, frame.can_id, frame.len);
            CAN_LOGE("... data1:[%2x,%2x,%2x,%2x,",
                    frame.data[0],frame.data[1],frame.data[2],frame.data[3]);
            //CAN_LOGE("...                   data2: %2x,%2x,%2x,%2x]\n",
            //        frame.data[4],frame.data[5],frame.data[6],frame.data[7]);
            if (mTimeStampSrc == TIMESTAMP_SRC_SYS_BOOT_TIME) {
                struct timespec timenow;
                clock_gettime(CLOCK_BOOTTIME, &timenow);
                timestamp = timenow.tv_sec * 1000 + timenow.tv_nsec / 1000000; // in milliseconds
                CAN_LOGE("System Ts: %ld.%ld", timenow.tv_sec, timenow.tv_nsec / 1000000);
            } else {
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                controlMessageHeader = CMSG_FIRSTHDR(&message);
                if (controlMessageHeader != 0 && controlMessageHeader->cmsg_type == SO_TIMESTAMP) {
                    tv = *(struct timeval *) CMSG_DATA(controlMessageHeader);
                }
                CAN_LOGE("CAN controller Ts: %ld.%ld", tv.tv_sec, tv.tv_usec / 1000);
                timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000; // in milliseconds
            }
            //
            // EarSet holds the listeners and will walk thru it
            // .. calling them back with this frame and ifNo
            VCwFrame * pFrame = new VCwFrame(&frame, timestamp, ifNo);
#ifdef ENABLE_CANTRANSLATOR_LIB
            if (mpVCwSettings != NULL && mpVCwSettings->mDecodeFramesFn != NULL) {
                VCwNodeSet *outFrameList = NULL;
                VCwNodeSet *inFrameList = new VCwNodeSet;
                if (inFrameList != NULL) {
                    inFrameList->add((VCwNode *)pFrame);
                    if (mpVCwSettings->mDecodeFramesFn(inFrameList, &outFrameList, mpVCwSettings->mLibMode) != 0) {
                        CAN_LOGE("decodeFrame failed");
                        delete pFrame;
                    } else {
                        if (outFrameList != NULL) {
                            pFrame = (VCwFrame *)outFrameList->rem();
                            mpEarSet->walk(pFrame, ifNo);
                        }
                    }
                    delete outFrameList;
                } else {
                    CAN_LOGE("Allocation failed for framelist");
                    delete pFrame;
                }
            } else {
                mpEarSet->walk(pFrame, ifNo);
            }
#else
            mpEarSet->walk(pFrame, ifNo);
#endif

        }
    }
    lock();
    mReadersRunning--;
    unlock();
    //
    return 0;
}
///////////////////////////////////////////////////////////////
// runReader - read loop for this Socket CAN nodes
//
//$ToDo$ - how do I handle FDs and Extended Addresses?
//
int VCwSock::readerLoopCan() {
    CAN_LOGE("readerLoopCan");
    canfd_frame frame;
    int timestamp = 0;
    struct sockaddr_can addr;
    socklen_t len = sizeof(sockaddr_can);
    int lenCanFrame = sizeof(struct canfd_frame);
    //
    memset(&frame, 0, sizeof(canfd_frame));
    lock();
    mCanReadersRunning++;
    unlock();
    CAN_LOGE("Reader %d running, sizeof can_frame=%d", mCanReadersRunning, lenCanFrame);
    char controlMessageBuffer[CMSG_SPACE(sizeof(struct timeval))];
    struct iovec ioVec;
    struct msghdr message;
    struct cmsghdr *controlMessageHeader;
    ioVec.iov_base = &frame;
    message.msg_name = &addr;
    message.msg_iov = &ioVec;
    message.msg_iovlen = 1;
    message.msg_control = &controlMessageBuffer;
#ifdef KPI_DEBUG
    int rcount = 0;
#endif
    //
    while (!mReadersStopping) {
        CAN_LOGE("readerLoop");
        ioVec.iov_len = sizeof(frame);
        message.msg_namelen = sizeof(addr);
        message.msg_controllen = sizeof(controlMessageBuffer);
        message.msg_flags = 0;
        int tranferredBytes = recvmsg(mSock, &message, 0);
        // int ifNo = getRelativeInterfaceNumber(addr.can_ifindex);
        int ifNo = getRelativeInterfaceNumberCan(addr.can_ifindex);
        //$ToDo$ what do I look at?
        if (tranferredBytes > 0) { // good read?
#ifdef KPI_DEBUG
            if (ifNo == 0) {
                if (frame.can_id == KPI_DEBUG_START_PID) {
                    rcount = 0;
                    CAN_LOGE("START PID %x data:[%2x,%2x] rcount=%d txbytes=%d \n",
                        frame.can_id, frame.data[0],
                        frame.data[1],rcount, tranferredBytes);
                }
                rcount ++;
                if (frame.can_id == KPI_DEBUG_TEST_PID) {
                    CAN_LOGE("TEST PID %x data:[%2x,%2x] rcount=%d txbytes=%d \n",
                            frame.can_id, frame.data[0],
                            frame.data[1],rcount, tranferredBytes);
                    printMarker("Cwrapper:TestPID");
                }
                if (frame.can_id == KPI_DEBUG_END_PID) {
                    CAN_LOGE("END PID %x data:[%2x,%2x] rcount=%d txbytes=%d \n",
                    frame.can_id, frame.data[0],
                    frame.data[1],rcount, tranferredBytes);
                }
            }
#endif
            CAN_LOGE("Reader got rc:%d, len:%d {frid %" PRIx32", len:%" PRIu8"}",
                    tranferredBytes, len, frame.can_id, frame.len);
            CAN_LOGE("... data1:[%2x,%2x,%2x,%2x,",
                    frame.data[0],frame.data[1],frame.data[2],frame.data[3]);
            //CAN_LOGE("...                   data2: %2x,%2x,%2x,%2x]\n",
            //        frame.data[4],frame.data[5],frame.data[6],frame.data[7]);
            if (mTimeStampSrc == TIMESTAMP_SRC_SYS_BOOT_TIME) {
                struct timespec timenow;
                clock_gettime(CLOCK_BOOTTIME, &timenow);
                timestamp = timenow.tv_sec * 1000 + timenow.tv_nsec / 1000000; // in milliseconds
                CAN_LOGE("System Ts: %ld.%ld", timenow.tv_sec, timenow.tv_nsec / 1000000);
            } else {
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                controlMessageHeader = CMSG_FIRSTHDR(&message);
                if (controlMessageHeader != 0 && controlMessageHeader->cmsg_type == SO_TIMESTAMP) {
                    tv = *(struct timeval *) CMSG_DATA(controlMessageHeader);
                }
                CAN_LOGE("CAN controller Ts: %ld.%ld", tv.tv_sec, tv.tv_usec / 1000);
                timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000; // in milliseconds
            }
            //
            // EarSet holds the listeners and will walk thru it
            // .. calling them back with this frame and ifNo
            VCwFrame * pFrame = new VCwFrame(&frame, timestamp, ifNo);
#ifdef ENABLE_CANTRANSLATOR_LIB
            if (mpVCwSettings != NULL && mpVCwSettings->mDecodeFramesFn != NULL) {
                VCwNodeSet *outFrameList = NULL;
                VCwNodeSet *inFrameList = new VCwNodeSet;
                if (inFrameList != NULL) {
                    inFrameList->add((VCwNode *)pFrame);
                    if (mpVCwSettings->mDecodeFramesFn(inFrameList, &outFrameList, mpVCwSettings->mLibMode) != 0) {
                        CAN_LOGE("decodeFrame failed");
                        delete pFrame;
                    } else {
                        if (outFrameList != NULL) {
                            pFrame = (VCwFrame *)outFrameList->rem();
                            mpEarSet->walk(pFrame, ifNo);
                        }
                    }
                    delete outFrameList;
                } else {
                    CAN_LOGE("Allocation failed for framelist");
                    delete pFrame;
                }
            } else {
                mpEarSet->walk(pFrame, ifNo);
            }
#else
            mpEarSet->walkCAN(pFrame, ifNo);
#endif

        }
    }
    lock();
    mCanReadersRunning--;
    unlock();
    //
    return 0;
}
///////////////////////////////////////////////////////////////
/// handlesIface -
///@param ifNo  integer returned from socketCan for this interface
///@param exact if zero, then all IFACE_ANY/ALL to match
///@returns 1 if matched, else zero
//
bool VCwSock::handlesIface(int ifNo, int exact) {
    CAN_LOGE("handlesIface");
    // exact match?
    if (ifNo == mIfNo) {
        return true;
    }
    // Any as a param matches everything
    if (ifNo == VCwSock::IFACE_ANY) {
        return true;
    }
    // match if we are Any/All, if not asking for exact
    if ((!exact) && mIfNo == VCwSock::IFACE_ANY) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////
// IFNO Conversion routines //
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// buildIfno2sys - build the ifNo conversion table
//    Maps can relative interface number to system interface number
//    - the system interface number is used by socketCAN
//    - the relative number is used by our clients
//    - e.g. can'0' uses mIfNo2Sys[0]=3 means socketCAN calls 'vcan0' interface 3
//
void VCwSock::buildIfno2sys() {
    CAN_LOGE("buildIfno2sys");
    int i;
    char canstr[7] = {'v','c', 'a', 'n', '-', 0, 0};
    static int CNO = 4; // index into the place to put '0', etc.
    int s=0;
    // try up to max number of interfaces
    for(i=0; i<MAX_NUM_CAN_IFS; i++) {
        if (i < 10) {
            canstr[CNO] = i + '0';  // ascii-ize it
        } else {
            canstr[CNO]   = '0' + (i / 10); // tens digit
            canstr[CNO+1] = '0' + (i % 10); // ones digit
        }
        s = lookupInterface(canstr);
        mIfNo2Sys[i] = s;
        if (s>0) {
            CAN_LOGE("buildIfno2sys, map ifNo->sys{%d, %d}", i, s);
        }
    }
}
///////////////////////////////////////////////////////////////
// buildIfno2sys - build the ifNo conversion table for CAN interfaces
//    Maps can relative interface number to system interface number
//    - the system interface number is used by socketCAN
//    - the relative number is used by our clients
//    - e.g. can'0' uses mIfNo2Sys[0]=3 means socketCAN calls 'can0' interface 3
//
void VCwSock::buildIfno2sysCan() {
    CAN_LOGE("buildIfno2sys for CAN nodes");
    int i;
    char canstr[6] = {'c', 'a', 'n', '-', 0, 0};
    static int CNO = 3; // index into the place to put '0', etc.
    int s=0;
    // try up to max number of interfaces
    for(i=0; i<MAX_NUM_VCAN_IFS; i++) {
        if (i < 10) {
            canstr[CNO] = i + '0';  // ascii-ize it
        } else {
            canstr[CNO]   = '0' + (i / 10); // tens digit
            canstr[CNO+1] = '0' + (i % 10); // ones digit
        }
        s = lookupInterfaceCan(canstr);
        mVIfNo2Sys[i] = s;
        if (s>0) {
            CAN_LOGE("buildIfno2sys, map ifNo->sys{%d, %d}", i, s);
        }
    }
}

///////////////////////////////////////////////////////////////
// getNextIfNo -
///@param prevIfNo - start looking one AFTER this ifNo
///@return the next Relative IfNo AFTER prevIfNo or -1 if done
//
int VCwSock::getNextIfNo(int prevIfNo) {
    CAN_LOGE("getNextIfNo");
    if (prevIfNo < 0) {
        prevIfNo = -1;
    }
    int ifNo = prevIfNo + 1;    // start looking plus one
    int ret = -1;   // presume failure
    while ((ifNo < MAX_NUM_CAN_IFS) && (ret == -1)) {
        if (mIfNo2Sys[ifNo] != IFACE_UNDEF) {
            ret = ifNo;
        } else {
            ifNo++;
        }
    }
    return ret;
}

///////////////////////////////////////////////////////////////
// getsysifNo - return system ifNo for this logical ifNo
///@param ifNo - vcanX relative ifNo for this system ifNo
///@returns integer - system relative interface number
//        - returns VCwBase::IFACE_UNDEF if there is no such system iface for vcan
//        - note returns VCwBase::ANY if ifNo is special (VCwBase::IFAC_ANY or VCwBase:IFACE_ALL)
//
int VCwSock::getSysIfNo(int ifNo) {
    CAN_LOGE("getSysIfNo");
    if (ifNo < 0) {
        return ifNo;
    }
    if (ifNo < MAX_NUM_CAN_IFS) {
        return mIfNo2Sys[ifNo];
    }
    return VCwBase::IFACE_UNDEF;
}
///////////////////////////////////////////////////////////////
// getsysifNo - return system ifNo for this logical ifNo
///@param ifNo - canX relative ifNo for this system ifNo
///@returns integer - system relative interface number
//        - returns VCwBase::IFACE_UNDEF if there is no such system iface for can
//        - note returns VCwBase::ANY if ifNo is special (VCwBase::IFAC_ANY or VCwBase:IFACE_ALL)
//
int VCwSock::getSysIfNoCan(int ifNo) {
    CAN_LOGE("getSysIfNo");
    if (ifNo < 0) {
        return ifNo;
    }
    if (ifNo < MAX_NUM_VCAN_IFS) {
        return mVIfNo2Sys[ifNo];
    }
    return VCwBase::IFACE_UNDEF;
}
///////////////////////////////////////////////////////////////
// getRelativeIinterfaceNumber - return canX relative ifNo for this system ifNo
///@param sysifNo - system relative interface number
///@returns canX relative ifNo for this system ifNo
//
int VCwSock::getRelativeInterfaceNumber(int sysIfNo) {
    CAN_LOGE("getRelativeInterfaceNumber");
    for(int i=0; i<MAX_NUM_CAN_IFS; i++) {
        if (mIfNo2Sys[i] == sysIfNo) {
            return i;
        }
    }
    return VCwBase::IFACE_UNDEF;
}
///////////////////////////////////////////////////////////////
// getRelativeIinterfaceNumber - return canX relative ifNo for this system ifNo
///@param sysifNo - system relative interface number
///@returns canX relative ifNo for this system ifNo
//
int VCwSock::getRelativeInterfaceNumberCan(int sysIfNo) {
    CAN_LOGE("getRelativeInterfaceNumberCan");
    for(int i=0; i<MAX_NUM_VCAN_IFS; i++) {
        if (mVIfNo2Sys[i] == sysIfNo) {
            return i;
        }
    }
    return VCwBase::IFACE_UNDEF;
}   //still dont know what it does

///////////////////////////////////////////////////////////////
// VCwSock - static methods  (VCwSockCan defined below)
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// lookupInterface
///@param pname, name, something like:  "vcan0"
///@returns ifNo or IFACE_UNDEF=not found
//
int VCwSock::lookupInterface(char * pname) {
    CAN_LOGE("lookupInterface");
    struct ifreq ifr;
    ifr.ifr_ifindex = 0;
    strlcpy(ifr.ifr_name, pname, sizeof(ifr.ifr_name));
    int rc = ioctl(mSock, SIOCGIFINDEX, &ifr);
    int i = ifr.ifr_ifindex;
    //CAN_LOGE("lookupInterface(%s) returned: %d, index: %d", ifr.ifr_name, rc, i);
    //$ToDo:  what is "i" actually when not found??
    if (rc<0) {
        i = IFACE_UNDEF;
    }
    return i;
}
///////////////////////////////////////////////////////////////
// lookupInterfaceCan
///@param pname, name, something like:  "can0"
///@returns ifNo or IFACE_UNDEF=not found
//
int VCwSock::lookupInterfaceCan(char * pname) {
    CAN_LOGE("lookupInterface for CAN nodes");
    struct ifreq ifr;
    ifr.ifr_ifindex = 0;
    strlcpy(ifr.ifr_name, pname, sizeof(ifr.ifr_name));
    int rc = ioctl(mSock, SIOCGIFINDEX, &ifr);
    int i = ifr.ifr_ifindex;
    //CAN_LOGE("lookupInterface(%s) returned: %d, index: %d", ifr.ifr_name, rc, i);
    //$ToDo:  what is "i" actually when not found??
    if (rc<0) {
        i = IFACE_UNDEF;
    }
    return i;
}



///////////////////////////////////////////////////////////////
/// createRaw - create a "raw" socketcan socket (as opposed to BCM)
///@parm ifNo - socket returned from VCwSockCan::getCanInterface
//
VCwSock * VCwSock::createRaw(int ifNo) {
    CAN_LOGE("createRaw");
    //
    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    VCwSock * ps = new VCwSock(sock, ifNo);
    //
    sockaddr_can addr;
    // bind an interface
    // convert to physical equivilent
    int sysifNo = ps->getSysIfNo(ifNo);
    //don't bind to our special (negative) interfaces
    if (isIfaceSpecial(sysifNo)) {
        sysifNo = 0;    // he uses this as "any" interface
    }
    ps->mSysIfNo = sysifNo;
    // from the cluesheet ...
    addr.can_family = AF_CAN;
    addr.can_ifindex = sysifNo;

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    //

    const int timestampOn = 1;
    int rc = setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &timestampOn, sizeof(timestampOn));
    if (rc != 0) {
        CAN_LOGE("createRaw setsockopt returned %d %s", rc, strerror(errno));
    }

    // tell the RAW layer to alloc can fd
    const int canfd = 1;
    rc = setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd, sizeof(canfd));
    if (rc != 0) {
        CAN_LOGE("createRaw setsockopt returned %d %s", rc, strerror(errno));
    }

    if (ifNo < 0) {
        ifNo = ps->getNextIfNo(ifNo);
    }
    char ifName[6];
    snprintf(ifName, sizeof(ifName), "can%d", ifNo);

    if ((ps->lookupInterface(ifName)) == IFACE_UNDEF) {
        delete ps;
        close(sock);
        return NULL;
    }

    return ps;
}

int VCwSock::releaseCanBuffer(char * ifaceName) {
    CAN_LOGE("releaseCanBuffer %s", ifaceName);
    struct ifreq ifr;
    strlcpy(ifr.ifr_name, ifaceName, sizeof(ifr.ifr_name));
    int driverMode = DRIVER_MODE_RAW_FRAMES;
    ifr.ifr_data = &driverMode;
    int rc = ioctl(mSock, IOCTL_RELEASE_CAN_BUFFER, &ifr);
    if (rc != 0) {
        CAN_LOGE("releaseCanBuffer ioctl returned error: %d %s", rc, strerror(errno));
    }
    return rc;
}

int VCwSock::enableBufferingIoctl(uint8_t ifNo, uint32_t frameId, uint32_t mask) {
    struct ifreq ifr;
    char ifName[6];
    int rc = -1;
    struct sam4e_add_to_buffer buff_id;

#ifdef ENABLE_CANTRANSLATOR_LIB
    if (mpVCwSettings != NULL && mpVCwSettings->mEncodeFrameIdsFn != NULL) {
        CAN_LOGE("Enable translation for frameId %x", frameId);
        if (mpVCwSettings->mEncodeFrameIdsFn(mpVCwSettings->mLibMode, &frameId) != 0) {
            CAN_LOGE("encodeFrame failed");
            return rc;
        } else {
            CAN_LOGE("mEncodeFrameIdsFn success %x", frameId);
        }
    } else {
        CAN_LOGE("Translation lib is NULL or EncodeFrameIdsFn is NULL");
    }
#endif

    snprintf(ifName, sizeof(ifName), "can%d", ifNo);
    strlcpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));
    CAN_LOGE("enableBuffering %d 0x%x 0x%x", ifNo, frameId, mask);
    buff_id.can = ifNo;
    buff_id.mid = frameId;
    buff_id.mask = mask;
    ifr.ifr_data = &buff_id;

    rc = ioctl(mSock, IOCTL_ENABLE_BUFFERING, &ifr);
    if (rc != 0) {
        CAN_LOGE("enableBuffering ioctl returned error: %d %s", rc, strerror(errno));
    }
    return rc;
}

int VCwSock::enableBuffering(uint8_t ifNo, uint32_t frameId, uint32_t mask) {
    int rc = -1;
    uint8_t i;
    CAN_LOGE("enableBuffering %d 0x%x 0x%x", ifNo, frameId, mask);
    if (ifNo == (uint8_t) VCwBase::IFACE_ANY) {
        for (i = 0; i < MAX_NUM_CAN_IFS && mIfNo2Sys[i] != VCwBase::IFACE_UNDEF; i++) {
            rc = enableBufferingIoctl(i, frameId, mask);
            if (rc != 0)
                return rc;
        }
    } else if (ifNo == (uint8_t) VCwBase::IFACE_UNDEF) {
        CAN_LOGE("VCwSock::enableBuffering - undefined interface number\n");
    } else {
        rc = enableBufferingIoctl(ifNo, frameId, mask);
    }
    return rc;
}

int VCwSock::processFrameFilter(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
        uint8_t type) {
    struct ifreq ifr;
    char ifName[6];
    int action, rc = -1;
    uint8_t i = 0;
    struct sam4e_frame_filter filter;

    switch (cmd) {
        case FRAME_FILTER_CMD_REMOVE:
            action = IOCTL_REMOVE_FRAME_FILTER;
            break;
        case FRAME_FILTER_CMD_ADD:
            action = IOCTL_ADD_FRAME_FILTER;
            break;
        case FRAME_FILTER_CMD_UPDATE:
            action = IOCTL_UPDATE_FRAME_FILTER;
            break;
        default:
            CAN_LOGE("VCwSock::processFrameFilter unrecognized cmd %i\n", cmd);
            return rc;
    }


    if (ifNo == (uint8_t) VCwBase::IFACE_ANY) {
        for (i=0; i < MAX_NUM_CAN_IFS && mIfNo2Sys[i] != VCwBase::IFACE_UNDEF; i++) {
           snprintf(ifName, sizeof(ifName), "vcan%d", i);
           strlcpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));

           CAN_LOGE("VCwSock::processFrameFilter %s ifaceNo:%i   frameId:0x%x   mask:0x%x  type:%i\n",
                                cmd ? "add/update" : "remove", i, frameId, mask, type);
           filter.can = i;
           filter.mid = frameId;
           filter.mask = mask;
           filter.type = type;
           ifr.ifr_data = &filter;
           rc = ioctl(mSock, action, &ifr);
           if (rc != 0) {
               CAN_LOGE("processFrameFilter ioctl for ifNo %d returned error: %d %s\n", i, rc, strerror(errno));
           }
       }
   } else if (ifNo == (uint8_t) VCwBase::IFACE_UNDEF) {
        CAN_LOGE("VCwSock::processFrameFilter - undefined interface number\n");
   } else {
        snprintf(ifName, sizeof(ifName), "vcan%d", ifNo);
        strlcpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));

        CAN_LOGE("VCwSock::processFrameFilter %s ifaceNo:%i   frameId:0x%x   mask:0x%x  type:%i\n",
                                cmd ? "add/update" : "remove", ifNo, frameId, mask, type);

        filter.can = ifNo;
        filter.mid = frameId;
        filter.mask = mask;
        filter.type = type;
        ifr.ifr_data = &filter;
        rc = ioctl(mSock, action, &ifr);
        if (rc != 0) {
            CAN_LOGE("processFrameFilter ioctl for ifNo %d returned error: %d %s\n", ifNo, rc, strerror(errno));
        }
   }

   return rc;
}
// ////////////////////////////////////////////////////////
// //testing methods                                   ////
// ////////////////////////////////////////////////////////
int VCwSock::processFrameFilterCAN(uint8_t cmd, uint8_t ifNo, uint32_t frameId, uint32_t mask,
        uint8_t type) {
    struct ifreq ifr;
    char ifName[6];
    int action, rc = -1;
    uint8_t i = 0;
    struct sam4e_frame_filter filter;

    switch (cmd) {
        case FRAME_FILTER_CMD_REMOVE:
            action = IOCTL_REMOVE_FRAME_FILTER;
            break;
        case FRAME_FILTER_CMD_ADD:
            action = IOCTL_ADD_FRAME_FILTER;
            break;
        case FRAME_FILTER_CMD_UPDATE:
            action = IOCTL_UPDATE_FRAME_FILTER;
            break;
        default:
            CAN_LOGE("VCwSock::processFrameFilter unrecognized cmd %i\n", cmd);
            return rc;
    }


    if (ifNo == (uint8_t) VCwBase::IFACE_ANY) {
        for (i=0; i < MAX_NUM_VCAN_IFS && mVIfNo2Sys[i] != VCwBase::IFACE_UNDEF; i++) {
           snprintf(ifName, sizeof(ifName), "can%d", i);
           strlcpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));

           CAN_LOGE("VCwSock::processFrameFilter %s ifaceNo:%i   frameId:0x%x   mask:0x%x  type:%i\n",
                                cmd ? "add/update" : "remove", i, frameId, mask, type);
           filter.can = i;
           filter.mid = frameId;
           filter.mask = mask;
           filter.type = type;
           ifr.ifr_data = &filter;
           rc = ioctl(mSock, action, &ifr);
           if (rc != 0) {
               CAN_LOGE("processFrameFilter ioctl for ifNo %d returned error: %d %s\n", i, rc, strerror(errno));
           }
       }
   } else if (ifNo == (uint8_t) VCwBase::IFACE_UNDEF) {
        CAN_LOGE("VCwSock::processFrameFilter - undefined interface number\n");
   } else {
        snprintf(ifName, sizeof(ifName), "can%d", ifNo);
        strlcpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));

        CAN_LOGE("VCwSock::processFrameFilter %s ifaceNo:%i   frameId:0x%x   mask:0x%x  type:%i\n",
                                cmd ? "add/update" : "remove", ifNo, frameId, mask, type);

        filter.can = ifNo;
        filter.mid = frameId;
        filter.mask = mask;
        filter.type = type;
        ifr.ifr_data = &filter;
        rc = ioctl(mSock, action, &ifr);
        if (rc != 0) {
            CAN_LOGE("processFrameFilter ioctl for ifNo %d returned error: %d %s\n", ifNo, rc, strerror(errno));
        }
   }

   return rc;
}
///////////////////////////////////////////////////////////////
// send - send a can frame to the bus
//     - note- we assume ownership of the frame!!
//
int VCwSock::send(VCwFrame * pFrame) {
    CAN_LOGE("send");
    int len = 0;
    int ifNo = pFrame->getIface();
    bool sendToAll = false;
    //uint8_t * pData = pFrame->getDataPtr();
    //CAN_LOGE("send(frid:0x%x, ifNo:%d, data1:0x%x, data2, 0x%x)",
    //        pFrame->getId(), ifNo, *pData, *(pData+1));
    if (ifNo < 0) {
        ifNo = -1;  // this means send to all interfaces
        sendToAll = true;
    }
    do {
        // sendToAll will advance the relative IfNo to send to all CAN buses
        if (sendToAll) {
            ifNo = getNextIfNo(ifNo);
            if (ifNo < 0) {
                break;
            }
        }
        //    if ((ifNo == -1)||(ifNo==mIfNo)) {    // our default interface
        //        len = write(mSock, pFrame->getFrame(), pFrame->getFrameLen());
        //    } else {
        // fill in the "to" address struct for the sendto() call
        struct sockaddr_can addr;
        addr.can_ifindex = getSysIfNo(ifNo);
        addr.can_family  = AF_CAN;

        if (mDoReal) {
            canfd_frame * pCanFrame = pFrame->getFrame();
            CAN_LOGE("preparing message");
            len = sendto(mSock, pCanFrame, pFrame->getFrameLen(),
                             0, (struct sockaddr*)&addr, sizeof(addr));
            CAN_LOGE("sent to vcan%d",ifNo);
            //CAN_LOGE("Did sendto len=%d, canid:0x%x, ifNo was,to{%d,%d}",
            //        len, pCanFrame->can_id, ifNo, addr.can_ifindex);
            if (len < 0) {
                CAN_LOGE("VCwSock::send failed. Errno: %d %s\n", errno, strerror(errno));
            }
        } else {
            CAN_LOGE("FAKE sento(frid: 0x%x, ifNo:%d) call", pFrame->getId(), ifNo);
        }
    } while (sendToAll);
    delete pFrame;  // consume it
    //CAN_LOGE("exit send()");
    return (len == 0);    // return 1 if we wrote none (sigh - sorry)
}

///////////////////////////////////////////////////////////////
// send_name - send a can frame to the bus as rquested by application param
//     - note- we assume ownership of the frame!!
//
int VCwSock::send_name(VCwFrame * pFrame) {
    CAN_LOGE("send_name");
    int len = 0;
    int ifNo = pFrame->getIface();
    bool sendToAll = false;
    //uint8_t * pData = pFrame->getDataPtr();
    //CAN_LOGE("send(frid:0x%x, ifNo:%d, data1:0x%x, data2, 0x%x)",
    //        pFrame->getId(), ifNo, *pData, *(pData+1));
    if (ifNo < 0) {
        ifNo = -1;  // this means send to all interfaces
        sendToAll = true;
    }
    do {
        // sendToAll will advance the relative IfNo to send to all CAN buses
        if (sendToAll) {
            ifNo = getNextIfNo(ifNo);
            if (ifNo < 0) {
                break;
            }
        }
        //    if ((ifNo == -1)||(ifNo==mIfNo)) {    // our default interface
        //        len = write(mSock, pFrame->getFrame(), pFrame->getFrameLen());
        //    } else {
        // fill in the "to" address struct for the sendto() call
        struct sockaddr_can addr;
        addr.can_ifindex = getSysIfNoCan(ifNo);
        addr.can_family  = AF_CAN;

        if (mDoReal) {
            canfd_frame * pCanFrame = pFrame->getFrame();
            CAN_LOGE("preparing message");
            len = sendto(mSock, pCanFrame, pFrame->getFrameLen(),
                             0, (struct sockaddr*)&addr, sizeof(addr));
            CAN_LOGE("sent to can%d",ifNo);
            //CAN_LOGE("Did sendto len=%d, canid:0x%x, ifNo was,to{%d,%d}",
            //        len, pCanFrame->can_id, ifNo, addr.can_ifindex);
            if (len < 0) {
                CAN_LOGE("VCwSock::send failed. Errno: %d %s\n", errno, strerror(errno));
            }
        } else {
            CAN_LOGE("FAKE sento(frid: 0x%x, ifNo:%d) call", pFrame->getId(), ifNo);
        }
    } while (sendToAll);
    delete pFrame;  // consume it
    //CAN_LOGE("exit send()");
    return (len == 0);    // return 1 if we wrote none (sigh - sorry)
}
