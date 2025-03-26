/*
*Copyright (c) 2015 Qualcomm Technologies, Inc.
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
#ifndef CW_CAN_SIMULATION_H_
#define CW_CAN_SIMULATION_H_

#include "VCwLock.h"

class VCwEarSet;

class VCwCanSimulation: public VCwLock {
public:
    VCwCanSimulation();
    virtual ~VCwCanSimulation();
    void startCanSimulation();
protected:
    long mThreadId;
    VCwEarSet * mEarSet;
    int canSimulationFileReaderLoop();
    friend void* canSimulationThreadEntry(void * parg);
    bool mbThreadCreated;
};

#endif // CW_CAN_SIMULATION_H_
