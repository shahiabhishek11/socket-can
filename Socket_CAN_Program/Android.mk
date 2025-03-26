LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Include directories
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/vcan_0  \
					$(LOCAL_PATH)/vcan_1

# Source files
LOCAL_SRC_FILES := main/Main.cpp

# Module name
LOCAL_MODULE := UnoMinda_MASTER_SERVICE
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)
