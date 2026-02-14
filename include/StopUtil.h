#pragma once

#include <mbed.h>
#include <stdint.h>

namespace stoputil {

// Terminate a thread and (best-effort) wait for it to become inactive.
void terminateThread(const char* name, rtos::Thread& thread, uint32_t waitMs = 250);

} // namespace stoputil
