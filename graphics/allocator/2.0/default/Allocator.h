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

#ifndef ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_ALLOCATOR_H
#define ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_ALLOCATOR_H

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <mapper-passthrough/2.0/GrallocBufferDescriptor.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

using common::V1_0::BufferUsage;
using mapper::V2_0::BufferDescriptor;
using mapper::V2_0::Error;
using mapper::V2_0::IMapper;
using mapper::V2_0::passthrough::grallocDecodeBufferDescriptor;

class Allocator : public IAllocator {
  public:
    Allocator();
    ~Allocator();
    Return<void> dumpDebugInfo(dumpDebugInfo_cb _hidl_cb) override;
    Return<void> allocate(const BufferDescriptor& descriptor, uint32_t count,
                IAllocator::allocate_cb hidl_cb) override;
  private:
    Error allocateOneBuffer(const IMapper::BufferDescriptorInfo& descInfo,
                buffer_handle_t* outBufferHandle, uint32_t *outStride);
    void freeBuffers(const std::vector<const native_handle_t*>& buffers);

    struct drm_module_t* mModule;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_ALLOCATOR_H
