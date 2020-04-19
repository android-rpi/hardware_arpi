/*
 * Copyright 2016 The Android Open Source Project
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

#include <android/hardware/graphics/composer/2.1/IComposer.h>

#include "ComposerClient.h"

namespace android {
namespace hardware {
namespace graphics {
namespace composer {
namespace V2_1 {
namespace implementation {

using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

class Composer : public IComposer {
  public:
    Composer();

    Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override;
    Return<void> dumpDebugInfo(dumpDebugInfo_cb _hidl_cb) override;
    Return<void> createClient(createClient_cb _hidl_cb) override;

  private:

    bool waitForClientDestroyedLocked(std::unique_lock<std::mutex>& lock);
    void onClientDestroyed();
    IComposerClient* createClient();

    std::unique_ptr<ComposerHal> mHal;

    std::mutex mClientMutex;
    wp<IComposerClient> mClient;
    std::condition_variable mClientDestroyedCondition;

};

extern "C" IComposer* HIDL_FETCH_IComposer(const char* name);

}  // namespace implementation
}  // namespace V2_1
}  // namespace composer
}  // namespace graphics
}  // namespace hardware
}  // namespace android
