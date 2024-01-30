/*
 * Copyright 2023 Android-RPi Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "mapper@4.0-Mapper"
//#define LOG_NDEBUG 0
#include <android-base/logging.h>
#include <utils/Log.h>
#include <inttypes.h>
#include <cutils/properties.h>
#include <gralloctypes/Gralloc4.h>
#include <hardware/gralloc1.h>

#include <drm_handle.h>
#include <gbm_gralloc.h>
#include "Mapper.h"

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V4_0 {
namespace implementation {

using aidl::android::hardware::graphics::common::Dataspace;
using aidl::android::hardware::graphics::common::PlaneLayout;
using aidl::android::hardware::graphics::common::StandardMetadataType;

Mapper::Mapper() {
    ALOGV("Mapper()");
    gbmDevice = gbm_init();
    if (gbmDevice == nullptr) {
        ALOGE("Failed to Mapper()");
    }
}

Mapper::~Mapper() {
    ALOGV("~Mapper()");
    if (gbmDevice != nullptr) {
        gbm_destroy(gbmDevice);
    }
}


static uint64_t getValidBufferUsageMask() {
	return BufferUsage::CPU_READ_MASK | BufferUsage::CPU_WRITE_MASK | BufferUsage::GPU_TEXTURE |
		   BufferUsage::GPU_RENDER_TARGET | BufferUsage::COMPOSER_OVERLAY |
		   BufferUsage::COMPOSER_CLIENT_TARGET | BufferUsage::PROTECTED |
		   BufferUsage::COMPOSER_CURSOR | BufferUsage::VIDEO_ENCODER |
		   BufferUsage::CAMERA_OUTPUT | BufferUsage::CAMERA_INPUT | BufferUsage::RENDERSCRIPT |
		   BufferUsage::VIDEO_DECODER | BufferUsage::SENSOR_DIRECT_DATA |
		   BufferUsage::GPU_DATA_BUFFER | BufferUsage::VENDOR_MASK |
		   BufferUsage::VENDOR_MASK_HI;
}

Return<void> Mapper::createDescriptor(const IMapper::BufferDescriptorInfo& descriptorInfo,
        createDescriptor_cb hidl_cb) {
	Error error = Error::NONE;
    if (!descriptorInfo.width || !descriptorInfo.height || !descriptorInfo.layerCount) {
        error = Error::BAD_VALUE;
    } else if (descriptorInfo.layerCount != 1) {
        error = Error::UNSUPPORTED;
    } else if (descriptorInfo.format == static_cast<PixelFormat>(0)) {
        error = Error::BAD_VALUE;
    }

    BufferDescriptor descriptor;
    if (error == Error::NONE) {
        const uint64_t validUsageBits = getValidBufferUsageMask();
        if (descriptorInfo.usage & ~validUsageBits) {
            ALOGW("buffer descriptor with invalid usage bits 0x%" PRIx64,
                    descriptorInfo.usage & ~validUsageBits);
        }
        int ret = ::android::gralloc4::encodeBufferDescriptorInfo(descriptorInfo, &descriptor);
        if (ret) {
            ALOGE("Failed to createDescriptor. Failed to encode: %d.", ret);
            error = Error::BAD_VALUE;
        }
    }
    hidl_cb(error, descriptor);
    return Void();
}

Return<void> Mapper::importBuffer(const hidl_handle& rawHandle,
		IMapper::importBuffer_cb hidl_cb) {
    if (!rawHandle.getNativeHandle()) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }
    Error error = Error::NONE;
    native_handle_t* bufferHandle = native_handle_clone(rawHandle.getNativeHandle());
    if (!bufferHandle) {
        error = Error::NO_RESOURCES;
    }

    ALOGV("register(%p)", bufferHandle);
    int result = gbm_register(gbmDevice, bufferHandle);
    if (result != 0) {
        ALOGE("register failed: %d", result);
        native_handle_close(bufferHandle);
        native_handle_delete(bufferHandle);
        bufferHandle = nullptr;
        error = Error::NO_RESOURCES;
    }

    hidl_cb(error, bufferHandle);
    return Void();
}

Return<Error> Mapper::freeBuffer(void* buffer) {
    Error error = Error::NONE;
    native_handle_t* bufferHandle = static_cast<native_handle_t*>(buffer);
    if (!bufferHandle) {
        error = Error::BAD_BUFFER;
    }
    if (error == Error::NONE) {
        ALOGV("unregister(%p)", bufferHandle);
        int result = gbm_unregister(bufferHandle);
        if (result != 0) {
            ALOGE("freeBuffer() failed: %d", result);
            error = Error::UNSUPPORTED;
        } else {
            native_handle_close(bufferHandle);
            native_handle_delete(bufferHandle);
        }
    }
    return error;
}


static Error getFenceFd(const hidl_handle& fenceHandle, base::unique_fd* outFenceFd) {
    auto handle = fenceHandle.getNativeHandle();
    if (handle && handle->numFds > 1) {
        ALOGE("invalid fence handle with %d fds", handle->numFds);
        return Error::BAD_VALUE;
    }
    int fenceFd = (handle && handle->numFds == 1) ? handle->data[0] : -1;
    if (fenceFd >= 0) {
        fenceFd = dup(fenceFd);
        if (fenceFd < 0) {
            return Error::NO_RESOURCES;
        }
    }
    outFenceFd->reset(fenceFd);
    return Error::NONE;
}

Return<void> Mapper::lock(void* buffer, uint64_t cpuUsage, const IMapper::Rect& accessRegion,
                  const hidl_handle& acquireFence, IMapper::lock_cb hidl_cb) {
    const native_handle_t* bufferHandle = static_cast<const native_handle_t*>(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    const auto pUsage = static_cast<gralloc1_producer_usage_t>(cpuUsage);
    const auto cUsage = static_cast<gralloc1_consumer_usage_t>(cpuUsage
            & ~static_cast<uint64_t>(BufferUsage::CPU_WRITE_MASK));
    const auto usage = static_cast<int32_t>(pUsage | cUsage);

    const auto accessRect = gralloc1_rect_t{accessRegion.left, accessRegion.top,
                 accessRegion.width, accessRegion.height};

    base::unique_fd fenceFd;
    Error error = getFenceFd(acquireFence, &fenceFd);
    if (error != Error::NONE) {
        hidl_cb(error, nullptr);
        return Void();
    }
    sp<Fence> aFence{new Fence(fenceFd.release())};
    aFence->waitForever("Mapper::lock");

    void* data = nullptr;
    int result = gbm_lock(bufferHandle, usage, accessRect.left, accessRect.top,
            accessRect.width, accessRect.height, &data);

    if (result != 0) {
    	ALOGE("gbm_lock() returned %d", result);
        hidl_cb(Error::UNSUPPORTED, nullptr);
    } else {
        hidl_cb(error, data);
    }
    return Void();
}

static hidl_handle getFenceHandle(const base::unique_fd& fenceFd, char* handleStorage) {
    native_handle_t* handle = nullptr;
    if (fenceFd >= 0) {
        handle = native_handle_init(handleStorage, 1, 0);
        handle->data[0] = fenceFd;
    }
    return hidl_handle(handle);
}

Return<void> Mapper::unlock(void* buffer, IMapper::unlock_cb hidl_cb) {
    const native_handle_t* bufferHandle = static_cast<const native_handle_t*>(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    int result = gbm_unlock(bufferHandle);
	if (result != 0) {
		ALOGE("Mapper unlock failed: %d", result);
        hidl_cb(Error::UNSUPPORTED, nullptr);
        return Void();
	}

    base::unique_fd fenceFd;
    fenceFd.reset((Fence::NO_FENCE)->dup());
    NATIVE_HANDLE_DECLARE_STORAGE(fenceStorage, 1, 0);
    hidl_cb(Error::NONE, getFenceHandle(fenceFd, fenceStorage));
    return Void();
}

Return<Error> Mapper::validateBufferSize(void* /*buffer*/, const BufferDescriptorInfo& /*descriptor*/,
        uint32_t /*stride*/) {
    ALOGV("validateBufferSize()");
    return Error::NONE;
}

