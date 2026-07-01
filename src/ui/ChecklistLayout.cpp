#include "ui/ChecklistLayout.h"
namespace own {
RectI checklistRowRect(RectI c, ChecklistMetrics m, int index) {
    return { c.x, c.y + index * m.rowHeight, c.w, m.rowHeight };
}
RectI checklistBoxRect(RectI c, ChecklistMetrics m, int index) {
    RectI row = checklistRowRect(c, m, index);
    int y = row.y + (m.rowHeight - m.boxSize) / 2;
    return { c.x + m.pad, y, m.boxSize, m.boxSize };
}
ChecklistHitResult checklistHitTest(RectI c, ChecklistMetrics m, int itemCount, int px, int py) {
    if (px < c.x || px >= c.x + c.w || py < c.y) return { ChecklistHit::None, -1 };
    int row = (py - c.y) / m.rowHeight;
    if (row < 0) return { ChecklistHit::None, -1 };
    if (row == itemCount) return { ChecklistHit::AddRow, -1 };
    if (row > itemCount) return { ChecklistHit::None, -1 };
    RectI box = checklistBoxRect(c, m, row);
    if (px >= box.x && px < box.x + box.w && py >= box.y && py < box.y + box.h)
        return { ChecklistHit::Checkbox, row };
    return { ChecklistHit::Text, row };
}
}
