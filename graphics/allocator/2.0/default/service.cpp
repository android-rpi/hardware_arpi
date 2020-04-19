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

#define LOG_TAG "allocator@2.0-service"
#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "Allocator.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::status_t;
using android::sp;
using android::UNKNOWN_ERROR;

using android::hardware::graphics::allocator::V2_0::IAllocator;
using android::hardware::graphics::allocator::V2_0::implementation::Allocator;

int main() {
    configureRpcThreadpool(4, true);

    sp<IAllocator> service = new Allocator();

    status_t status = service->registerAsService();

    if (android::OK != status) {
        LOG(FATAL) << "Unable to register allocator service: " << status;
    }

    joinRpcThreadpool();
    return UNKNOWN_ERROR;
}
