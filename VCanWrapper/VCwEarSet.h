/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwEarSet.h
 * ----------------------------------------------------
 *  Created on: Jul 17, 2014
 * ----------------------------------------------------
 * Yeah - maybe should be ChListeners.
 * This class anchors a set callback definitions -
 * that is, the composite of all the VCanWrapper::registerListener calls
 */

#ifndef CHEARS_H_
#define CHEARS_H_
#include <inttypes.h>
#include "VCwNodeSet.h"
#include "VCwEar.h"
#include "VCanWrapper.h"
#include "VCwCanSimulation.h"
#include <pthread.h>


///////////////////////////////////////////////////////////////
// VCwEarSet - a singleton
//
class VCwEarSet : public VCwNodeSet {
protected:
    VCwEarSet();
    virtual ~VCwEarSet();
    static VCwEarSet * pTheEarSet;
public:
    using VCwNodeSet::add;
    using VCwNodeSet::rem;
    static VCwEarSet * getInstance();
    int add(VCwEar * ear);
    int add(int fid, int mask, CanCallback callback,
            void * userData, int ifNo);  
    int addCan(int fid, int mask, CanCallback_CAN callback,
            void * userData, int ifNo);       //for can node support
    // int add(int fid, int mask, CanCallback callback,
            // void * userData, int ifNo);
    int add(int fid, int mask, VCwBlockingQueue * rxQueue,
            void * userData, int ifNo);
    int rem(int xEarId);  // returns 1 if error

    // this is the main loop of the whole receive thing!
    // walk will call "each" matching listener
    //
    void walk(VCwFrame * pFrame, int ifNo);
    void walkCAN(VCwFrame * pFrame, int ifNo);      //WALK FOR CAN NODES


    int count() {return mCnt;};

    // need the following?
    int count(int ifNo); // count number matching this iface

protected:
    int mCnt;
    static int mEarCount;

    // need a RWLock for earset - to allow multiple readers but only one writer
    // need multiple threads calling walk, traversing the list of "ears" (listeners)
    // -- but then exclusive access when changing the list, e.g. registerListener()
    pthread_rwlock_t mRWLock;   // our multi-reader, 1-writer lock
    void readLock() {
        pthread_rwlock_rdlock(&mRWLock);
    }
    void writeLock() {
        pthread_rwlock_wrlock(&mRWLock);
    }
    void rwUnlock() {
        pthread_rwlock_unlock(&mRWLock);
    }
private:
    VCwCanSimulation* mpCanSimulationHandle;
};

#endif /* CHEARS_H_ */
