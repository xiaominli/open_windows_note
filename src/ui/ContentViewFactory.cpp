#include "ui/ContentViewFactory.h"
#include "ui/TextContentView.h"
#include "ui/ChecklistContentView.h"
namespace own {
std::unique_ptr<INoteContentView> makeContentView(NoteType type) {
    switch (type) {
        case NoteType::Checklist:
            return std::make_unique<CChecklistContentView>();
        case NoteType::RichText:
        default:
            return std::make_unique<CTextContentView>();
    }
}
}
