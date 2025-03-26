/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwBlockingQueue.cpp
 *
 */
#include "VCwBlockingQueue.h"
#include "VCanWrapper.h"
//---------------------------------
//---------------------------------


VCwBlockingQueue::VCwBlockingQueue(int maxEntryCount) {
    CAN_LOGE("VCwBlockingQueue");
    mMaxEntryCount = maxEntryCount;
    sem_init(&mSemaphore,0,0);   // create it locked (count of zero)
}

VCwBlockingQueue::~VCwBlockingQueue() {
    VCwNode * pNode;
    while ( (pNode = mNodes.rem()) ) {
        delete pNode;
    }
    // this should release waiting workers with no jobs to fetch
    sem_destroy(&mSemaphore);
}

int VCwBlockingQueue::enqueueNode(VCwNode * pNode) {
    CAN_LOGE("enqueueNode");
    if (mNodes.count() > mMaxEntryCount) {
        return -1; // return error when the queue is full
    }
    mNodes.add(pNode); // the jobs BaseSet is self locking
    sem_post(&mSemaphore);   // kick our semaphore ticking in jobs
    return 0;
}

VCwNode * VCwBlockingQueue::dequeueNode() {
    CAN_LOGE("dequeueNode");
    sem_wait(&mSemaphore);   // block until we have jobs
    VCwNode * pNode = mNodes.rem();
    return pNode;
}

int VCwBlockingQueue::dequeueNodeSet(VCwNodeSet *pNodeSet, unsigned int maxNodes) {
    VCwNode * pNode;
    if (maxNodes > (unsigned int) mMaxEntryCount) {
        return -1;
    }
    sem_wait(&mSemaphore);
    int i = 0;
    if (pNodeSet != NULL) {
        while (((unsigned int) i < maxNodes) && (pNode = mNodes.rem()) != NULL) {
            pNodeSet->add(pNode);
            i++;
        }
    } else {
        return -1;
    }
    return i;
}

int VCwBlockingQueue::countNodes(){
    return mNodes.count();
}
