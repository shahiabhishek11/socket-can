/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwWorker.h
 *  Can Worker Class - wraps a thread
 *
 *  Created on: Jul 14, 2014
 * -------------------------------------------------
 * -------------------------------------------------
 *
 */
#ifndef __VCwWorker_H__
#define __VCwWorker_H__

#include "VCwBlockingQueue.h"
#include "VCanWrapper.h"
#include "VCwLock.h"
#include <pthread.h>

//class VCwBlockingQueue;
//class VCwNode;

///////////////////////////////////////////////////////////////
// VCwWorker
///
class VCwWorker : public VCwLock {
public:
    static VCwWorker * create(VCwBlockingQueue * pQueue, int txMode = 0,
        CanQCallback cb = 0, void *userData = 0);

protected:
    VCwWorker(VCwBlockingQueue * pQueue, int txMode = 1);
    VCwWorker(VCwBlockingQueue * pQueue, int txMode = 0,
            CanQCallback cb = 0, void *userData = 0);
    virtual ~VCwWorker();

    friend void * startVCwWorkerThread(void * pArg);

    virtual int mainJobLoop(); // the main loop run by the mPosixThread

    VCwBlockingQueue * mPQueue;    // pointer at our queue

    void doSend(VCwNode * pNode);

    //$ToDo?$ //void doCallback(VCwNode * pNode);

    int mIsTxWorker;

    CanQCallback mQueueCb;

    void * mUserData;

private:
    pthread_t mPosixThread; // we are wrapping this Posix Thread
};
#endif //#ifndef __VCwWorker_H__
