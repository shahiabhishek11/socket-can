/*
 * Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

///////////////////////////////////////////////////////////////
// VCanWrapper.cpp - Vehicle Network Can [library] main file
//-------------------------------------------------------------
//
///////////////////////////////////////////////////////////////
#include "VCanWrapper.h"
#include "VCwSockCan.h"
#include "VCwFilterManager.h"
#include "VCwEarSet.h"
#include "VCwEar.h"
#include "VCwBlockingQueue.h"
#include "VCwSim.h"
#include "VCwWorker.h"
#include "VCwLock.h"
#include <string.h>
#include <linux/can.h>

#ifdef ENABLE_CANTRANSLATOR_LIB
#include <CanTranslator.h>
#endif
//#undef LOG_TAG
//#define LOG_TAG "CANLIB:VCanWrapper"

#define UNUSED(x) (void)(x)

///////////////////////////////////////////////////////////////
// Statics used by the VCanWrapper singleton
// - but not part of the VCanWrapper class
// - so these are not in the .h file which clients see
///////////////////////////////////////////////////////////////

// rev - current revision of this code
static float rev = 1.21;

// pTheVCanWrapper - singleton pointer
VCanWrapper * VCanWrapper::pTheVCanWrapper = 0;

// theVCanWrapperLock - lock used to arbitrate singleton creation
static VCwLock theVCanWrapperLock;

// numSendWorkers - VCanWrapper class creates these
static const int numSendWorkers = 1;

// sendQueue - the sendWorkers will read from this queue
static VCwBlockingQueue * pSendQueue;

// singletons:
VCwEarSet * pEarSet;  // set of Listeners registered, directs callbacks
VCwSockCan * pSockCan;// set of socketCAN sockets open, VCwSock does reading and sending
VCwSim * pSim;        // runs the simulate playlist
VCwFilterManager * pFilterManager;//  Frame Filter manager
VCwSettings * pSettings;

///////////////////////////////////////////////////////////////
// Constructors, Destructors, Init
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// VCanWrapper - CONSTRUCTor
VCanWrapper::VCanWrapper(int mode, uint8_t timeStampSrc) {
    init(mode, timeStampSrc);
}

///////////////////////////////////////////////////////////////
// VCanWrapper - DESTRUCTor
VCanWrapper::~VCanWrapper() {
    delete pSendQueue;  // this will kill all the workers also
}

int VCanWrapper::releaseBufferedData(int ifNo)
{
    int rc = -1;
    VCwSockCan *pSockCan = VCwSockCan::getInstance();
    if (pSockCan) {
        pSockCan->startReaders();
        rc = pSockCan->releaseBufferedData(ifNo);
    }
    return rc;
}

///////////////////////////////////////////////////////////////
// init - just called from above for now
///
/// NOTE: at startup we instantiate our primary service singletons
/// - these are assumed started and thus not NULL-checked in the APIs below
//
int VCanWrapper::init(int mode, uint8_t tSrc) {
    pSockCan = VCwSockCan::getInstance(tSrc);
    if (!pSockCan)
        return -1;
    pEarSet  = VCwEarSet::getInstance();
    pFilterManager = VCwFilterManager::getInstance();
    //
    pSendQueue = new VCwBlockingQueue(); // use default max entries
    for (int i = 0; i < numSendWorkers; i++) {
        VCwWorker::create(pSendQueue, 1);  // 1=doSends (not callbacks)
    }
    pSettings = VCwSettings::getInstance();
    if (pSettings) {
        pSettings->mLibMode = mode;
#ifdef ENABLE_CANTRANSLATOR_LIB
        if (mode != DRIVER_MODE_RAW_FRAMES) {
            if (pSettings->loadCanTranslatorLib() == true) {
                CAN_LOGE("Loading translation lib successful");
            } else {
                CAN_LOGE("Failed to load Translation lib");
            }
        }
#endif
    } else {
        CAN_LOGE("Failed to get instance of VCwSettings");
    }
    pSim = 0;
    real(1);    // start out assuming we are real (not sim)
    //doReal = 0;
    //
    return 0;
}

///////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// createWorkerQueue
// @brief  This function creates queue,establishes a callback ifunction with  Worker queue
// @parm numWorkers  Number of worker threads which would call the callback function
// @param cb - Callback function to be called upon receiving frames
// @parm userData - This is returned, unchanged, in the CanQCallback() function
// @desc This is the mechanism used to receive incoming messages over queue
//         It creates worker which calls this callback function on incoming frames
// @returns pointer to receiver queue
//
VCwBlockingQueue *VCanWrapper::createWorkerQueue(int numWorkers, CanQCallback cb, void *userData) {
    VCwBlockingQueue *pQueue = new VCwBlockingQueue();
    if (pQueue != NULL && cb != NULL) {
        for (int i = 0; i < numWorkers; i++) {
            VCwWorker::create(pQueue, 0, cb, userData);
        }
    }
    return pQueue;
}
///////////////////////////////////////////////////////////////
// real -
//
void VCanWrapper::real(int is) {
    mDoReal = is;
    pSockCan->real(is);
}

///////////////////////////////////////////////////////////////
// getRevision
///@returns float - v.rrr floating point version number
//
float VCanWrapper::getRevision() {
    return rev;
}

///////////////////////////////////////////////////////////////
/// getInstance - return pointer at the VCanWrapper singleton
///@returns &VCanWrapper - the VNCan Accessor Singleton
//
VCanWrapper * VCanWrapper::getInstance() {
    return getInstance(DRIVER_MODE_RAW_FRAMES, TIMESTAMP_SRC_CAN_CONTROLLER);
}

///////////////////////////////////////////////////////////////
/// getInstance - return pointer at the VCanWrapper singleton
/// in Raw frames or Property mode
///@returns &VCanWrapper - the VNCan Accessor Singleton
//
VCanWrapper * VCanWrapper::getInstance(int mode, uint8_t timeStampSrc) {
    theVCanWrapperLock.lock();
    CAN_LOGE("getInstance");
    if (!pTheVCanWrapper)  {
        pTheVCanWrapper = new VCanWrapper(mode,timeStampSrc);
    }
    theVCanWrapperLock.unlock();
    if (!pSockCan) {
        delete pTheVCanWrapper;
        return NULL;
    }
    CAN_LOGE("VCanWrapper getInstance(rev:\"%.2f\")", rev);
    return pTheVCanWrapper;
}

///////////////////////////////////////////////////////////////
/// getIface - open (if needed) return number for this string
///@brief return a logical id for a given CAN interface
///@parm psdev  a char string such as: "/dev/can1"
///   NULL gets the hard-coded default
///@returns int a logical number to use in ensuing calls
///   a negative return signifies an error, usually "Not Found"<br>
///   NOTE: this number is PROBALBY what socketCAN uses,
///   however, it might be one of the Meta ifaces:
///      e.g.: VCwBase::IFACE_ALL
///@desc This function returns a shortcut number for the client to use in the future <br>
///   The basic assumption is that the caller somehow "knows" what interfaces
///   he is interested in.  This is presumably found through a config file
///   or can simply be a natural guess (e.g. vcanX {X=0..n} for testing
//
int VCanWrapper::getIface(char * ps) {

    return pSockCan->lookupInterface(ps);
}

int VCanWrapper::enableBuffering(uint8_t ifNo, uint32_t canId, uint32_t mask) {
    CAN_LOGE("enableBuffering: %d 0x%x 0x%x", ifNo, canId, mask);
    return pSockCan->enableBuffering(ifNo, canId, mask);
}
///////////////////////////////////////////////////////////////
/// sendMessage
///@brief This function sends a can msg out a single interface.
///@parm frame pointer at the VCwFrame object
///          ownership of this object is passed to VCanWrapper
///@param ifNo = the interface number
///@returns 0 on success,  -1 on error
//
int VCanWrapper::sendMessage(VCwFrame * pFrame, int ifNo) {
    if (!pSockCan) {
        CAN_LOGE("Socket object not found\n");
        return -1;
    }
    CAN_LOGE("sendMessage");
#ifdef ENABLE_CANTRANSLATOR_LIB
    VCwNodeSet *inFrameList = new VCwNodeSet;
    if (inFrameList != NULL) {
        inFrameList->add(pFrame);
        if (sendMessages(inFrameList, ifNo) != 0) {
            CAN_LOGE("sendMessage failed \n");
            delete inFrameList;
            return -1;
        } else {
            return 0;
        }
    } else {
        CAN_LOGE("Allocation failed frame list\n");
        return -1;
    }
#else
    pFrame->setIface(ifNo);
    //pFrame->logFrame("VCanWrapper::SendMessage");
    return pSendQueue->enqueueNode(pFrame);
#endif
}
///////////////////////////////////////////////////////////////
/// sendMessage_node
///@brief This function sends a can msg out a single interface.
///@parm frame pointer at the VCwFrame object
///          ownership of this object is passed to VCanWrapper
///@param name = the interface name
///@returns 0 on success,  -1 on error
//
int VCanWrapper::sendMessage_flag(VCwFrame * pFrame, int cflag, int ifNo) {
    if (!pSockCan) {
        CAN_LOGE("Socket object not found\n");
        return -1;
    }
    CAN_LOGE("sendMessage_node");
#ifdef ENABLE_CANTRANSLATOR_LIB
    VCwNodeSet *inFrameList = new VCwNodeSet;
    if (inFrameList != NULL) {
        inFrameList->add(pFrame);
        if (sendMessages(inFrameList, ifNo) != 0) {
            CAN_LOGE("sendMessage failed \n");
            delete inFrameList;
            return -1;
        } else {
            return 0;
        }
    } else {
        CAN_LOGE("Allocation failed frame list\n");
        return -1;
    }
#else
    pFrame->setIfaceFlag(cflag);
    pFrame->setIface(ifNo);
    //pFrame->logFrame("VCanWrapper::SendMessage");
    return pSendQueue->enqueueNode(pFrame);
#endif
}
///////////////////////////////////////////////////////////////
/// sendMessages
///@brief This function sends list of frames out over a single interface.
///@parm pFrameList pointer at frame list object
///@param ifNo = the interface number
///@returns 0 on success, -1 on error
//
int VCanWrapper::sendMessages(VCwNodeSet *pFrameList, int ifNo) {
    int ret = 0;
    VCwFrame *pInFrame = NULL;
    VCwNodeSet *pSendFrameList = pFrameList;
    if (!pSockCan) {
        CAN_LOGE("Socket object not found\n");
        return -1;
    }
#ifdef ENABLE_CANTRANSLATOR_LIB
    VCwNodeSet *outFrameList = NULL;
    if (pSettings != NULL) {
        if (pSettings->mLibMode != DRIVER_MODE_RAW_FRAMES) {
            if (pSettings->mEncodeFramesFn != NULL) {
                if (pSettings->mEncodeFramesFn(pFrameList, &outFrameList, pSettings->mLibMode) != 0) {
                    CAN_LOGE("sendMessages:encodeFrames failed \n");
                    delete outFrameList;
                    return -1;
                } else {
                    pSendFrameList = outFrameList;
                }
            } else {
                CAN_LOGE("sendMessages:encodeFrames function not found\n");
                return -1;
            }
        }
    } else {
        CAN_LOGE("libSettings not found \n");
        return -1;
    }
#endif
    if (pSendFrameList != NULL) {
        while ((pInFrame = (VCwFrame *)pSendFrameList->rem()) != NULL){
            pInFrame->setIface(ifNo);
            //pInFrame->logFrame("VCanWrapper::SendMessages");
            ret = pSendQueue->enqueueNode(pInFrame);
            if (ret < 0)
                break;
        }
    } else {
        CAN_LOGE("sendMessages:frame list not found\n");
        ret = -1;
    }
    if (ret != 0)
        CAN_LOGE("sendMessages failed \n");
    return ret;
}

///////////////////////////////////////////////////////////////
/// register_listener
///@brief  This function establishes a callback function for a FrameID set
///@parm nid  use this to match incoming frame ID after anding the mask
///@parm mask incoming id with this, then match with parm: id,
///      note: hi-bit on means NOT matching!
///mask is ORed with CAN_RTR_FLAG so that the buffered frame doesn't invoke
///the live frame callback
///
///Eg: Incoming frame: 0x40000082 ; frIdMask: 0x7FF ; frIdMask | CAN_RTR_FLAG : 0x400007FF
///    Incoming frame & frIdMask = 0x40000082 & 0x7FF == 0x82 --- this will invoke the live
///    frame callback even though the incoming frame was a buffered frame.
///    If we OR the mask with CAN_RTR_FLAG
///    Incoming frame & (frIdMask | CAN_RTR_FLAG) = 0x40000082 & 0x400007FF == 0x40000082
///    This will ensure that only the buffered frame callback is called.
///@parm callbackFunc  called when matching msg is read
///      if callbackFunc == 0 then this id[,mask pair] is unregistered
///       note: this type is defined in VCwBase.h
///@parm userData - this is returned, unchanged, in the CanCallback() function
///@parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
///@parm filterType - frame can be requested onchange or onrefresh
///@returns int earId - unique ID used in the unregisterListener(earId) call
///@desc This is the mechanism used to receive incoming messages.
///      Note that the duple: {id,mask} must be unique.  That is,
///      if a new registerListener() is received with a duplicate {id,mask}
///      then the previous callback is over-written.
///      If the callback is 0, then the listener instance is removed.
//
int VCanWrapper::registerListener(uint32_t frameId, uint32_t frIdMask, CanCallback callbackFunc,
        void* userData, int ifNo, uint8_t filterType) {
            CAN_LOGE("registerListener");
    CAN_LOGE("registerListener: if:%i frame:0x%x mask:0x%x filtertype:%i", ifNo, frameId, frIdMask, filterType);
    int earId = pEarSet->add(frameId, frIdMask | CAN_RTR_FLAG, callbackFunc, userData, ifNo);
    CAN_LOGE("registerListener: 0x%x", earId);
    pFilterManager->add(ifNo, frameId, frIdMask | CAN_RTR_FLAG, filterType, earId);
    return earId;
}
///////////////////////////////////////////////////////////////
/// register_listener flag
///@brief  This function establishes a callback function for a FrameID set
///@parm nid  use this to match incoming frame ID after anding the mask
///@parm mask incoming id with this, then match with parm: id,
///      note: hi-bit on means NOT matching!
///mask is ORed with CAN_RTR_FLAG so that the buffered frame doesn't invoke
///the live frame callback
///
///Eg: Incoming frame: 0x40000082 ; frIdMask: 0x7FF ; frIdMask | CAN_RTR_FLAG : 0x400007FF
///    Incoming frame & frIdMask = 0x40000082 & 0x7FF == 0x82 --- this will invoke the live
///    frame callback even though the incoming frame was a buffered frame.
///    If we OR the mask with CAN_RTR_FLAG
///    Incoming frame & (frIdMask | CAN_RTR_FLAG) = 0x40000082 & 0x400007FF == 0x40000082
///    This will ensure that only the buffered frame callback is called.
///@parm callbackFunc  called when matching msg is read
///      if callbackFunc == 0 then this id[,mask pair] is unregistered
///       note: this type is defined in VCwBase.h
///@parm userData - this is returned, unchanged, in the CanCallback() function
///@parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
///@parm filterType - frame can be requested onchange or onrefresh
///@parm flag - to differ ifno of vcan nodes and can nodes
///@returns int earId - unique ID used in the unregisterListener(earId) call
///@desc This is the mechanism used to receive incoming messages.
///      Note that the duple: {id,mask} must be unique.  That is,
///      if a new registerListener() is received with a duplicate {id,mask}
///      then the previous callback is over-written.
///      If the callback is 0, then the listener instance is removed.
//
int VCanWrapper::registerListener_can(uint32_t frameId, uint32_t frIdMask, CanCallback_CAN callbackFunc,
        void* userData, int ifNo, uint8_t filterType) {
            CAN_LOGE("registerListener_can");
    CAN_LOGE("registerListener: if:%i frame:0x%x mask:0x%x filtertype:%i", ifNo, frameId, frIdMask, filterType);
    int earId = pEarSet->addCan(frameId, frIdMask | CAN_RTR_FLAG, callbackFunc, userData, ifNo);
    CAN_LOGE("registerListener: 0x%x", earId);
    // pFilterManager->add(ifNo, frameId, frIdMask | CAN_RTR_FLAG, filterType, earId, flag);
    pFilterManager->addCAN(ifNo, frameId, frIdMask | CAN_RTR_FLAG, filterType, earId);
    return earId;
}
///////////////////////////////////////////////////////////////
/// registerBufferedDataListener - using separate callback method for buffered frames.
/// We are differentiating the live frames and buffered frames using the RTR bit.
/// We OR the frame ID and frame mask with CAN_RTR_FLAG to indicate buffered frame.
///@brief This function establishes a callback function for the buffered frames.
///@parm frameId  use this to match incoming frame ID after anding the mask
///@parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
///e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
///note: frIdMask==0 will match all frames
///note: typically, for normal frames use VCwBase::MASK11,
///or VCwBase::MASK29 for extended address frames
///@parm callbackFunc  called when matching msg is read
///      if callbackFunc == 0 then this id[,mask pair] is unregistered
///       note: this type is defined in VCwBase.h
///@parm userData - this is returned, unchanged, in the CanCallback() function
///@parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
///@parm filterType - frame can be requested onchange or onrefresh
///@returns earId - unique ID used in the unregisterListener(earId) call
///@desc The application need to specify the frame IDs it wants to buffer using
///registerBufferedDataListener API and this function establishes a callback funtion
///for the buffered frames.
///      Note that the duple: {id,mask} must be unique.  That is,
///      if a new registerListener() is received with a duplicate {id,mask}
///      then the previous callback is over-written.
///      If the callback is 0, then the listener instance is removed.
//
int VCanWrapper::registerBufferedDataListener(uint32_t frameId, uint32_t frIdMask,
        CanCallback callbackFunc, void* userData, int ifNo, uint8_t filterType) {
    CAN_LOGE("registerBufferedDataListener: if:%i frame:0x%x mask:0x%x filtertype:%i",
          ifNo, frameId | CAN_RTR_FLAG, frIdMask | CAN_RTR_FLAG, filterType);
    int earId = pEarSet->add(frameId | CAN_RTR_FLAG, frIdMask | CAN_RTR_FLAG,
                             callbackFunc, userData, ifNo);
    CAN_LOGE("registerBufferedDataListener: 0x%x", earId);
    return earId;
}

int VCanWrapper::updateListener(uint32_t frameId, uint32_t frIdMask, CanCallback callbackFunc,
        void* userData, int ifNo, uint8_t filterType) {
    UNUSED(callbackFunc);
    UNUSED(userData);
    CAN_LOGE("updateListener: if:%i frame:0x%x mask:0x%x filtertype:%i", ifNo, frameId, frIdMask, filterType);
    pFilterManager->update(ifNo, frameId, frIdMask, filterType);
    return 0;
}
///////////////////////////////////////////////////////////////
/// registerListener
///@brief  This function establishes a worker queue for a FrameID set
///@parm frameId use this to match incoming frame ID after anding the mask
///@parm frIdMask mask incoming id with this, then match with parm: id,
///      note: hi-bit on means NOT matching!
///mask is ORed with CAN_RTR_FLAG so that the buffered frame doesn't get queued to
///the live frame queue.
///
///Eg: Incoming frame: 0x40000082 ; frIdMask: 0x7FF ; frIdMask | CAN_RTR_FLAG : 0x400007FF
///    Incoming frame & frIdMask = 0x40000082 & 0x7FF == 0x82 --- this will queue the frame
///    to the live frame queue even though the incoming frame was a buffered frame.
///    If we OR the mask with CAN_RTR_FLAG
///    Incoming frame & (frIdMask | CAN_RTR_FLAG) = 0x40000082 & 0x400007FF == 0x40000082
///    This will ensure that the frame is queued only to the buffered frame queue.
///@parm rxQueue - Queue to enqueue frames received from socket listener thread
///@parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
///@parm filterType - frame can be requested onchange or onrefresh
///@returns int earId - unique ID used in the unregisterListener(earId) call
///@desc This is the mechanism used to receive incoming messages over queue
//
int VCanWrapper::registerListener(uint32_t frameId, uint32_t frIdMask,
        VCwBlockingQueue *rxQueue, int ifNo, uint8_t filterType) {
    CAN_LOGE("registerListener: if:%i frame:0x%x mask:0x%x filtertype:%i", ifNo,
            frameId, frIdMask, filterType);
    int earId = pEarSet->add(frameId, frIdMask, rxQueue, NULL, ifNo);
    CAN_LOGE("registerListener: 0x%x", earId);
    pFilterManager->add(ifNo, frameId, frIdMask, filterType, earId);
    return earId;
}

///////////////////////////////////////////////////////////////
/// registerBufferedDataListener - using queue based worker thread method for buffered frames.
/// We are differentiating the live frames and buffered frames using the RTR bit.
/// We OR the frame ID and frame mask with CAN_RTR_FLAG to indicate buffered frame.
///@brief This function establishes receiver queue for a FrameID
///@parm frameId  use this to match incoming frame ID after anding the mask
///@parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
///       e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
///       note: frIdMask==0 will match all frames
///       note: typically, for normal frames use VCwBase::MASK11,
///             or VCwBase::MASK29 for extended address frames
///@param rxQueue - Queue to enqueue frames received from socket listener thread
///@parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
///@parm filterType - frame can be requested onchange or onrefresh
///@returns earId - unique ID used in the unregisterListener(earId) call
///@desc The application need to specify the frame IDs it wants to buffer using
///registerBufferedDataListener API and this function is used to receive the buffered frames
///over queue.
///
int VCanWrapper::registerBufferedDataListener(uint32_t frameId, uint32_t frIdMask,
        VCwBlockingQueue *rxQueue, int ifNo, uint8_t filterType) {
    CAN_LOGE("registerBufferedDataListener: if:%i frame:0x%x mask:0x%x filtertype:%i", ifNo,
            frameId | CAN_RTR_FLAG, frIdMask | CAN_RTR_FLAG, filterType);
    int earId = pEarSet->add(frameId | CAN_RTR_FLAG, frIdMask | CAN_RTR_FLAG, rxQueue, NULL, ifNo);
    CAN_LOGE("registerBufferedDataListener: 0x%x", earId);
    return earId;
}

/*------------------------------------------------------------
 * unregisterListener - by earId as returned in registerListener
 */
