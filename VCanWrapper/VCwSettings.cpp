/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * VCwSettings.cpp
 * Class and functions for storing VCanWrapper settings
 * -------------------------------------------------
 *
 */

#include <VCanWrapper.h>
#include "VCwSettings.h"


int VCwSettings::mLibMode = DRIVER_MODE_RAW_FRAMES;
VCwSettings*  VCwSettings::pLibSettings = nullptr;
#ifdef ENABLE_CANTRANSLATOR_LIB
void* VCwSettings::mCanTranslatorLibHandle = NULL;
encodeFrames_t  VCwSettings::mEncodeFramesFn = NULL;
decodeFrames_t  VCwSettings::mDecodeFramesFn = NULL;
encodeFrameId_t VCwSettings::mEncodeFrameIdsFn = NULL;
#endif
///////////////////////////////////////////////////////////////
VCwSettings::VCwSettings() {

}
///////////////////////////////////////////////////////////////
VCwSettings::~VCwSettings() {
}


#ifdef ENABLE_CANTRANSLATOR_LIB
///////////////////////////////////////////////////////////////
// loadCanTranslatorLib - Loads CAN translation library
///@param - none
///@returns 0 on success else -1
//
bool VCwSettings::loadCanTranslatorLib() {
    mCanTranslatorLibHandle = dlopen(LIB_CAN_TRANSLATOR, RTLD_NOW);
    if (mCanTranslatorLibHandle == NULL) {
        CAN_LOGE("%s loading failed: %s", LIB_CAN_TRANSLATOR, dlerror());
        return false;
    }
    mEncodeFramesFn = (encodeFrames_t) dlsym(mCanTranslatorLibHandle, "encodeFrames");
    if (mEncodeFramesFn == NULL) {
        CAN_LOGE("decodeFrames lookup failed %s ", dlerror());
        return false;
    }
    mDecodeFramesFn = (decodeFrames_t) dlsym(mCanTranslatorLibHandle, "decodeFrames");
    if (mDecodeFramesFn == NULL) {
        CAN_LOGE("encodeFrames lookup failed %s ", dlerror());
        return false;
    }
    mEncodeFrameIdsFn = (encodeFrameId_t) dlsym(mCanTranslatorLibHandle, "encodeFrameId");
    if (mEncodeFrameIdsFn == NULL) {
        CAN_LOGE("encodeFrameId lookup failed %s ", dlerror());
        return false;
    }
    return true;
}
#endif
//////////////////////////////////////////////////////////////////
// getInstance
// @brief This function returns pointer to VCwSettings instance
// @return pointer at the VCwSettings singleton
//
VCwSettings * VCwSettings::getInstance() {
    if (pLibSettings == nullptr) {
        pLibSettings = new VCwSettings();
    }
    return pLibSettings;
}
