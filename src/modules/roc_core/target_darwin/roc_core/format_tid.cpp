/*
 * Copyright (c) 2019 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "roc_core/format_tid.h"

namespace roc {
namespace core {

bool format_tid(char* buf, size_t bufsz) {
    uint64_t tid = 0;
    int ret_tid = pthread_threadid_np(NULL, &tid);
    int ret_buff = snprintf(buf, bufsz, "%llu", (unsigned long long)tid);
    return ret_tid == 0 && ret_buff > 0 && (size_t)ret_buff < bufsz;
}

} // namespace core
} // namespace roc
