/*
 * Copyright 2018 The Android Open Source Project
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

#include <composer-command-buffer/2.1/ComposerCommandBuffer.h>
#include <composer-resources/2.1/ComposerResources.h>

#include "ComposerHal.h"

namespace android {
namespace hardware {
namespace graphics {
namespace composer {
namespace V2_1 {
namespace implementation {

using composer::V2_1::hal::ComposerResources;

class ComposerCommandEngine : protected CommandReaderBase {
   public:
    ComposerCommandEngine(ComposerHal* hal, ComposerResources* resources)
        : mHal(hal), mResources(resources) {}

    bool setInputMQDescriptor(const MQDescriptorSync<uint32_t>& descriptor);

    const MQDescriptorSync<uint32_t>* getOutputMQDescriptor();

    Error execute(uint32_t inLength, const hidl_vec<hidl_handle>& inHandles, bool* outQueueChanged,
                  uint32_t* outCommandLength, hidl_vec<hidl_handle>* outCommandHandles);
    void reset();

  private:
    bool executeCommand(IComposerClient::Command command, uint16_t length);

    bool executeSelectDisplay(uint16_t length);
    bool executeSelectLayer(uint16_t length);
    bool executeSetColorTransform(uint16_t length);
    bool executeSetClientTarget(uint16_t length);
    bool executeSetOutputBuffer(uint16_t length);
    bool executeValidateDisplay(uint16_t length);
    bool executePresentOrValidateDisplay(uint16_t length);
    bool executeAcceptDisplayChanges(uint16_t length);
    bool executePresentDisplay(uint16_t length);

    bool executeSetLayerCursorPosition(uint16_t length);
    bool executeSetLayerBuffer(uint16_t length);
    bool executeSetLayerSurfaceDamage(uint16_t length);

    bool executeSetLayerBlendMode(uint16_t length);
    bool executeSetLayerColor(uint16_t length);
    bool executeSetLayerCompositionType(uint16_t length);
    bool executeSetLayerDataspace(uint16_t length);
    bool executeSetLayerDisplayFrame(uint16_t length);
    bool executeSetLayerPlaneAlpha(uint16_t length);
    bool executeSetLayerSidebandStream(uint16_t length);
    bool executeSetLayerSourceCrop(uint16_t length);
    bool executeSetLayerTransform(uint16_t length);
    bool executeSetLayerVisibleRegion(uint16_t length);
    bool executeSetLayerZOrder(uint16_t length);


    ComposerHal* mHal;
    ComposerResources* mResources;

    static constexpr size_t kWriterInitialSize = 64 * 1024 / sizeof(uint32_t) - 16;
    CommandWriterBase mWriter{kWriterInitialSize};

    Display mCurrentDisplay = 0;
    Layer mCurrentLayer = 0;


    hwc_rect_t readRect() {
        return hwc_rect_t{
            readSigned(), readSigned(), readSigned(), readSigned(),
        };
    }

    std::vector<hwc_rect_t> readRegion(size_t count) {
        std::vector<hwc_rect_t> region;
        region.reserve(count);
        while (count > 0) {
            region.emplace_back(readRect());
            count--;
        }

        return region;
    }

    hwc_frect_t readFRect() {
        return hwc_frect_t{
            readFloat(), readFloat(), readFloat(), readFloat(),
        };
    }

};

}  // namespace implementation
}  // namespace V2_1
}  // namespace composer
}  // namespace graphics
}  // namespace hardware
}  // namespace android
