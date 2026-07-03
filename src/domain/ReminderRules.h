#pragma once
#include <cstdint>
#include <vector>
#include "domain/Models.h"
namespace own {
bool isDue(const Reminder& r, int64_t now);
int64_t computeNextDue(const Reminder& r, int64_t firedAt);
int64_t snooze(int64_t now, int minutes);
// P6: 到期挑选与通知按钮的状态迁移（纯函数，落库由调用方做）
std::vector<Reminder> pickDueReminders(const std::vector<Reminder>& rs, int64_t now,
                                       const std::vector<int64_t>& skipIds);
Reminder resolveReminderDismiss(Reminder r, int64_t now);
Reminder resolveReminderSnooze(Reminder r, int64_t now, int minutes);
}
