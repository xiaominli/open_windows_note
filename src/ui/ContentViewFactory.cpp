#include "ui/ContentViewFactory.h"
#include "ui/TextContentView.h"
namespace own {
std::unique_ptr<INoteContentView> makeContentView(NoteType type) {
    switch (type) {
        case NoteType::RichText:
        default:
            return std::make_unique<CTextContentView>();
    }
}
}
