#include "app/AppPaths.h"

namespace own {

DbPathChoice chooseDbPath(const std::string& exeDir,
                          const std::string& appDataDir,
                          bool exeDirWritable) {
    if (exeDirWritable)
        return { exeDir + "\\notes.db", true };
    return { appDataDir + "\\open_windows_note\\notes.db", false };
}

} // namespace own
