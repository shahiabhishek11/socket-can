/*
 * Copyright (c) 2014-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwNodeSet.h
 *
 * Encapsulate linked list handling for CwNodes
 * Implements a set of objects of type VCwNode
 * Provides interfaces to add/remove VCwNode to/from the list
 */

#ifndef VCwNodeSet_H_
#define VCwNodeSet_H_

#include "VCwBase.h"
#include "VCwNode.h"
#include "VCwLock.h"
#include <semaphore.h>

////////////////////////////////////////
// VCwNodeSet
//
class VCwNodeSet: public VCwNode, public VCwLock {
public:
    // queue methods, FIFO, add to end, remove from front
    virtual void add(VCwNode *pNewTail);   // adds to the end of the queue (list)
    virtual VCwNode * rem(VCwNode *pObjOut = 0); // removes from the front of the queue (list)
    // stack methods, LIFO,
    virtual void push(VCwNode *pNewNext); // add (insert) to head of a stack
    virtual VCwNode * pop() {
        return rem();
    }; //remove from the top of a stack (front of queue)
    virtual int count(); // return number of items linked in this list
};
#endif /* VCwNodeSet_H_ */
