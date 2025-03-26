/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwNodeSet.cpp
 * - list handler
 */

#include "VCwNodeSet.h"
#include "VCanWrapper.h"
//---------------------------------
//#undef  LOG_TAG
//#define LOG_TAG "**CANLIB:VCwNodeSet**"
//---------------------------------

///////////////////////////////////////////////////////////////
// The List Routines
// - choosing to avoid template classes
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// These list routines use THIS as a list header or item predecessor
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// push - add @pObj to just after this
//
void VCwNodeSet::push(VCwNode * pObj) {
    if (!pObj) return;    // nothing to push
    lock();
    mNext = pObj->getNext();
    pObj->setNext(this);
    unlock();
}

///////////////////////////////////////////////////////////////
// add - add pc to END of list headed by us
//
void VCwNodeSet::add(VCwNode * pObj) {
    if (!pObj) {
        return;    // nothing to tail
    }
    CAN_LOGE("add");
    lock();
    pObj->setNext(0);    // terminate list at us
    // find end of list starting with us
    VCwNode * pp = this;  // starting with us :-)
    while(pp->getNext()) {
        pp = pp->getNext();
    }
    // no mNext (mNext), so add us in
    pp->setNext(pObj);
    unlock();
}

///////////////////////////////////////////////////////////////
// rem - unlink param object from list @mNext (from this)
//
VCwNode * VCwNodeSet::rem(VCwNode * pObj) {
    //
    VCwNode * pp = this;  // starting with us :-)
    CAN_LOGE("rem");
    lock();
    if (pObj) { // then find it
        while(pp) { // pp is pointer at previous (to pObj) VCwNode
            if (pp->getNext() == pObj) break;
            pp = pp->getNext();
        }
    } else { // just rem from front of list
        pObj = getNext();
    }
    // link around pObj if found & removed
    if (pObj && pp) {
        pp->setNext(pObj->getNext());
        pObj->setNext(0);
    }
    unlock();
    return pObj;
}

///////////////////////////////////////////////////////////////
// count
//
int VCwNodeSet::count() {
    int c=0;
    lock();
    for(VCwNode * p = getNext(); p; p = p->getNext())
        c++;
    unlock();
    return c;
}
