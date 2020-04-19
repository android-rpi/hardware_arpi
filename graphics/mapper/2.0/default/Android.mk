LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.graphics.mapper@2.0-impl.rpi4
LOCAL_VENDOR_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
        drm_gralloc_mapper.cpp \
        Fence.cpp \
        Mapper.cpp

LOCAL_SHARED_LIBRARIES := \
        android.hardware.graphics.mapper@2.0 \
        android.hardware.graphics.common@1.0 \
        libhidlbase \
        libhidltransport \
        libutils \
        libcutils \
        liblog \
        libhardware \
        libsync \
        libdrm \
        libgralloc_drm

LOCAL_HEADER_LIBRARIES := android.hardware.graphics.mapper@2.0-passthrough_headers

LOCAL_C_INCLUDES := \
        external/drm_gralloc \
        external/libdrm \
        external/libdrm/include/drm \
        system/core/libgrallocusage/include

LOCAL_CFLAGS += \
        -Wall \
        -Werror

include $(BUILD_SHARED_LIBRARY)
