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

#define LOG_TAG "composer@2.1-service"

#include <sched.h>
#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include <binder/ProcessState.h>

#include "Composer.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::status_t;
using android::sp;
using android::UNKNOWN_ERROR;

using android::hardware::graphics::composer::V2_1::IComposer;
using android::hardware::graphics::composer::V2_1::implementation::Composer;

int main() {
    // the conventional HAL might start binder services
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->setThreadPoolMaxThreadCount(4);
    android::ProcessState::self()->startThreadPool();

    // same as SF main thread
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
        LOG(FATAL) << "Couldn't set SCHED_FIFO: " << errno;
    }

    configureRpcThreadpool(4, true);

    sp<IComposer> service = new Composer();

    status_t status = service->registerAsService();

    if (android::OK != status) {
        LOG(FATAL) << "Unable to register composer service: " << status;
    }

    joinRpcThreadpool();
    return UNKNOWN_ERROR;
}
