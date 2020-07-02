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

#define LOG_TAG "composer@2.1-Composer"
//#define LOG_NDEBUG 0
#include <android-base/logging.h>
#include <utils/Log.h>

#include "Composer.h"

namespace android {
namespace hardware {
namespace graphics {
namespace composer {
namespace V2_1 {
namespace implementation {

Composer::Composer() {
    mHal = std::make_unique<ComposerHal>();
}

Return<void> Composer::getCapabilities(getCapabilities_cb hidl_cb) {
    std::vector<Capability> caps;
    caps.push_back(Capability::PRESENT_FENCE_IS_NOT_RELIABLE);

    hidl_vec<Capability> caps_reply;
    caps_reply.setToExternal(caps.data(), caps.size());
    hidl_cb(caps_reply);
    return Void();
}

Return<void> Composer::dumpDebugInfo(dumpDebugInfo_cb hidl_cb) {
	hidl_cb(mHal->dumpDebugInfo());
    return Void();
}

Return<void> Composer::createClient(createClient_cb hidl_cb) {
    std::unique_lock<std::mutex> lock(mClientMutex);
    if (!waitForClientDestroyedLocked(lock)) {
        hidl_cb(Error::NO_RESOURCES, nullptr);
        return Void();
    }
    sp<IComposerClient> client = createClient();
    if (!client) {
        hidl_cb(Error::NO_RESOURCES, nullptr);
        return Void();
    }
    mClient = client;
    hidl_cb(Error::NONE, client);
    return Void();
}

bool Composer::waitForClientDestroyedLocked(std::unique_lock<std::mutex>& lock) {
    if (mClient != nullptr) {
        using namespace std::chrono_literals;
        ALOGD("waiting for previous client to be destroyed");
        mClientDestroyedCondition.wait_for(
            lock, 1s, [this]() -> bool { return mClient.promote() == nullptr; });
        if (mClient.promote() != nullptr) {
            ALOGD("previous client was not destroyed");
        } else {
            mClient.clear();
        }
    }
    return mClient == nullptr;
}

void Composer::onClientDestroyed() {
    std::lock_guard<std::mutex> lock(mClientMutex);
    mClient.clear();
    mClientDestroyedCondition.notify_all();
}

IComposerClient* Composer::createClient() {
    auto client = new ComposerClient(mHal.get());

    auto clientDestroyed = [this]() { onClientDestroyed(); };
    client->setOnClientDestroyed(clientDestroyed);

    return client;
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace composer
}  // namespace graphics
}  // namespace hardware
}  // namespace android