Return<void> Mapper::getTransportSize(void* buffer, getTransportSize_cb hidl_cb) {
    ALOGV("getTransportSize()");
    const native_handle_t* bufferHandle = static_cast<const native_handle_t*>(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, 0, 0);
        return Void();
    }
    hidl_cb(Error::NONE, bufferHandle->numFds, bufferHandle->numInts);
    return Void();
}

Return<void> Mapper::flushLockedBuffer(void* buffer,flushLockedBuffer_cb hidl_cb) {
    ALOGV("flushLockedBuffer()");
    const native_handle_t* bufferHandle = static_cast<const native_handle_t*>(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    int result = gbm_unlock(bufferHandle);
	if (result != 0) {
		ALOGE("Mapper unlock failed: %d", result);
        hidl_cb(Error::UNSUPPORTED, nullptr);
        return Void();
	}

    base::unique_fd fenceFd;
    fenceFd.reset((Fence::NO_FENCE)->dup());
    NATIVE_HANDLE_DECLARE_STORAGE(fenceStorage, 1, 0);
    hidl_cb(Error::NONE, getFenceHandle(fenceFd, fenceStorage));
    return Void();
}

Return<Error> Mapper::rereadLockedBuffer(void* buffer) {
    ALOGV("rereadLockedBuffer()");
    const native_handle_t* bufferHandle = static_cast<const native_handle_t*>(buffer);
    if (!bufferHandle) {
        ALOGE("Failed to rereadLockedBuffer. Empty handle.");
        return Error::BAD_BUFFER;
    }
    return Error::NONE;
}

Return<void> Mapper::isSupported(const BufferDescriptorInfo& /*descriptor*/,
        isSupported_cb hidl_cb) {
    ALOGV("isSupported()");
    hidl_cb(Error::NONE, true);
    return Void();
}

#define GRALLOC_ALIGN(value, base) (((value) + ((base)-1)) & ~((base)-1))

static android::status_t get_plane_layouts(buffer_handle_t handle,
                                std::vector<PlaneLayout> *layouts) {
    struct private_handle_t *hnd = (struct private_handle_t *)handle;
    PlaneLayout layout;
    /* Y */
    switch (hnd->format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        layouts->reserve(3);
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_Y,
 	                     .offsetInBits = 0}},
                .offsetInBytes = 0,
                .sampleIncrementInBits = 8,
		.strideInBytes = GRALLOC_ALIGN(hnd->width, 16),
		.widthInSamples = hnd->width,
		.heightInSamples = hnd->height,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width, 16) * hnd->height,
                .horizontalSubsampling = 1,
                .verticalSubsampling = 1};
	break;
    case HAL_PIXEL_FORMAT_YV12:
        layouts->reserve(3);
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_Y,
                             .offsetInBits = 0}},
                .offsetInBytes = 0,
                .sampleIncrementInBits = 8,
		.strideInBytes = hnd->width,
		.widthInSamples = hnd->width,
		.heightInSamples = hnd->height,
		.totalSizeInBytes = hnd->width * hnd->height,
                .horizontalSubsampling = 1,
                .verticalSubsampling = 1};
	break;
    default:
        return android::BAD_VALUE;
    }
    layouts->push_back(layout);

    /* CR */
    switch (hnd->format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_CR,
                             .offsetInBits = 0}},
                .offsetInBytes = GRALLOC_ALIGN(hnd->width, 16) * hnd->height + 1,
		.sampleIncrementInBits = 16,
		.strideInBytes = GRALLOC_ALIGN(hnd->width, 16),
		.widthInSamples = hnd->width / 2,
		.heightInSamples = hnd->height / 2,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
                .horizontalSubsampling = 2,
                .verticalSubsampling = 2};
	break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_CR,
                             .offsetInBits = 0}},
                .offsetInBytes = GRALLOC_ALIGN(hnd->width, 16) * hnd->height,
		.sampleIncrementInBits = 16,
		.strideInBytes = GRALLOC_ALIGN(hnd->width, 16),
		.widthInSamples = hnd->width / 2,
		.heightInSamples = hnd->height / 2,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
                .horizontalSubsampling = 2,
                .verticalSubsampling = 2};
	break;
    case HAL_PIXEL_FORMAT_YV12:
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_CR,
                             .offsetInBits = 0}},
                .offsetInBytes = hnd->width * hnd->height,
		.sampleIncrementInBits = 8,
		.strideInBytes = GRALLOC_ALIGN(hnd->width / 2, 16),
		.widthInSamples = hnd->width / 2,
		.heightInSamples = hnd->height / 2,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
                .horizontalSubsampling = 2,
                .verticalSubsampling = 2};
	break;
    default:
        return android::BAD_VALUE;
    }
    layouts->push_back(layout);

    /* CB */
    switch (hnd->format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
        ALOGV("get_plane_layouts() HAL_PIXEL_FORMAT_YCbCr_420_888");
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_CB,
                             .offsetInBits = 0}},
                .offsetInBytes = GRALLOC_ALIGN(hnd->width, 16) * hnd->height,
		.sampleIncrementInBits = 16,
		.strideInBytes = GRALLOC_ALIGN(hnd->width, 16),
		.widthInSamples = hnd->width / 2,
		.heightInSamples = hnd->height / 2,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
                .horizontalSubsampling = 2,
                .verticalSubsampling = 2};
	break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        ALOGV("get_plane_layouts() HAL_PIXEL_FORMAT_YCrCb_420_SP");
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_CB,
                             .offsetInBits = 0}},
                .offsetInBytes = GRALLOC_ALIGN(hnd->width, 16) * hnd->height + 1,
		.sampleIncrementInBits = 16,
		.strideInBytes = GRALLOC_ALIGN(hnd->width, 16),
		.widthInSamples = hnd->width / 2,
		.heightInSamples = hnd->height / 2,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
                .horizontalSubsampling = 2,
                .verticalSubsampling = 2};
	break;
    case HAL_PIXEL_FORMAT_YV12:
        ALOGV("get_plane_layouts() HAL_PIXEL_FORMAT_YV12");
        layout = {.components = {{.type = android::gralloc4::PlaneLayoutComponentType_CB,
                             .offsetInBits = 0}},
                .offsetInBytes = hnd->width * hnd->height
		                + GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
		.sampleIncrementInBits = 8,
		.strideInBytes = GRALLOC_ALIGN(hnd->width / 2, 16),
		.widthInSamples = hnd->width / 2,
		.heightInSamples = hnd->height / 2,
		.totalSizeInBytes = GRALLOC_ALIGN(hnd->width / 2, 16) * hnd->height / 2,
                .horizontalSubsampling = 2,
                .verticalSubsampling = 2};
	break;
    default:
        return android::BAD_VALUE;
    }
    layouts->push_back(layout);

    return android::OK;
}

