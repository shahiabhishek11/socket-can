/*
 * Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/*----------------------------------------------------------
 * VCanWrapper.h
 * ----------------------------------------------------------
 * This class is the "Front-End" to: libVCanWrapper.
 * It provides APIs to Send messages, get callbacks, and discern interfaces
 *
 * "libcwtest" demonstrates these APIs (if you have source)
 * or at least will exercise your installation.
 * "libcwtest -?" for help
 *
 * The test simulator is exposed for your convenience.
 */
#ifndef VCanWrapper_H_
#define VCanWrapper_H_

#include "VCwBase.h"
#include "VCwFrame.h"    // VCanWrapper sends and receives VCwFrames which wrap can data
#include "VCwBlockingQueue.h"
#include <android/log.h>
#define DRIVER_MODE_RAW_FRAMES 0
#define DRIVER_MODE_PROPERTIES 1
#define DRIVER_MODE_AMB 2

#define TIMESTAMP_SRC_CAN_CONTROLLER 0
#define TIMESTAMP_SRC_SYS_BOOT_TIME 1

/////////////////////////////////////
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define DBG 0
#if DBG == 1
#define CAN_LOGE ALOGE
#endif
#define CAN_LOGE ALOGV
#undef LOG_TAG
#define LOG_TAG "CANWRAPPER "

///////////////////////////////////////////////////////////////
// CanCallback - callback prototype - for receiving VCAN traffic
///@param pFrame - pointer at incoming VCwFrame object
///              - VCanWrapper retains ownership of VCwFrame
///@param userData - the unit32 passed into registerListener() for this callback
///@param ifNo - interface number this frame was received on
///            - e.g. it's 1 if the frame was received on "can1"
//
typedef void (*CanCallback)(VCwFrame* pFrame, void* userData, int ifNo);
//added callback prototype for receiving CAN node traffic ---TEST
// if not working add flag to differ from above callback for vcan node
typedef void (*CanCallback_CAN)(VCwFrame* pFrame, void* userData, int ifno, int flag);
typedef void (*CanQCallback)(VCwNodeSet *nodeSet, void *userData, int numFrames);

///////////////////////////////////////////////////////////////
// VCanWrapper - the Client interface singleton
//
class VCanWrapper {
private:  // VCanWrapper is a SINGLETON
    static VCanWrapper * pTheVCanWrapper;
    VCanWrapper(int mode, uint8_t timeStampSrc);
    ~VCanWrapper();   // we sustain a VCanWrapper until the creating process dissolves
    int init(int mode, uint8_t timeStampSrc);
public:
    /*----------------------------------------------------
     * getInstance()
     * @returns VCanWrapper * the VN Can Wrapper Singleton, in Raw Frames mode
     */
    static VCanWrapper * getInstance();

    /*----------------------------------------------------
     * getInstance()
	 * parm mode - RAW_FRAMES or PROPERTIES or AMB mode
	 * timeStampSrc - 0 -> CAN controller. This is default configuration
	 *              - 1 -> System Boot time
     * @returns VCanWrapper * the VN Can Wrapper for raw or FD frames
     */
    static VCanWrapper * getInstance(int mode, uint8_t timeStampSrc);

    /*------------------------------------------------------------
     * createWorkerQueue
     * @brief  This function creates queue, establishes a callback function with queue
     * @parm numWorkers  Number of worker threads which would call the callback function
     * @param cb - Callback function to be called upon receiving frames
     * @parm userData - this is returned, unchanged, in the CanQCallback() function
     * @returns pointer to receiver queue
     * @desc This is the mechanism used to receive incoming messages over queue
     *         It creates worker threads which call callback function on incoming frames
     */
    VCwBlockingQueue * createWorkerQueue(int numWorkers, CanQCallback cb,
            void *userData);

