/*
 * Copyright (c) 2015 Mikhail Baranov
 * Copyright (c) 2015 Victor Gaydov
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_core/thread.h"
#include "roc_core/errno_to_str.h"
#include "roc_core/panic.h"

namespace roc {
namespace core {

Thread::Thread()
    : started_(0)
    , joinable_(0) {
}

Thread::~Thread() {
    if (joinable()) {
        roc_panic("thread: thread was not joined before calling destructor");
    }
}

bool Thread::joinable() const {
    return joinable_;
}

void Thread::start() {
    Mutex::Lock lock(mutex_);

    if (started_) {
        roc_panic("thread: can't start thread more than once");
    }

    if (int err = uv_thread_create(&thread_, thread_runner_, this)) {
        roc_panic("thread: uv_thread_create(): [%s] %s", uv_err_name(err),
                  uv_strerror(err));
    }

    started_ = 1;
    joinable_ = 1;
}

void Thread::join() {
    Mutex::Lock lock(mutex_);

    if (!started_) {
        roc_panic("thread: can't join thread that was not started");
    }

    if (!joinable_) {
        return;
    }

    if (int err = uv_thread_join(&thread_)) {
        roc_panic("thread: uv_thread_join(): [%s] %s", uv_err_name(err),
                  uv_strerror(err));
    }

    joinable_ = 0;
}

void Thread::thread_runner_(void* ptr) {
    static_cast<Thread*>(ptr)->run();
}

} // namespace core
} // namespace roc
