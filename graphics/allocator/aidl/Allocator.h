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

#pragma once

#include <aidl/android/hardware/graphics/allocator/AllocationResult.h>
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>

#include <cstdint>
#include <vector>

using BufferDescriptorInfo = android::hardware::graphics::mapper::V4_0::IMapper::BufferDescriptorInfo;
using BufferUsage = android::hardware::graphics::common::V1_0::BufferUsage;

namespace arpi {
namespace allocator {

namespace AidlAllocator = aidl::android::hardware::graphics::allocator;

class Allocator : public AidlAllocator::BnAllocator {
public:
    Allocator();
    ~Allocator();

    virtual ndk::ScopedAStatus allocate(const std::vector<uint8_t>& descriptor, int32_t count,
                                        AidlAllocator::AllocationResult* result) override;

private:
    ndk::ScopedAStatus allocateOneBuffer(const BufferDescriptorInfo& descriptor,
                                            buffer_handle_t* outBufferHandle,
                                            uint32_t* outStride);
    void freeBuffers(const std::vector<const native_handle_t*>& buffers);

    struct gbm_device *gbmDevice;
};

} // namespace allocator
} // namespace arpi