    /*------------------------------------------------------------
     * registerListener - using callback method
     * @brief  This function establishes a callback function for a FrameID set
     * @parm frameId  use this to match incoming frame ID after anding the mask
     * @parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
     *       e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
     *       note: frIdMask==0 will match all frames
     *       note: typically, for normal frames use VCwBase::MASK11,
     *             or VCwBase::MASK29 for extended address frames
     * @parm callbackFunc  called when matching msg is read
     *       note: this type is defined as CanCallback above
     * @parm userData - this is returned, unchanged, in the CanCallback() function
     * @parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @parm filterType - frame can be requested onchange or onrefresh
     * @returns earId - unique ID used in the unregisterListener(earId) call
     * @desc This is the mechanism used to receive incoming messages.
     *     >> Ownership of the VCwFrame in the Callback STAYS WITH VCanWrapper <<
     *       Copy what you need from the VCwFrame before you return to VCanWrapper.
     */
    int registerListener(uint32_t frameId, uint32_t frIdMask, CanCallback callbackFunc,
            void* userData = NULL, int ifNo = VCwBase::IFACE_ANY, uint8_t filterType = 0);

    /*------------------------------------------------------------
     * registerListener_can - using callback method for CAN nodes in VCANWRAPER
     * @brief  This function establishes a callback function for a FrameID set
     * @parm frameId  use this to match incoming frame ID after anding the mask
     * @parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
     *       e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
     *       note: frIdMask==0 will match all frames
     *       note: typically, for normal frames use VCwBase::MASK11,
     *             or VCwBase::MASK29 for extended address frames
     * @parm callbackFunc  called when matching msg is read
     *       note: this type is defined as CanCallback above
     * @parm userData - this is returned, unchanged, in the CanCallback() function
     * @parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @parm filterType - frame can be requested onchange or onrefresh
     * @returns earId - unique ID used in the unregisterListener(earId) call
     * @desc This is the mechanism used to receive incoming messages.
     *     >> Ownership of the VCwFrame in the Callback STAYS WITH VCanWrapper <<
     *       Copy what you need from the VCwFrame before you return to VCanWrapper.
     */
    int registerListener_can(uint32_t frameId, uint32_t frIdMask, CanCallback_CAN callbackFunc,
            void* userData = NULL, int ifNo = VCwBase::IFACE_ANY, uint8_t filterType = 0);
    /*------------------------------------------------------------
     * registerBufferedDataListener - using separate callback method for buffered frames.
     * @brief This function establishes a callback function for the buffered frames.
     * @parm frameId  use this to match incoming frame ID after anding the mask
     * @parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
     *       e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
     *       note: frIdMask==0 will match all frames
     *       note: typically, for normal frames use VCwBase::MASK11,
     *             or VCwBase::MASK29 for extended address frames
     * @parm callbackFunc  called when matching msg is read
     *       note: this type is defined as CanCallback above
     * @parm userData - this is returned, unchanged, in the CanCallback() function
     * @parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @parm filterType - frame can be requested onchange or onrefresh
     * @returns earId - unique ID used in the unregisterListener(earId) call
     * @desc The application need to specify the frame IDs it wants to buffer using
     * registerBufferedDataListener API and this function establishes a callback funtion
     * for the buffered frames.
     */
    int registerBufferedDataListener(uint32_t frameId, uint32_t frIdMask, CanCallback callbackFunc,
            void* userData = NULL, int ifNo = VCwBase::IFACE_ANY, uint8_t filterType = 0);
    /*------------------------------------------------------------
     * unregisterListener - by earId as returned in registerListener
     */
    int unregisterListener(int earId);
    /*------------------------------------------------------------
     * unregisterListener_can - by earId as returned in registerListener_can
     */
    int unregisterListener_can(int earId);
    /*------------------------------------------------------------
     * updateListener - update filter settings
     * @brief  This function updates a filter setting for a FrameID
     * @parm frameId  use this to match incoming frame ID after anding the mask
     * @parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
     * @parm callbackFunc  called when matching msg is read
     * @parm userData - this is returned, unchanged, in the CanCallback() function
     * @parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @parm filterType - frame can be requested onchange or onrefresh
     * @returns earId - unique ID used in the unregisterListener(earId) call
     */
    int updateListener(uint32_t frameId, uint32_t frIdMask, CanCallback callbackFunc,
            void* userData, int ifNo, uint8_t filterType);

