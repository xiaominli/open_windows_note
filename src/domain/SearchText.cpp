#include "domain/SearchText.h"
namespace own {
static bool isAsciiSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}
std::string searchNormalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool pendingSpace = false;
    for (unsigned char c : s) {
        if (isAsciiSpace(c)) {
            if (!out.empty()) pendingSpace = true;   // 首部空白直接丢
            continue;
        }
        if (pendingSpace) { out.push_back(' '); pendingSpace = false; }
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');   // 仅 ASCII 转小写
        out.push_back((char)c);
    }
    return out;   // 尾部空白因 pendingSpace 未提交而天然裁掉
}
}
