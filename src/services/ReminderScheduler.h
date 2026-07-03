#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "domain/Models.h"
namespace own { class NoteStore; }

// 到期提醒轮询：外部定时驱动 poll(now)。到期项回调 onFire；
// 通知未关（活动集内）不重复触发，关闭后 markResolved 移出。
class ReminderScheduler {
public:
    void attach(own::NoteStore* store) { m_store = store; }
    std::function<void(const own::Reminder&, const own::Note&)> onFire;
    void poll(int64_t now);
    void markResolved(int64_t reminderId);
private:
    own::NoteStore* m_store = nullptr;
    std::vector<int64_t> m_active;   // 已弹通知、尚未关闭的提醒 id
};
