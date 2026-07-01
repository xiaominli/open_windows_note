#pragma once
#include <cstdint>
class INoteWindowHost {
public:
    virtual ~INoteWindowHost() {}
    virtual void openOrFocusNote(int64_t id) = 0;
    virtual void refreshNoteWindow(int64_t id) = 0;
    virtual void closeNoteWindow(int64_t id) = 0;
    virtual void setAllNotesVisible(bool show) = 0;
};
