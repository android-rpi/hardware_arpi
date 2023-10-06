/*
 * Copyright 2020 Android-RPi Project
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

#define LOG_TAG "allocator-Allocator"
//#define LOG_NDEBUG 0
#include <android-base/logging.h>
#include <utils/Log.h>
#include <cutils/properties.h>

#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <android/binder_ibinder.h>
#include <android/binder_status.h>

#include <gralloctypes/Gralloc4.h>
#include <hardware/gralloc1.h>
#include <gbm_gralloc.h>

#include "Allocator.h"

static gralloc1_producer_usage_t toProducerUsage(uint64_t usage) {
    uint64_t producerUsage = usage & ~static_cast<uint64_t>(
            BufferUsage::CPU_READ_MASK | BufferUsage::CPU_WRITE_MASK |
            BufferUsage::GPU_DATA_BUFFER);
    switch (usage & BufferUsage::CPU_WRITE_MASK) {
    case static_cast<uint64_t>(BufferUsage::CPU_WRITE_RARELY):
        producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_WRITE;
        break;
    case static_cast<uint64_t>(BufferUsage::CPU_WRITE_OFTEN):
        producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN;
        break;
    default:
        break;
    }
    switch (usage & BufferUsage::CPU_READ_MASK) {
    case static_cast<uint64_t>(BufferUsage::CPU_READ_RARELY):
        producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_READ;
        break;
    case static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN):
        producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN;
        break;
    default:
        break;
    }
    return (gralloc1_producer_usage_t)producerUsage;
}

static gralloc1_consumer_usage_t toConsumerUsage(uint64_t usage) {
    uint64_t consumerUsage = usage & ~static_cast<uint64_t>(
            BufferUsage::CPU_READ_MASK | BufferUsage::CPU_WRITE_MASK |
            BufferUsage::SENSOR_DIRECT_DATA | BufferUsage::GPU_DATA_BUFFER);
    switch (usage & BufferUsage::CPU_READ_MASK) {
    case static_cast<uint64_t>(BufferUsage::CPU_READ_RARELY):
        consumerUsage |= GRALLOC1_CONSUMER_USAGE_CPU_READ;
        break;
    case static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN):
        consumerUsage |= GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN;
        break;
    default:
        break;
    }
    if (usage & BufferUsage::GPU_DATA_BUFFER) {
        consumerUsage |= GRALLOC1_CONSUMER_USAGE_GPU_DATA_BUFFER;
    }
    return (gralloc1_consumer_usage_t)consumerUsage;
}


namespace arpi::allocator {

unsigned long callingPid() {
    return static_cast<unsigned long>(AIBinder_getCallingPid());
}

Allocator::Allocator() {
    ALOGV("Allocator()");
    gbmDevice = gbm_init();
    if (gbmDevice == nullptr) {
        ALOGE("Failed Allocator()");
    }
}

Allocator::~Allocator() {
    ALOGV("~Allocator()");
    if (gbmDevice != nullptr) {
        gbm_destroy(gbmDevice);
    }
}

ndk::ScopedAStatus Allocator::allocateOneBuffer(const BufferDescriptorInfo& descriptor,
                                            buffer_handle_t* outBufferHandle,
                                            uint32_t* outStride) {

    uint64_t usage = toProducerUsage(descriptor.usage) | toConsumerUsage(descriptor.usage);
    buffer_handle_t handle = nullptr;
    int stride = 0;

    ALOGV("Calling alloc(%u, %u, %i, %lx)", descriptor.width,
            descriptor.height, descriptor.format, usage);
    auto error = gbm_alloc(gbmDevice, static_cast<int>(descriptor.width),
            static_cast<int>(descriptor.height), static_cast<int>(descriptor.format),
            usage, &handle, &stride);
    if (error != 0) {
        ALOGE("allocateOneBuffer() failed: %d (%s)", error, strerror(-error));
        return ndk::ScopedAStatus::fromStatus(error);
    }
    *outBufferHandle = handle;
    *outStride = stride;
    return ndk::ScopedAStatus::ok();
}

void Allocator::freeBuffers(const std::vector<const native_handle_t*>& buffers) {
    for (auto buffer : buffers) {
  	gbm_free(buffer);
        native_handle_close(buffer);
        delete buffer;
    }
}

ndk::ScopedAStatus Allocator::allocate(const std::vector<uint8_t>& descriptor, int32_t count,
                                              AidlAllocator::AllocationResult* outResult) {
    ALOGV("Allocation request from process: %lu", callingPid());

    BufferDescriptorInfo descriptorInfo;
    int ret = ::android::gralloc4::decodeBufferDescriptorInfo(descriptor, &descriptorInfo);
    if (ret) {
        ALOGE("Failed to allocate. Failed to decode buffer descriptor: %d.\n", ret);
        return ndk::ScopedAStatus::fromStatus(ret);
    }

    uint32_t stride = 0;
    std::vector<const native_handle_t*> buffers;
    buffers.reserve(count);

    for (int32_t i = 0; i < count; i++) {
        const native_handle_t* tmpBuffer;
        uint32_t tmpStride;

        ndk::ScopedAStatus status = allocateOneBuffer(descriptorInfo, &tmpBuffer, &tmpStride);

        if (!status.isOk()) {
            freeBuffers(buffers);
            return status;
        }

        buffers.push_back(tmpBuffer);

        if (stride == 0) {
            stride = tmpStride;
        } else if (stride != tmpStride) {
            freeBuffers(buffers);
            return ndk::ScopedAStatus::fromStatus(STATUS_UNKNOWN_ERROR);
        }
    }

    outResult->buffers.resize(count);
    outResult->stride = stride;

    for (int32_t i = 0; i < count; i++) {
        auto handle = buffers[i];
        outResult->buffers[i] = ::android::dupToAidl(handle);
    }
    freeBuffers(buffers);

    return ndk::ScopedAStatus::ok();
}

} // namespace arpi::allocator
