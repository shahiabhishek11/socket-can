/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */
/*
 * VCwLock.cpp
 *
 *  Created on: Sep 24, 2014
 *      Author: wbarrett
 */

#include "VCwLock.h"


VCwLock::VCwLock() {
    // &sem_t, 1=out_of_process, count:0=locked, 1=unlocked
    sem_init(&mSemLok, 0, 1);  // 1=out of process  1=init unlocked
}

VCwLock::~VCwLock() {
    sem_destroy(&mSemLok);
}

void VCwLock::lock() {
    sem_wait(&mSemLok);
}

void VCwLock::unlock() {
    sem_post(&mSemLok);
}
