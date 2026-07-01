#pragma once
#include <cstdint>
#include "domain/Models.h"
namespace own {
bool isDue(const Reminder& r, int64_t now);
int64_t computeNextDue(const Reminder& r, int64_t firedAt);
int64_t snooze(int64_t now, int minutes);
}
