#pragma once
#include <string>

namespace own {

struct DbPathChoice { std::string path; bool portable; };

// Pure decision: portable next to the exe when writable, else %APPDATA% fallback.
DbPathChoice chooseDbPath(const std::string& exeDir,
                          const std::string& appDataDir,
                          bool exeDirWritable);

// Win32 wrapper (defined in AppPathsWin.cpp, compiled only into the app project).
std::string resolveDbPathWin();

} // namespace own
