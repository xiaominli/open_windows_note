#pragma once
#include <memory>
#include "domain/Models.h"
#include "ui/INoteContentView.h"
namespace own {
std::unique_ptr<INoteContentView> makeContentView(NoteType type);
}
