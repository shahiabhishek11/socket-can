/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwSettings.h
 * This is used to store configurations/settings of entire library
 */

#ifndef VCwSettings_H_
#define VCwSettings_H_

#include <dlfcn.h>
#include "VCwBase.h"

#ifdef ENABLE_CANTRANSLATOR_LIB
#include <CanTranslator.h>
#endif

#define LIB_CAN_TRANSLATOR "libcantranslator.so"

class VCwSettings {
protected:
    VCwSettings();
    virtual ~VCwSettings();
public:
    static VCwSettings* pLibSettings;
    static int mLibMode;// Mode of library (eg:DRIVER_MODE_PROPERTIES)
#ifdef ENABLE_CANTRANSLATOR_LIB
    static void* mCanTranslatorLibHandle;
    static encodeFrames_t  mEncodeFramesFn;
    static decodeFrames_t  mDecodeFramesFn;
    static encodeFrameId_t mEncodeFrameIdsFn;
#endif
    static VCwSettings * getInstance();
#ifdef ENABLE_CANTRANSLATOR_LIB
    static bool loadCanTranslatorLib();
#endif

};
#endif /* VCwSettings_H_ */
