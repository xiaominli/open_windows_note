#pragma once
#include "domain/Models.h"
namespace own {
struct ChecklistMetrics { int rowHeight; int boxSize; int pad; };
enum class ChecklistHit { None, Checkbox, Text, AddRow };
struct ChecklistHitResult { ChecklistHit kind; int index; };
RectI checklistRowRect(RectI content, ChecklistMetrics m, int index);
RectI checklistBoxRect(RectI content, ChecklistMetrics m, int index);
ChecklistHitResult checklistHitTest(RectI content, ChecklistMetrics m, int itemCount, int px, int py);
}