Return<void> Mapper::get(void* buffer, const MetadataType& metadataType, get_cb hidl_cb) {
    android::status_t err = android::OK;
    hidl_vec<uint8_t> encodedMetadata;
    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, encodedMetadata);
        return Void();
    }
    if (android::gralloc4::isStandardMetadataType(metadataType)) {
        switch (android::gralloc4::getStandardMetadataTypeValue(metadataType)) {
        case StandardMetadataType::DATASPACE: {
            err = android::gralloc4::encodeDataspace(Dataspace::UNKNOWN, &encodedMetadata);
	    break;
	}
        case StandardMetadataType::PLANE_LAYOUTS: {
            std::vector<PlaneLayout> layouts;
            err = get_plane_layouts(bufferHandle, &layouts);
            if (!err) {
                err = android::gralloc4::encodePlaneLayouts(layouts, &encodedMetadata);
            }
	    break;
        }
	default:
	    err = android::BAD_VALUE;
	}
    }
    hidl_cb((err) ? Error::UNSUPPORTED : Error::NONE, encodedMetadata);
    return Void();
}

Return<Error> Mapper::set(void* buffer, const MetadataType& /*metadataType*/,
        const hidl_vec<uint8_t>& /*metadata*/) {
    ALOGV("set()");
    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(buffer);
    if (!bufferHandle) {
        return Error::BAD_BUFFER;
    }
    return Error::UNSUPPORTED;
}