int VCanWrapper::unregisterListener(int earId) {
    CAN_LOGE("unRegisterListener(earId: 0x%x)", earId);
    int ret;
    ret = pEarSet->rem(earId);
    if (ret != 0) {
        return ret;
    }
    ret = pFilterManager->remove(earId);
    return ret;
}
/*------------------------------------------------------------
 * unregisterListener_can - by earId as returned in unregisterListener_can
 */
int VCanWrapper::unregisterListener_can(int earId) {
    CAN_LOGE("unregisterListener_can(earId: 0x%x)", earId);
    int ret;
    ret = pEarSet->rem(earId);
    if (ret != 0) {
        return ret;
    }
    // ret = pFilterManager->remove(earId);
    return ret;
}

/*------------------------------------------------
 * simulate - start a simulate playback
 * @param mode - - a hook for future
 * @returns errorCode = 0 is okay
 */
int VCanWrapper::simulate(int mode, int delayMS) {
    CAN_LOGE("simulate (enable) called");
    if (!pSim) {
        pSim = VCwSim::getInstance();
        pSim->start(mode, delayMS);
    }
    return 0;
}

/*------------------------------------------------
 * unSimulate - STOP a simulate playback
 * @returns errorCode = 0 is okay
 */
int VCanWrapper::unSimulate() {
    CAN_LOGE("unSimulate [disable] called");
    int rc = 0;        // assume it will not be running when we exit
    if (pSim) {
        if (pSim->stop()) {
            CAN_LOGE("Stopped VCwSim");
        } else {
            CAN_LOGE("FAILED to stop VCwSim");
            rc = 1;
        }
    } else {
        CAN_LOGE("VCwSim already stopped");
    }
    return rc;
}
