/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwNode.h
 * Inherit from this to make an object a node in a list.
 * VCwNodeSet is used to make a list head of VCwNode objects.
 */

#ifndef CWNODE_H_
#define CWNODE_H_

class VCwNode {
public:
    VCwNode() {mNext = 0;};
    virtual ~VCwNode() {;};

    virtual void setNext(VCwNode * atThis) {
        mNext = atThis;
    }
    virtual VCwNode * getNext() {
        return mNext;
    }
protected:
    VCwNode * mNext;        // next object after this one
};

#endif /* CWNODE_H_ */