    /*------------------------------------------------------------
     * registerListener - using queue based worker thread  method
     * @brief  This function establishes receiver queue for a FrameID
     * @parm frameId  use this to match incoming frame ID after anding the mask
     * @parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
     *       e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
     *       note: frIdMask==0 will match all frames
     *       note: typically, for normal frames use VCwBase::MASK11,
     *             or VCwBase::MASK29 for extended address frames
     * @param rxQueue - Receive Queue to which frames received from socket are pushed
     * @parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @parm filterType - frame can be requested onchange or onrefresh
     * @returns earId - unique ID used in the unregisterListener(earId) call
     * @desc This is the mechanism used to receive incoming messages over queue
     */
    int registerListener(uint32_t frameId, uint32_t frIdMask,
            VCwBlockingQueue *rxQueue, int ifNo = VCwBase::IFACE_ANY, uint8_t filterType = 0);

    /*------------------------------------------------------------
     * registerBufferedDataListener - using queue based worker thread method for buffered frames.
     * @brief This function establishes receiver queue for a FrameID
     * @parm frameId  use this to match incoming frame ID after anding the mask
     * @parm frIdMask is anded with frameId, then matched with incoming frameId & frIdMask,
     *       e.g:  match = (frameId & frIdMask) == (wire.frameId & frIdMask)
     *       note: frIdMask==0 will match all frames
     *       note: typically, for normal frames use VCwBase::MASK11,
     *             or VCwBase::MASK29 for extended address frames
     * @param rxQueue - Receive Queue to which frames received from socket are pushed
     * @parm ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @parm filterType - frame can be requested onchange or onrefresh
     * @returns earId - unique ID used in the unregisterListener(earId) call
     * @desc The application need to specify the frame IDs it wants to buffer using
     * registerBufferedDataListener API and this function is used to receive the buffered frames
     * over queue.
     */
    int registerBufferedDataListener(uint32_t frameId, uint32_t frIdMask,
            VCwBlockingQueue *rxQueue, int ifNo = VCwBase::IFACE_ANY, uint8_t filterType = 0);

    /* releaseBufferedData
     * @brief This functions releases buffered CAN frames to the applications
     * @param ifNo - logical interface (or special, e.g. VCwBase::IFACE_ANY)
     * @returns 0 on success and error code on failure.
     */
    int releaseBufferedData(int ifNo = VCwBase::IFACE_ANY);

    /*------------------------------------------------------------
     * sendMessage
     * @brief This function sends a can message frame out a single interface.
     * @param frame pointer to the VCwFrame object
     *      ownership of this object is passed to VCanWrapper on method success
     *      and VCwFrame object is deleted before returning.
     * @param ifNo = the interface number
     * @returns 0 = success, can message queued to be sent, frame accepted
     *          1 = error, message not sent, queue blocked by errors, frame not consumed
     *              ** the VCwFrame is still owned by caller in this condition
     * @desc it bears repeating:
     *    >> ownership of this object is passed to VCanWrapper except when error <<
     */
    int sendMessage(VCwFrame* frame, int ifNo);

        /*------------------------------------------------------------
     * sendMessage
     * @brief This function sends a can message frame out a single interface.
     * @param frame pointer to the VCwFrame object
     *      ownership of this object is passed to VCanWrapper on method success
     *      and VCwFrame object is deleted before returning.
     * @param ifNo = the interface number
     * @param cflag = if user application wants to send to CAN interface
     * @returns 0 = success, can message queued to be sent, frame accepted
     *          1 = error, message not sent, queue blocked by errors, frame not consumed
     *              ** the VCwFrame is still owned by caller in this condition
     * @desc it bears repeating:
     *    >> ownership of this object is passed to VCanWrapper except when error <<
     */
    int sendMessage_flag(VCwFrame* frame, int cflag, int ifNo);

