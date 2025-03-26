/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwWorker.cpp
 *  Can Worker Class - wraps a thread
 * -------------------------------------------------
 *
 */
#include "VCwWorker.h"
#include "VCwSockCan.h"

static VCwSockCan * pSockCan = 0;
//defined for load balancing, limit nodes dequeued by worker thread.
#define DEQUEUE_NODE_LIMIT  8
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// VCwWorker
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// startVCwWorkerThread - takes a VCwWorker pointer as an arg
//

void * startVCwWorkerThread(void * pArg) {
    CAN_LOGE("startVCwWorkerThread");
    if (pArg) {
        ((VCwWorker*)pArg)->mainJobLoop();
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// VCwWorker - Constructor
//
VCwWorker::VCwWorker(VCwBlockingQueue * pQueue, int txMode) {
    mPQueue = pQueue;
    mIsTxWorker = txMode;
    mPosixThread = 0;
    mQueueCb = NULL;
    mUserData = NULL;

    if (!pSockCan) {
        pSockCan = VCwSockCan::getInstance();
    }
}

///////////////////////////////////////////////////////////////
// VCwWorker - Constructor with callback function
//
VCwWorker::VCwWorker(VCwBlockingQueue * pQueue, int txMode,
        CanQCallback cb, void *userData) {
            CAN_LOGE("VCwWorker");
    mPQueue = pQueue;
    mIsTxWorker = txMode;
    mPosixThread = 0;
    mQueueCb = cb;
    mUserData = userData;

    if (!pSockCan) {
        pSockCan = VCwSockCan::getInstance();
    }
}

///////////////////////////////////////////////////////////////
// ~VCwWorker - destructor
//
VCwWorker::~VCwWorker() {
    //pthread_cancel(mPosixThread);
}

///////////////////////////////////////////////////////////////
// create - STATIC - the indirect constructor
//
VCwWorker * VCwWorker::create(VCwBlockingQueue * pQueue, int txMode,
        CanQCallback cb, void * userData) {
    // make a configured worker first
    CAN_LOGE("create");
    VCwWorker * pWorker = new VCwWorker(pQueue, txMode, cb, userData);
    // then inject the thread into him
    if (!pWorker)
        return NULL;
    pthread_create(&(pWorker->mPosixThread), // pthread_t, thread id
            0,              // thread attributes???  no idea what these are
            startVCwWorkerThread,    // entry point
            pWorker          // sole arg to entry point
            );
    return pWorker;
}

///////////////////////////////////////////////////////////////
// mainJobLoop -
//
int VCwWorker::mainJobLoop() {
    CAN_LOGE("mainJobLoop");
    while (1) {
        if (mPQueue != NULL) {
            // mIsTxWorker = 1 for Transmit worker,mIsTxWorker = 0 for Receive worker
            if (mIsTxWorker) {
                VCwNode * pNode = mPQueue->dequeueNode();
                if (!pNode) {
                    break;  // from while loop, we are being destroyed
                }
                doSend(pNode);
            } else {
                VCwNodeSet nodeSet;
                VCwNode * pNode;
                int numFrames = mPQueue->dequeueNodeSet(&nodeSet, DEQUEUE_NODE_LIMIT);
                if (numFrames > 0) {
                    if (mQueueCb != NULL) {
                        (*mQueueCb)(&nodeSet, mUserData, numFrames);
                    } else {
                        while ((pNode = nodeSet.rem()) != NULL) {
                            delete pNode;
                        }
                    }
                }
            }
        } else {
            return 1;
        }
    }
    return 1;
}

///////////////////////////////////////////////////////////////
// doSend - call VCwSockCan to get it sent
//    - this is separate to make it easy to override
//    - at this point we accept that we are handling VCwFrames
//
void VCwWorker::doSend(VCwNode * pNode) {
    CAN_LOGE("doSend");
    VCwFrame* frame = (VCwFrame *)pNode;
    int cf = frame->getIfaceFlag();
    if (cf == 0){
        CAN_LOGE("flag is zero so sending to VCAN nodes");
        pSockCan->send((VCwFrame *)pNode);
    }else{
        CAN_LOGE("flag is one so sending to CAN node");
        pSockCan->send_node((VCwFrame *)pNode);
    }
    // pSockCan->send((VCwFrame *)pNode);
}
