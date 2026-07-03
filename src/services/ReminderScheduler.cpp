#include "services/ReminderScheduler.h"
#include "data/NoteStore.h"
#include "domain/ReminderRules.h"
#include <algorithm>

void ReminderScheduler::poll(int64_t now) {
    if (!m_store || !onFire) return;
    auto due = own::pickDueReminders(m_store->enabledReminders(), now, m_active);
    for (const auto& r : due) {
        auto note = m_store->getNote(r.noteId);
        if (!note) { m_store->deleteReminder(r.id); continue; }   // 孤儿提醒清理
        m_active.push_back(r.id);
        onFire(r, *note);
    }
}

void ReminderScheduler::markResolved(int64_t reminderId) {
    m_active.erase(std::remove(m_active.begin(), m_active.end(), reminderId), m_active.end());
}
