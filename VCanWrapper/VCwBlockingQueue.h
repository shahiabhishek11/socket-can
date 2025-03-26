/*
 * Copyright (c) 2014-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwBlockingQueue.h
 *
 *  VCwBlockingQueue implements a C++ Version of the Java BlockingQueue
 *  It works with CwNodes as the items being queued
 *  and VCwWorkers which wrap the threads consuming CwNodes from the queue
 *
 *  enqueueNode(&VCwNode) returns int
 *      - is called to add a VCwNode to the queue
 *      - if there are VCwWorkers waiting,
 *          - then the front VCwWorker will immediately get the node
 *      - if no VCwWorkers waiting and the queue is not full
 *          - then the node is added to the queue
 *      - if the queue is full, then addNode will return a value of -1 = failure
 *          - VCwNode is not consumed in this case
 *  dequeueNode() returns &VCwNode
 *      - is called to get a VCwNode from the queue
 *      - if there are no CwNodes in the queue, execution will block until there is
 *      - note that VCwWorkers will be awoken in any order
 *  dequeueNodeSet(&VCwNodeSet, maxNodes) returns int
 *      - is called to get a list of CwNodes from the queue
 *      - can specify maximum nodes to be returned
 *      - returns error (value = -1) when maxNodes > maxEntryCount
 *  countNodes() returns int
 *      - return the size of the queue, number of Nodes to process
 */

#ifndef VCwBlockingQueue_H_
#define VCwBlockingQueue_H_

#include "VCwNode.h"
#include "VCwLock.h"
#include "VCwNodeSet.h"
#include <semaphore.h>

///////////////////////////////////////////////////////////////
// VCwBlockingQueue
//
class VCwBlockingQueue {
public:
    VCwBlockingQueue(int maxEntryCount = 1024);
    virtual ~VCwBlockingQueue();
    int enqueueNode(VCwNode * pNode);
    VCwNode * dequeueNode();
    int dequeueNodeSet(VCwNodeSet * pNodeSet, unsigned int maxNodes);
    int countNodes();
protected:
    VCwNodeSet mNodes;     // queue of CwNodes to process
    int mMaxEntryCount;
    sem_t mSemaphore;
};

#endif /* VCwBlockingQueue_H_ */
