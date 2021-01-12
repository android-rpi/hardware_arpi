LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := vulkan.rpi4
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := vulkan_rpi.cpp

LOCAL_LDFLAGS += -Wl,--build-id=sha1

LOCAL_C_INCLUDES := \
    external/mesa3d/include \
    external/mesa3d/prebuilt-intermediates/v3dv \
    frameworks/native/vulkan/include

LOCAL_STATIC_LIBRARIES := \
    libexpat \
    libmesa_compiler \
    libmesa_glsl \
    libmesa_nir \
    libmesa_util \
    libmesa_vulkan_util \
    libmesa_broadcom_genxml \
    libmesa_broadcom_cle

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libmesa_vulkan_broadcom

LOCAL_SHARED_LIBRARIES += \
    libsync \
    libdrm \
    liblog \
    libz

LOCAL_HEADER_LIBRARIES += \
    libhardware_headers 

include $(BUILD_SHARED_LIBRARY)