Return<void> Mapper::getFromBufferDescriptorInfo(const BufferDescriptorInfo& /*descriptor*/,
        const MetadataType& /*metadataType*/, getFromBufferDescriptorInfo_cb hidl_cb) {
    ALOGV("getFromBufferDescriptorInfo()");
    hidl_vec<uint8_t> encodedMetadata;
    hidl_cb(Error::UNSUPPORTED, encodedMetadata);
    return Void();
}

Return<void> Mapper::listSupportedMetadataTypes(listSupportedMetadataTypes_cb hidl_cb) {
    ALOGV("listSupportedMetadataTypes()");
    hidl_vec<MetadataTypeDescription> supported;
    hidl_cb(Error::NONE, supported);
    return Void();
}

Return<void> Mapper::dumpBuffer(void* /*buffer*/, dumpBuffer_cb hidl_cb) {
    ALOGV("dumpBuffer()");
    BufferDump bufferDump;
    hidl_cb(Error::NONE, bufferDump);
    return Void();
}

Return<void> Mapper::dumpBuffers(dumpBuffers_cb hidl_cb) {
    ALOGV("dumpBuffers()");
    std::vector<BufferDump> bufferDumps;
    hidl_cb(Error::NONE, bufferDumps);
    return Void();
}

Return<void> Mapper::getReservedRegion(void* buffer, getReservedRegion_cb hidl_cb) {
    ALOGV("getReservedRegion()");
    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, nullptr, 0);
        return Void();
    }
    hidl_cb(Error::UNSUPPORTED, nullptr, 0);
    return Void();
}

IMapper* HIDL_FETCH_IMapper(const char* /* name */) {
    return new Mapper();
}

}  // namespace implementation
}  // namespace V4_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android
