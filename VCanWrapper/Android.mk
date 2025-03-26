#/*
# * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
# * Qualcomm Technologies Proprietary and Confidential.
# */
#--------------------------------------------
# libVCanWrapper.so Makefile
#--------------------------------------------
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ENABLE_CANTRANSLATOR_SUPPORT:= false

LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/VCanWrapper

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

ifeq ($(ENABLE_CANTRANSLATOR_SUPPORT),true)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/CanTranslator
LOCAL_CFLAGS := -DENABLE_CANTRANSLATOR_LIB
endif

# ---------------------------------------------------------------------------------
#            Common definitons
# ---------------------------------------------------------------------------------
LOCAL_SRC_FILES:=      \
        VCanWrapper.cpp \
        VCwBase.cpp     \
        VCwBlockingQueue.cpp \
        VCwEar.cpp      \
        VCwEarSet.cpp   \
        VCwFrame.cpp    \
        VCwFilterManager.cpp \
        VCwLock.cpp     \
        VCwNodeSet.cpp  \
        VCwSim.cpp      \
        VCwSock.cpp     \
        VCwSockCan.cpp  \
        VCwCanSimulation.cpp \
        VCwWorker.cpp \
        VCwDropFrameLog.cpp \
        VCwSettings.cpp \

LOCAL_SHARED_LIBRARIES += \
    libcutils \
    liblog

LOCAL_MODULE:= libVCanWrapper

LOCAL_MODULE_TAGS := optional

LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += -Wall -Werror -Wunused -Wunreachable-code -Wmacro-redefined

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_USE_VNDK := true

LOCAL_COPY_HEADERS_TO := VCanWrapper

LOCAL_COPY_HEADERS += VCanWrapper.h VCwBase.h VCwFrame.h VCwSim.h VCwNode.h VCwBlockingQueue.h VCwNodeSet.h VCwLock.h

include $(BUILD_COPY_HEADERS)