    /*------------------------------------------------------------
     * sendMessages
     * @brief This function sends list of can message frames on a single interface.
     * @param  pointer to the VCwNodeSet object which points to list of VCwFrames object
     *      VCwFrame objects are deleted before returning.
     * @param ifNo = the interface number
     * @returns 0 = success, can messages queued to be sent, frames are accepted
     *          -1 = error, messages not sent, queue blocked by errors, frames are not consumed
     *              ** the pFrameList and its VCwFrames are still owned by caller in this condition
     * @desc it bears repeating:
     *    >> ownership of pFrameList object is passed to VCanWrapper except when error <<
     */
    int sendMessages(VCwNodeSet *pFrameList, int ifNo);

    /* ---------------------------------------------------
     * getRevision - return VCanWrapper version.revision
     * @returns float - in ver.rev format
     */
    float getRevision();

    ///////////////////////////////////////////////////////////////
    // Test calls follow
    /* ------------------------------------------------------------
     * "simulate()" starts the simulator running.
     * CanSim is the simulator, it loops sending the playlist of CanMessages
     *   as defined in CanSim.h.
     * The caller may wish to issue one or more registerListeners
     *   to get callbacks.
     * Note that calling registerListener with frIdMask=0 and ifNo=VCwBase::IFACE_ANY
     *   should allow you to receive all messages within the playlist.
     * "simulate()" works whether connected to a can bus or not
     *   and will intersperse messages with real messages, if they are arriving.
     * "real(0)" will turn off incoming "real" messages
     *   as well as cause outgoing (sendMessage() frames) to be dropped before the wire.
     */
    ///////////////////////////////////////////////////////////////

    /*------------------------------------------------
     * simulate - start a simulate playback
     * @param mode - - a hook for future
     * @param playBackDelayMS - sleep this between incoming can frames
     * @returns errorCode = 0 is okay
     */
    int simulate(int mode =0, int playbackDelayMs =1000);

    /*------------------------------------------------
     * unSimulate - STOP a simulate playback
     * @returns errorCode = 0 is okay
     */
    int unSimulate();

    /*------------------------------------------------------------
     * real - set the doReal flag here and in the key driving components (Boss & SockCan)
     */
    void real(int is =1);    // set to 0 to turn it off

    /*------------------------------------------------------------
     * getIface -- low-level test, check for existance of an interface!!
     * @brief return a logical id for a given CAN interface
     * @parm psdev  a char string such as: "/dev/can1"
     *    NULL gets the default
     * @returns int a logical number to use in ensuing calls
     *    a negative return signifies an error, usually "Not Found"<br>
     *    NOTE: this number is PROBALBY what socketCAN uses,
     *    however, it might be one of the Meta ifaces:
     *       e.g.: VCwBase::IFACE_ALL
     * @desc This function returns the system interface number. <br>
     *    This is solely for test or troubleshooting uses.
     */
    int getIface(char * psdev=0);
    //
    //$ ToDO:
    //$ findIfaces - causes us to look for any and all possible CAN interfaces
    //$ listIfaces - returns array of strings naming the interfaces
    //$ listIfaceNums - returns int array of valid interface numbers

    /*
     * Add this canId to the list of messages that should be buffered upon boot
     * Buffered messages will be released to the first client
     * who calls getInstance()
     */
    int enableBuffering(uint8_t ifNo, uint32_t canId, uint32_t mask);
protected:
    int mDoReal;  // flag used by the above sender & receivers to enable real bus access
};

#endif /* VCanWrapper_H_ */
