/*
 * Copyright (c) 2014-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwLock.h
 *
 *  Interfaces for lock/unlock  requirements around critical sections.
 *  These are semaphore based.
 *  These locks don't nest.
 */

#ifndef VCwLock_H_
#define VCwLock_H_

#include <semaphore.h>

///////////////////////////////////////////////////////////////
// VCwLock - a basic blocking semaphore
//        - makes call to sem_wait for lock()
//        - makes call to sem_post for unlock()
//
class VCwLock {
public:
    VCwLock();   // creates an open lock
    virtual ~VCwLock();
    void lock();    // enter critical section
    void unlock();  // leave critical section
private:
    sem_t mSemLok;
};
#endif /* VCwLock_H_ */
