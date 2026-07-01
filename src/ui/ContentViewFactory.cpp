#include "ui/ContentViewFactory.h"
#include "ui/TextContentView.h"
#include "ui/ChecklistContentView.h"
#include "ui/DrawingContentView.h"
namespace own {
std::unique_ptr<INoteContentView> makeContentView(NoteType type) {
    switch (type) {
        case NoteType::Checklist:
            return std::make_unique<CChecklistContentView>();
        case NoteType::Drawing:
            return std::make_unique<CDrawingContentView>();
        case NoteType::RichText:
        default:
            return std::make_unique<CTextContentView>();
    }
}
}
