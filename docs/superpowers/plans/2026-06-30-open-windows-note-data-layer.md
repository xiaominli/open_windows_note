# open_windows_note — P1: 工程骨架 + 数据层 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 搭好 VS 工程骨架并实现可单元测试的数据层(`Database` + `NoteStore` + 迁移 + JSON 序列化 + 纯逻辑函数),全部用 doctest 覆盖,不依赖任何 HWND。

**Architecture:** 分层中的「数据层 + 领域层」先行。`Database` 封装 `sqlite3*`(RAII、事务、预编译语句、`PRAGMA user_version` 迁移、`integrity_check`);`NoteStore` 是唯一写 SQL 的仓储,返回领域结构体。清单/涂鸦内容序列化为 JSON;提醒重复计算与窗口越界钳制做成纯函数,便于确定性测试。

**Tech Stack:** C++17 / MFC(静态链接,应用工程用;数据层本身不碰 MFC)· SQLite amalgamation(`sqlite3.c`)· nlohmann/json(单头)· doctest(单头,测试)· Visual Studio 2022 / MSBuild。

## Global Constraints

- 平台:Windows 10+,**x64**。
- 语言标准:**C++17**(`/std:c++17`)。
- 数据库文件名固定 **`notes.db`**;便携路径 `<exe目录>\notes.db`,不可写回落 `%APPDATA%\open_windows_note\notes.db`。
- 第三方库全部 **vendored 到 `src/third_party/`**,不用包管理器:`sqlite/`、`json/`、`doctest/`。
- SQLite 编译宏:`SQLITE_THREADSAFE=1`、`SQLITE_DEFAULT_MEMSTATUS=0`;**不启用 FTS5**(v1 搜索用 `LIKE`)。
- 数据层(`data/`、`domain/`)**不得 include 任何 MFC / windows.h GUI 头**;仅可用标准库 + `sqlite3.h` + json。越界钳制函数用平台无关的 `struct RectI`,不用 `RECT`。
- 时间一律用 `int64_t` Unix 秒(UTC)。任何"当前时间"必须作参数注入,**禁止在被测逻辑里直接调用 `time(nullptr)`**。
- License:MIT。`docs/temp/` 与 `*.db` 不入库(已在 `.gitignore`)。
- 提交信息末尾附:`Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`。

---

## File Structure

```
open_windows_note/
├─ open_windows_note.sln
├─ src/
│  ├─ data/
│  │  ├─ Database.h / Database.cpp          # sqlite3* RAII, 事务, 迁移, integrity_check
│  │  ├─ NoteStore.h / NoteStore.cpp        # 仓储: 所有 SQL
│  │  └─ Migrations.h / Migrations.cpp      # schema 版本 → SQL 脚本
│  ├─ domain/
│  │  ├─ Models.h                           # Note/NoteType/ChecklistItem/Stroke/Tag/Group/Reminder/Theme/RectI
│  │  ├─ ChecklistJson.h / .cpp             # 清单 ⇄ JSON
│  │  ├─ StrokesJson.h / .cpp               # 笔迹 ⇄ JSON
│  │  ├─ ReminderRules.h / .cpp             # computeNextDue 纯函数
│  │  └─ Geometry.h / .cpp                  # clampRectToWorkArea 纯函数
│  └─ third_party/
│     ├─ sqlite/ (sqlite3.c, sqlite3.h)
│     ├─ json/ (json.hpp)
│     └─ doctest/ (doctest.h)
├─ tests/
│  ├─ tests.vcxproj                         # 控制台 doctest 工程
│  ├─ test_main.cpp                         # doctest 入口
│  ├─ test_database.cpp
│  ├─ test_notestore.cpp
│  ├─ test_checklist_json.cpp
│  ├─ test_strokes_json.cpp
│  ├─ test_reminder_rules.cpp
│  └─ test_geometry.cpp
└─ app/                                     # (P2 起) MFC 应用工程, 本计划仅占位
```

**边界说明**:`data/` 依赖 `domain/` 与 `third_party/sqlite`;`domain/` 只依赖标准库与 `third_party/json`。测试工程链接 `sqlite3.c` + `data/*` + `domain/*` + doctest,不链接 MFC/app。

---

### Task 1: 工程骨架与第三方库

**Files:**
- Create: `LICENSE`, `README.md`
- Create: `src/third_party/sqlite/sqlite3.c`, `src/third_party/sqlite/sqlite3.h`
- Create: `src/third_party/json/json.hpp`
- Create: `src/third_party/doctest/doctest.h`
- Create: `tests/tests.vcxproj`, `tests/test_main.cpp`
- Create: `open_windows_note.sln`

**Interfaces:**
- Consumes: 无。
- Produces: 一个能编译运行的空 doctest 控制台工程 `tests.exe`,后续任务往里加测试文件。

- [ ] **Step 1: 下载 vendored 依赖**

```bash
# 在仓库根执行 (Git Bash)。sqlite 用 amalgamation zip 里的 sqlite3.c/.h。
mkdir -p src/third_party/sqlite src/third_party/json src/third_party/doctest
curl -L -o /tmp/sqlite.zip https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
unzip -j /tmp/sqlite.zip 'sqlite-amalgamation-*/sqlite3.c' 'sqlite-amalgamation-*/sqlite3.h' -d src/third_party/sqlite
curl -L -o src/third_party/json/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
curl -L -o src/third_party/doctest/doctest.h https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
```

Expected: 四个文件存在且非空(`sqlite3.c` 约 9MB,`json.hpp` 约 900KB,`doctest.h` 约 250KB)。

- [ ] **Step 2: 写 doctest 入口**

`tests/test_main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 3: 写 LICENSE 与 README**

`LICENSE` = 标准 MIT 全文,版权行:`Copyright (c) 2026 xiaominli`。
`README.md`:
```markdown
# open_windows_note
Windows 桌面便签 (C++/MFC + SQLite)。开源 (MIT)。
构建: 打开 open_windows_note.sln, 选 x64。测试工程为 tests。
```

- [ ] **Step 4: 建 tests.vcxproj(控制台, x64, C++17)**

关键工程设置(在 VS 里新建"控制台 App"命名 `tests`,或手写 vcxproj):
- 配置:`Debug|x64`、`Release|x64`。
- C/C++ → 语言 → C++ 语言标准:`/std:c++17`。
- C/C++ → 常规 → 附加包含目录:`src;src/third_party/sqlite;src/third_party/json;src/third_party/doctest`。
- 预处理器定义:`SQLITE_THREADSAFE=1;SQLITE_DEFAULT_MEMSTATUS=0;_CRT_SECURE_NO_WARNINGS`。
- 加入源文件:`tests/test_main.cpp`、`src/third_party/sqlite/sqlite3.c`。
- `sqlite3.c` 属性 → 预编译头:**不使用**(设为 "Not Using Precompiled Headers")。

- [ ] **Step 5: 建解决方案并编译运行**

Run:
```bash
# 用 VS 的开发者命令行 (Developer Command Prompt) 或:
msbuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m
./x64/Debug/tests.exe
```
Expected: 编译通过;运行输出 doctest 的 `[doctest] Status: SUCCESS!`(0 个测试,0 失败)。

- [ ] **Step 6: Commit**

```bash
git add LICENSE README.md src/third_party tests open_windows_note.sln
git commit -m "chore: scaffold solution, vendor sqlite/json/doctest, empty test runner

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: 领域模型头文件

**Files:**
- Create: `src/domain/Models.h`

**Interfaces:**
- Consumes: 无(纯标准库)。
- Produces: 下列类型供全数据层使用:
  - `enum class NoteType { RichText=0, Checklist=1, Drawing=2 };`
  - `struct RectI { int x, y, w, h; };`
  - `struct Note`(字段见下)、`struct ChecklistItem`、`struct Stroke`、`struct Tag`、`struct Group`、`struct Reminder`、`enum class Recurrence`、`struct Theme`。

- [ ] **Step 1: 写模型头**

`src/domain/Models.h`:
```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace own {  // open_windows_note

enum class NoteType : int { RichText = 0, Checklist = 1, Drawing = 2 };
enum class Recurrence : int { None = 0, Daily = 1, Weekly = 2, Monthly = 3 };

struct RectI { int x = 0, y = 0, w = 0, h = 0; };

struct Note {
    int64_t id = 0;
    NoteType type = NoteType::RichText;
    std::string title;
    std::vector<uint8_t> contentBlob;   // RTF / JSON items / JSON strokes
    std::string plainText;              // 搜索缓存(已小写)
    int64_t themeId = 0;
    int64_t groupId = 0;                // 0 = 无分组
    RectI rect{ 100, 100, 240, 200 };
    std::string monitorId;
    int opacity = 255;                  // 0..255
    bool pinned = true;
    bool rolledUp = false;
    bool visible = true;
    std::string stickTarget;            // 空 = 不贴
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

struct ChecklistItem { std::string text; bool checked = false; int order = 0; };

struct Stroke {
    uint32_t color = 0x000000;          // 0xRRGGBB
    int width = 3;
    std::vector<std::pair<int,int>> points;
};

struct Tag { int64_t id = 0; std::string name; };
struct Group { int64_t id = 0; std::string name; int orderIdx = 0; };

struct Reminder {
    int64_t id = 0;
    int64_t noteId = 0;
    int64_t dueAt = 0;                  // Unix 秒
    Recurrence recurrence = Recurrence::None;
    int recurInterval = 1;
    int64_t snoozeUntil = 0;            // 0 = 无
    std::string soundPath;
    bool enabled = true;
};

struct Theme {
    int64_t id = 0;
    std::string name;
    uint32_t bgColor = 0xFFF7B0;        // 0xRRGGBB
    uint32_t titleColor = 0xF2D24A;
    uint32_t textColor = 0x202020;
    bool isBuiltin = false;
};

} // namespace own
```

- [ ] **Step 2: 编译校验(纳入测试工程)**

在 `tests/test_main.cpp` 之外无需引用;仅确认头能被包含。新建 `tests/test_models.cpp`:
```cpp
#include "doctest.h"
#include "domain/Models.h"
TEST_CASE("models default-construct") {
    own::Note n;
    CHECK(n.opacity == 255);
    CHECK(n.pinned == true);
    CHECK(static_cast<int>(n.type) == 0);
}
```
把 `tests/test_models.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 3: 运行**

Run: `msbuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m && ./x64/Debug/tests.exe`
Expected: PASS(1 个测试)。

- [ ] **Step 4: Commit**

```bash
git add src/domain/Models.h tests/test_models.cpp tests/tests.vcxproj
git commit -m "feat(domain): add core model structs

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Database — 打开/关闭/exec 与 RAII

**Files:**
- Create: `src/data/Database.h`, `src/data/Database.cpp`
- Test: `tests/test_database.cpp`

**Interfaces:**
- Consumes: `sqlite3.h`。
- Produces:
  - `class Database`,不可拷贝、可移动。
  - `bool Database::open(const std::string& path, std::string* err)` —— 打开或创建;成功返回 true。
  - `bool Database::exec(const std::string& sql, std::string* err)` —— 执行无结果 SQL。
  - `sqlite3* Database::handle() const`。
  - `int64_t Database::lastInsertRowId() const`。
  - 析构自动 `sqlite3_close`。
  - 静态 `Database::openInMemory()` 便于测试(path=`":memory:"`)。

- [ ] **Step 1: 写失败测试**

`tests/test_database.cpp`:
```cpp
#include "doctest.h"
#include "data/Database.h"

TEST_CASE("open in-memory and exec create table") {
    own::Database db;
    std::string err;
    REQUIRE(db.open(":memory:", &err));
    CHECK(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT);", &err));
    CHECK(db.exec("INSERT INTO t(v) VALUES('hello');", &err));
    CHECK(db.lastInsertRowId() == 1);
}

TEST_CASE("exec on bad sql reports error") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    CHECK_FALSE(db.exec("NOT VALID SQL;", &err));
    CHECK_FALSE(err.empty());
}
```
加入 `tests.vcxproj`。

- [ ] **Step 2: 运行验证失败**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: 编译失败(`Database.h` 不存在)。

- [ ] **Step 3: 实现**

`src/data/Database.h`:
```cpp
#pragma once
#include <string>
#include <cstdint>
struct sqlite3;
namespace own {
class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& o) noexcept;
    Database& operator=(Database&& o) noexcept;

    bool open(const std::string& path, std::string* err);
    bool exec(const std::string& sql, std::string* err);
    int64_t lastInsertRowId() const;
    sqlite3* handle() const { return db_; }
    void close();
private:
    sqlite3* db_ = nullptr;
};
} // namespace own
```

`src/data/Database.cpp`:
```cpp
#include "data/Database.h"
#include "sqlite3.h"

namespace own {

Database::~Database() { close(); }
Database::Database(Database&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
Database& Database::operator=(Database&& o) noexcept {
    if (this != &o) { close(); db_ = o.db_; o.db_ = nullptr; }
    return *this;
}
void Database::close() { if (db_) { sqlite3_close(db_); db_ = nullptr; } }

bool Database::open(const std::string& path, std::string* err) {
    close();
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) { if (err) *err = db_ ? sqlite3_errmsg(db_) : "open failed"; return false; }
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    return true;
}

bool Database::exec(const std::string& sql, std::string* err) {
    char* msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &msg);
    if (rc != SQLITE_OK) { if (err && msg) *err = msg; if (msg) sqlite3_free(msg); return false; }
    return true;
}

int64_t Database::lastInsertRowId() const { return sqlite3_last_insert_rowid(db_); }

} // namespace own
```
把 `src/data/Database.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/data/Database.h src/data/Database.cpp tests/test_database.cpp tests/tests.vcxproj
git commit -m "feat(data): Database RAII wrapper over sqlite3

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Database — 事务 RAII 与预编译语句辅助

**Files:**
- Modify: `src/data/Database.h`, `src/data/Database.cpp`
- Create: `src/data/Statement.h`, `src/data/Statement.cpp`
- Test: `tests/test_database.cpp`(追加)

**Interfaces:**
- Consumes: Task 3 `Database`。
- Produces:
  - `class Transaction { Transaction(Database&); void commit(); ~Transaction()回滚未提交; };`
  - `class Statement`:`Statement(Database&, sql)`;`bind(int idx, int64_t)`、`bind(int, const std::string&)`、`bindBlob(int, const uint8_t*, size_t)`、`bindNull(int)`;`step()→bool(有行)`;`columnInt64(int)`、`columnText(int)`、`columnBlob(int)→std::vector<uint8_t>`;`execDone()` 执行到结束。

- [ ] **Step 1: 写失败测试(追加到 test_database.cpp)**

```cpp
#include "data/Statement.h"

TEST_CASE("statement bind/step roundtrip") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, s TEXT, b BLOB);", &err));
    {
        own::Statement ins(db, "INSERT INTO t(s,b) VALUES(?,?);");
        ins.bind(1, std::string("abc"));
        std::vector<uint8_t> blob{1,2,3};
        ins.bindBlob(2, blob.data(), blob.size());
        ins.execDone();
    }
    own::Statement sel(db, "SELECT s,b FROM t WHERE id=1;");
    REQUIRE(sel.step());
    CHECK(sel.columnText(0) == "abc");
    CHECK(sel.columnBlob(1) == std::vector<uint8_t>{1,2,3});
}

TEST_CASE("transaction rollback on scope exit without commit") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY);", &err));
    { own::Transaction tx(db); db.exec("INSERT INTO t DEFAULT VALUES;", &err); } // 不 commit
    own::Statement cnt(db, "SELECT COUNT(*) FROM t;");
    REQUIRE(cnt.step());
    CHECK(cnt.columnInt64(0) == 0);
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`Statement.h`/`Transaction` 未定义)。

- [ ] **Step 3: 实现 Statement 与 Transaction**

`src/data/Statement.h`:
```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>
struct sqlite3_stmt;
namespace own {
class Database;
class Statement {
public:
    Statement(Database& db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void bind(int i, int64_t v);
    void bind(int i, const std::string& v);
    void bindBlob(int i, const uint8_t* p, size_t n);
    void bindNull(int i);
    bool step();                 // true=有行, false=完成
    void reset();
    void execDone();             // step 到结束
    int64_t columnInt64(int c);
    std::string columnText(int c);
    std::vector<uint8_t> columnBlob(int c);
    bool columnIsNull(int c);
private:
    sqlite3_stmt* st_ = nullptr;
};
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();
    void commit();
private:
    Database& db_;
    bool active_ = true;
};
} // namespace own
```

`src/data/Statement.cpp`:
```cpp
#include "data/Statement.h"
#include "data/Database.h"
#include "sqlite3.h"
#include <stdexcept>

namespace own {

Statement::Statement(Database& db, const std::string& sql) {
    if (sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &st_, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db.handle()));
}
Statement::~Statement() { if (st_) sqlite3_finalize(st_); }
void Statement::bind(int i, int64_t v) { sqlite3_bind_int64(st_, i, v); }
void Statement::bind(int i, const std::string& v) {
    sqlite3_bind_text(st_, i, v.c_str(), (int)v.size(), SQLITE_TRANSIENT);
}
void Statement::bindBlob(int i, const uint8_t* p, size_t n) {
    sqlite3_bind_blob(st_, i, p, (int)n, SQLITE_TRANSIENT);
}
void Statement::bindNull(int i) { sqlite3_bind_null(st_, i); }
bool Statement::step() {
    int rc = sqlite3_step(st_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(st_)));
}
void Statement::reset() { sqlite3_reset(st_); }
void Statement::execDone() { while (step()) {} }
int64_t Statement::columnInt64(int c) { return sqlite3_column_int64(st_, c); }
std::string Statement::columnText(int c) {
    const unsigned char* p = sqlite3_column_text(st_, c);
    int n = sqlite3_column_bytes(st_, c);
    return p ? std::string(reinterpret_cast<const char*>(p), n) : std::string();
}
std::vector<uint8_t> Statement::columnBlob(int c) {
    const void* p = sqlite3_column_blob(st_, c);
    int n = sqlite3_column_bytes(st_, c);
    const uint8_t* b = static_cast<const uint8_t*>(p);
    return b ? std::vector<uint8_t>(b, b + n) : std::vector<uint8_t>();
}
bool Statement::columnIsNull(int c) { return sqlite3_column_type(st_, c) == SQLITE_NULL; }

Transaction::Transaction(Database& db) : db_(db) {
    std::string e; db_.exec("BEGIN;", &e);
}
Transaction::~Transaction() { if (active_) { std::string e; db_.exec("ROLLBACK;", &e); } }
void Transaction::commit() { if (active_) { std::string e; db_.exec("COMMIT;", &e); active_ = false; } }

} // namespace own
```
把 `src/data/Statement.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/data/Statement.h src/data/Statement.cpp tests/test_database.cpp tests/tests.vcxproj
git commit -m "feat(data): Statement + Transaction RAII helpers

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: 迁移框架 + schema + 内置主题播种

**Files:**
- Create: `src/data/Migrations.h`, `src/data/Migrations.cpp`
- Modify: `src/data/Database.h/.cpp`(加 `userVersion()`/`setUserVersion()`/`integrityOk()`)
- Test: `tests/test_migrations.cpp`

**Interfaces:**
- Consumes: `Database`、`Statement`。
- Produces:
  - `int Database::userVersion();` `void Database::setUserVersion(int);` `bool Database::integrityOk();`
  - `bool own::migrate(Database& db, std::string* err);` —— 幂等:把 DB 从当前 `user_version` 迁到 `own::kSchemaVersion`(=1),建全部表+索引,并在版本 0→1 时播种内置主题。
  - `constexpr int own::kSchemaVersion = 1;`

- [ ] **Step 1: 写失败测试**

`tests/test_migrations.cpp`:
```cpp
#include "doctest.h"
#include "data/Database.h"
#include "data/Statement.h"
#include "data/Migrations.h"

TEST_CASE("migrate creates schema and is idempotent") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(db.userVersion() == 0);
    REQUIRE(own::migrate(&db == nullptr ? *(own::Database*)nullptr : db, &err)); // 见下修正
}
```
> 注:上面示意有误,按此实现最终测试:
```cpp
TEST_CASE("migrate creates schema, seeds themes, idempotent") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    CHECK(db.userVersion() == own::kSchemaVersion);
    // 表存在
    own::Statement s(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='notes';");
    REQUIRE(s.step()); CHECK(s.columnInt64(0) == 1);
    // 播种了内置主题
    own::Statement t(db, "SELECT COUNT(*) FROM themes WHERE is_builtin=1;");
    REQUIRE(t.step()); CHECK(t.columnInt64(0) >= 4);
    // 再迁一次不报错、主题不翻倍
    REQUIRE(own::migrate(db, &err));
    own::Statement t2(db, "SELECT COUNT(*) FROM themes WHERE is_builtin=1;");
    REQUIRE(t2.step()); CHECK(t2.columnInt64(0) >= 4);
}

TEST_CASE("integrityOk true on fresh db") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    CHECK(db.integrityOk());
}
```
加入 `tests.vcxproj`(删掉上面示意错误的第一个 TEST_CASE)。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`Migrations.h`/`userVersion` 未定义)。

- [ ] **Step 3: 给 Database 加版本/完整性方法**

`Database.h` 增声明:
```cpp
    int userVersion();
    void setUserVersion(int v);
    bool integrityOk();
```
`Database.cpp` 增实现(需要 `#include "data/Statement.h"`):
```cpp
int Database::userVersion() {
    Statement s(*this, "PRAGMA user_version;");
    return s.step() ? (int)s.columnInt64(0) : 0;
}
void Database::setUserVersion(int v) {
    std::string e; exec("PRAGMA user_version=" + std::to_string(v) + ";", &e);
}
bool Database::integrityOk() {
    Statement s(*this, "PRAGMA integrity_check;");
    return s.step() && s.columnText(0) == "ok";
}
```

- [ ] **Step 4: 实现 migrate**

`src/data/Migrations.h`:
```cpp
#pragma once
#include <string>
namespace own {
class Database;
constexpr int kSchemaVersion = 1;
bool migrate(Database& db, std::string* err);
}
```

`src/data/Migrations.cpp`:
```cpp
#include "data/Migrations.h"
#include "data/Database.h"
#include "data/Statement.h"

namespace own {

static const char* kSchemaSqlV1 = R"SQL(
CREATE TABLE IF NOT EXISTS notes(
  id INTEGER PRIMARY KEY, type INTEGER NOT NULL, title TEXT,
  content_blob BLOB, plain_text TEXT, theme_id INTEGER, group_id INTEGER,
  pos_x INTEGER, pos_y INTEGER, width INTEGER, height INTEGER, monitor_id TEXT,
  opacity INTEGER DEFAULT 255, pinned INTEGER DEFAULT 1, rolled_up INTEGER DEFAULT 0,
  visible INTEGER DEFAULT 1, stick_target TEXT, created_at INTEGER, updated_at INTEGER);
CREATE TABLE IF NOT EXISTS groups(id INTEGER PRIMARY KEY, name TEXT, order_idx INTEGER);
CREATE TABLE IF NOT EXISTS tags(id INTEGER PRIMARY KEY, name TEXT UNIQUE);
CREATE TABLE IF NOT EXISTS note_tags(note_id INTEGER, tag_id INTEGER, PRIMARY KEY(note_id,tag_id));
CREATE TABLE IF NOT EXISTS reminders(
  id INTEGER PRIMARY KEY, note_id INTEGER NOT NULL, due_at INTEGER NOT NULL,
  recurrence INTEGER DEFAULT 0, recur_interval INTEGER DEFAULT 1,
  snooze_until INTEGER, sound_path TEXT, enabled INTEGER DEFAULT 1);
CREATE TABLE IF NOT EXISTS themes(
  id INTEGER PRIMARY KEY, name TEXT, bg_color INTEGER, title_color INTEGER,
  text_color INTEGER, is_builtin INTEGER);
CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT);
CREATE INDEX IF NOT EXISTS idx_notes_group ON notes(group_id);
CREATE INDEX IF NOT EXISTS idx_notes_visible ON notes(visible);
CREATE INDEX IF NOT EXISTS idx_note_tags_tag ON note_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_reminders_due ON reminders(due_at, enabled);
)SQL";

static bool seedThemes(Database& db, std::string* err) {
    // 4 个内置主题: 黄/粉/蓝/绿
    struct T { const char* n; int bg, title, text; };
    const T ts[] = {
        {"黄", 0xFFF7B0, 0xF2D24A, 0x202020},
        {"粉", 0xFFCFE0, 0xF29CB8, 0x202020},
        {"蓝", 0xCFE3FF, 0x8FBCF2, 0x202020},
        {"绿", 0xCFF7D6, 0x8FE0A0, 0x202020},
    };
    for (const T& t : ts) {
        Statement s(db, "INSERT INTO themes(name,bg_color,title_color,text_color,is_builtin) VALUES(?,?,?,?,1);");
        s.bind(1, std::string(t.n)); s.bind(2, (int64_t)t.bg);
        s.bind(3, (int64_t)t.title); s.bind(4, (int64_t)t.text);
        s.execDone();
    }
    (void)err; return true;
}

bool migrate(Database& db, std::string* err) {
    int v = db.userVersion();
    if (v >= kSchemaVersion) return true;
    Transaction tx(db);
    if (!db.exec(kSchemaSqlV1, err)) return false;
    if (v < 1) { if (!seedThemes(db, err)) return false; }
    db.setUserVersion(kSchemaVersion);
    tx.commit();
    return true;
}

} // namespace own
```
把 `Migrations.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 5: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 6: Commit**

```bash
git add src/data/Migrations.* src/data/Database.* tests/test_migrations.cpp tests/tests.vcxproj
git commit -m "feat(data): schema migration + builtin theme seeding

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: NoteStore — note 增删改查

**Files:**
- Create: `src/data/NoteStore.h`, `src/data/NoteStore.cpp`
- Test: `tests/test_notestore.cpp`

**Interfaces:**
- Consumes: `Database`、`Statement`、`domain/Models.h`。
- Produces `class NoteStore`(持有 `Database&`):
  - `int64_t insertNote(const Note&);`(返回新 id,写入 `created_at/updated_at`=传入的 `now` 参数)—— 签名:`int64_t insertNote(Note n, int64_t now);`
  - `bool updateNote(const Note& n, int64_t now);`(全字段更新,刷新 `updated_at`)
  - `bool updateGeometry(int64_t id, RectI r, const std::string& monitorId);`
  - `bool updateFlags(int64_t id, int opacity, bool pinned, bool rolledUp, bool visible);`
  - `bool deleteNote(int64_t id);`(级联删 `note_tags`、`reminders`)
  - `std::optional<Note> getNote(int64_t id);`
  - `std::vector<Note> allNotes();`(按 updated_at DESC)

- [ ] **Step 1: 写失败测试**

`tests/test_notestore.cpp`:
```cpp
#include "doctest.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/NoteStore.h"

static own::Database freshDb() {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    return db;
}

TEST_CASE("insert then get roundtrips all fields") {
    auto db = freshDb();
    own::NoteStore store(db);
    own::Note n;
    n.type = own::NoteType::Checklist;
    n.title = "标题";
    n.contentBlob = {10,20,30};
    n.plainText = "买 牛奶";
    n.rect = {5,6,300,400};
    n.opacity = 128; n.pinned = false; n.rolledUp = true; n.visible = false;
    n.stickTarget = "chrome";
    int64_t id = store.insertNote(n, 1000);
    CHECK(id > 0);
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->title == "标题");
    CHECK(got->contentBlob == std::vector<uint8_t>{10,20,30});
    CHECK(got->rect.w == 300);
    CHECK(got->opacity == 128);
    CHECK(got->pinned == false);
    CHECK(got->visible == false);
    CHECK(got->createdAt == 1000);
    CHECK(got->updatedAt == 1000);
    CHECK(static_cast<int>(got->type) == static_cast<int>(own::NoteType::Checklist));
}

TEST_CASE("update refreshes updated_at only") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; n.title = "a";
    int64_t id = store.insertNote(n, 1000);
    auto got = store.getNote(id); got->title = "b";
    REQUIRE(store.updateNote(*got, 2000));
    auto g2 = store.getNote(id);
    CHECK(g2->title == "b");
    CHECK(g2->createdAt == 1000);
    CHECK(g2->updatedAt == 2000);
}

TEST_CASE("delete removes note") {
    auto db = freshDb(); own::NoteStore store(db);
    int64_t id = store.insertNote(own::Note{}, 1000);
    REQUIRE(store.deleteNote(id));
    CHECK_FALSE(store.getNote(id).has_value());
}

TEST_CASE("allNotes sorted by updated_at desc") {
    auto db = freshDb(); own::NoteStore store(db);
    int64_t a = store.insertNote(own::Note{}, 1000);
    int64_t b = store.insertNote(own::Note{}, 3000);
    int64_t c = store.insertNote(own::Note{}, 2000);
    auto all = store.allNotes();
    REQUIRE(all.size() == 3);
    CHECK(all[0].id == b);
    CHECK(all[1].id == c);
    CHECK(all[2].id == a);
}
```
加入 `tests.vcxproj`。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`NoteStore.h` 不存在)。

- [ ] **Step 3: 实现**

`src/data/NoteStore.h`:
```cpp
#pragma once
#include <optional>
#include <vector>
#include "domain/Models.h"
namespace own {
class Database;
class NoteStore {
public:
    explicit NoteStore(Database& db) : db_(db) {}
    int64_t insertNote(Note n, int64_t now);
    bool updateNote(const Note& n, int64_t now);
    bool updateGeometry(int64_t id, RectI r, const std::string& monitorId);
    bool updateFlags(int64_t id, int opacity, bool pinned, bool rolledUp, bool visible);
    bool deleteNote(int64_t id);
    std::optional<Note> getNote(int64_t id);
    std::vector<Note> allNotes();
private:
    Note readRow(class Statement& s);  // 从 SELECT * 顺序读一行
    Database& db_;
};
} // namespace own
```

`src/data/NoteStore.cpp`:
```cpp
#include "data/NoteStore.h"
#include "data/Database.h"
#include "data/Statement.h"

namespace own {

static const char* kCols =
    "id,type,title,content_blob,plain_text,theme_id,group_id,"
    "pos_x,pos_y,width,height,monitor_id,opacity,pinned,rolled_up,"
    "visible,stick_target,created_at,updated_at";

Note NoteStore::readRow(Statement& s) {
    Note n;
    n.id = s.columnInt64(0);
    n.type = (NoteType)s.columnInt64(1);
    n.title = s.columnText(2);
    n.contentBlob = s.columnBlob(3);
    n.plainText = s.columnText(4);
    n.themeId = s.columnInt64(5);
    n.groupId = s.columnInt64(6);
    n.rect = { (int)s.columnInt64(7),(int)s.columnInt64(8),(int)s.columnInt64(9),(int)s.columnInt64(10) };
    n.monitorId = s.columnText(11);
    n.opacity = (int)s.columnInt64(12);
    n.pinned = s.columnInt64(13) != 0;
    n.rolledUp = s.columnInt64(14) != 0;
    n.visible = s.columnInt64(15) != 0;
    n.stickTarget = s.columnText(16);
    n.createdAt = s.columnInt64(17);
    n.updatedAt = s.columnInt64(18);
    return n;
}

int64_t NoteStore::insertNote(Note n, int64_t now) {
    n.createdAt = now; n.updatedAt = now;
    Statement s(db_,
        "INSERT INTO notes(type,title,content_blob,plain_text,theme_id,group_id,"
        "pos_x,pos_y,width,height,monitor_id,opacity,pinned,rolled_up,visible,"
        "stick_target,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    s.bind(1, (int64_t)n.type); s.bind(2, n.title);
    s.bindBlob(3, n.contentBlob.data(), n.contentBlob.size());
    s.bind(4, n.plainText); s.bind(5, n.themeId); s.bind(6, n.groupId);
    s.bind(7, (int64_t)n.rect.x); s.bind(8, (int64_t)n.rect.y);
    s.bind(9, (int64_t)n.rect.w); s.bind(10, (int64_t)n.rect.h);
    s.bind(11, n.monitorId); s.bind(12, (int64_t)n.opacity);
    s.bind(13, (int64_t)(n.pinned?1:0)); s.bind(14, (int64_t)(n.rolledUp?1:0));
    s.bind(15, (int64_t)(n.visible?1:0)); s.bind(16, n.stickTarget);
    s.bind(17, n.createdAt); s.bind(18, n.updatedAt);
    s.execDone();
    return db_.lastInsertRowId();
}

bool NoteStore::updateNote(const Note& n, int64_t now) {
    Statement s(db_,
        "UPDATE notes SET type=?,title=?,content_blob=?,plain_text=?,theme_id=?,group_id=?,"
        "pos_x=?,pos_y=?,width=?,height=?,monitor_id=?,opacity=?,pinned=?,rolled_up=?,"
        "visible=?,stick_target=?,updated_at=? WHERE id=?;");
    s.bind(1, (int64_t)n.type); s.bind(2, n.title);
    s.bindBlob(3, n.contentBlob.data(), n.contentBlob.size());
    s.bind(4, n.plainText); s.bind(5, n.themeId); s.bind(6, n.groupId);
    s.bind(7,(int64_t)n.rect.x); s.bind(8,(int64_t)n.rect.y);
    s.bind(9,(int64_t)n.rect.w); s.bind(10,(int64_t)n.rect.h);
    s.bind(11, n.monitorId); s.bind(12,(int64_t)n.opacity);
    s.bind(13,(int64_t)(n.pinned?1:0)); s.bind(14,(int64_t)(n.rolledUp?1:0));
    s.bind(15,(int64_t)(n.visible?1:0)); s.bind(16, n.stickTarget);
    s.bind(17, now); s.bind(18, n.id);
    s.execDone();
    return true;
}

bool NoteStore::updateGeometry(int64_t id, RectI r, const std::string& monitorId) {
    Statement s(db_, "UPDATE notes SET pos_x=?,pos_y=?,width=?,height=?,monitor_id=? WHERE id=?;");
    s.bind(1,(int64_t)r.x); s.bind(2,(int64_t)r.y); s.bind(3,(int64_t)r.w);
    s.bind(4,(int64_t)r.h); s.bind(5, monitorId); s.bind(6, id);
    s.execDone(); return true;
}

bool NoteStore::updateFlags(int64_t id, int opacity, bool pinned, bool rolledUp, bool visible) {
    Statement s(db_, "UPDATE notes SET opacity=?,pinned=?,rolled_up=?,visible=? WHERE id=?;");
    s.bind(1,(int64_t)opacity); s.bind(2,(int64_t)(pinned?1:0));
    s.bind(3,(int64_t)(rolledUp?1:0)); s.bind(4,(int64_t)(visible?1:0)); s.bind(5, id);
    s.execDone(); return true;
}

bool NoteStore::deleteNote(int64_t id) {
    Transaction tx(db_);
    { Statement s(db_, "DELETE FROM note_tags WHERE note_id=?;"); s.bind(1,id); s.execDone(); }
    { Statement s(db_, "DELETE FROM reminders WHERE note_id=?;"); s.bind(1,id); s.execDone(); }
    { Statement s(db_, "DELETE FROM notes WHERE id=?;"); s.bind(1,id); s.execDone(); }
    tx.commit(); return true;
}

std::optional<Note> NoteStore::getNote(int64_t id) {
    Statement s(db_, std::string("SELECT ")+kCols+" FROM notes WHERE id=?;");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return readRow(s);
}

std::vector<Note> NoteStore::allNotes() {
    Statement s(db_, std::string("SELECT ")+kCols+" FROM notes ORDER BY updated_at DESC, id DESC;");
    std::vector<Note> out;
    while (s.step()) out.push_back(readRow(s));
    return out;
}

} // namespace own
```
把 `NoteStore.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/data/NoteStore.* tests/test_notestore.cpp tests/tests.vcxproj
git commit -m "feat(data): NoteStore note CRUD

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: NoteStore — 搜索与筛选

**Files:**
- Modify: `src/data/NoteStore.h/.cpp`
- Test: `tests/test_notestore.cpp`(追加)

**Interfaces:**
- Consumes: Task 6。
- Produces:
  - `struct NoteQuery { std::string search; int64_t groupId = -1; int64_t tagId = -1; bool onlyVisible = false; };`(`groupId==-1` 表示不按分组过滤;`tagId==-1` 不按标签)
  - `std::vector<Note> NoteStore::query(const NoteQuery& q);`
  - 搜索对 `plain_text` 用 `LIKE`(大小写不敏感由调用方传入已小写关键字;SQL 用 `LIKE '%'||?||'%'`)。

- [ ] **Step 1: 写失败测试(追加)**

```cpp
TEST_CASE("query filters by search substring on plain_text") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note a; a.plainText = "买 牛奶 面包"; store.insertNote(a, 1000);
    own::Note b; b.plainText = "开会 周一"; store.insertNote(b, 2000);
    own::NoteQuery q; q.search = "牛奶";
    auto r = store.query(q);
    REQUIRE(r.size() == 1);
    CHECK(r[0].plainText.find("牛奶") != std::string::npos);
}

TEST_CASE("query onlyVisible excludes hidden") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note a; a.visible = true;  store.insertNote(a, 1000);
    own::Note b; b.visible = false; store.insertNote(b, 2000);
    own::NoteQuery q; q.onlyVisible = true;
    CHECK(store.query(q).size() == 1);
}

TEST_CASE("query filters by group") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note a; a.groupId = 7; store.insertNote(a, 1000);
    own::Note b; b.groupId = 9; store.insertNote(b, 2000);
    own::NoteQuery q; q.groupId = 7;
    auto r = store.query(q);
    REQUIRE(r.size() == 1); CHECK(r[0].groupId == 7);
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`NoteQuery`/`query` 未定义)。

- [ ] **Step 3: 实现**

`NoteStore.h` 增:
```cpp
struct NoteQuery { std::string search; int64_t groupId = -1; int64_t tagId = -1; bool onlyVisible = false; };
```
`class NoteStore` 增:`std::vector<Note> query(const NoteQuery& q);`

`NoteStore.cpp` 增:
```cpp
std::vector<Note> NoteStore::query(const NoteQuery& q) {
    std::string sql = std::string("SELECT ")+kCols+" FROM notes n";
    std::vector<std::string> where;
    if (q.tagId >= 0) sql += " JOIN note_tags nt ON nt.note_id=n.id AND nt.tag_id=?";
    if (!q.search.empty()) where.push_back("plain_text LIKE '%'||?||'%'");
    if (q.groupId >= 0)   where.push_back("group_id=?");
    if (q.onlyVisible)    where.push_back("visible=1");
    for (size_t i=0;i<where.size();++i) sql += (i==0?" WHERE ":" AND ") + where[i];
    sql += " ORDER BY updated_at DESC, id DESC;";
    Statement s(db_, sql);
    int idx = 1;
    if (q.tagId >= 0) s.bind(idx++, q.tagId);
    if (!q.search.empty()) s.bind(idx++, q.search);
    if (q.groupId >= 0) s.bind(idx++, q.groupId);
    std::vector<Note> out;
    while (s.step()) out.push_back(readRow(s));
    return out;
}
```
> 注意:`SELECT id,...` 前缀列名在有 JOIN 时无歧义(均属 notes),但 `kCols` 里是裸列名;JOIN 情况下 SQLite 会正确解析到 `notes` 列,因两表列名不冲突。若日后列名冲突,给 `kCols` 加 `n.` 前缀。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/data/NoteStore.* tests/test_notestore.cpp
git commit -m "feat(data): NoteStore query with search/group/tag/visible filters

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: NoteStore — 分组、标签、提醒

**Files:**
- Modify: `src/data/NoteStore.h/.cpp`
- Test: `tests/test_notestore.cpp`(追加)

**Interfaces:**
- Consumes: Task 6/7。
- Produces:
  - 分组:`int64_t upsertGroup(Group g);` `std::vector<Group> allGroups();` `bool deleteGroup(int64_t id);`(删组后其 note 的 `group_id` 置 0)
  - 标签:`int64_t upsertTag(const std::string& name);`(按 name 唯一,存在则返回既有 id)`std::vector<Tag> allTags();` `bool addTagToNote(int64_t noteId, int64_t tagId);` `bool removeTagFromNote(int64_t noteId, int64_t tagId);` `std::vector<Tag> tagsOfNote(int64_t noteId);`
  - 提醒:`int64_t insertReminder(Reminder r);` `bool updateReminder(const Reminder& r);` `bool deleteReminder(int64_t id);` `std::vector<Reminder> remindersOfNote(int64_t noteId);` `std::vector<Reminder> enabledReminders();`

- [ ] **Step 1: 写失败测试(追加)**

```cpp
TEST_CASE("upsertTag is idempotent by name") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t t1 = s.upsertTag("工作");
    int64_t t2 = s.upsertTag("工作");
    CHECK(t1 == t2);
    CHECK(s.allTags().size() == 1);
}

TEST_CASE("tag attach/detach on note") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t nid = s.insertNote(own::Note{}, 1000);
    int64_t tid = s.upsertTag("紧急");
    REQUIRE(s.addTagToNote(nid, tid));
    REQUIRE(s.tagsOfNote(nid).size() == 1);
    REQUIRE(s.removeTagFromNote(nid, tid));
    CHECK(s.tagsOfNote(nid).empty());
}

TEST_CASE("deleteGroup nulls note.group_id") {
    auto db = freshDb(); own::NoteStore s(db);
    own::Group g; g.name="项目A"; int64_t gid = s.upsertGroup(g);
    own::Note n; n.groupId = gid; int64_t nid = s.insertNote(n, 1000);
    REQUIRE(s.deleteGroup(gid));
    CHECK(s.getNote(nid)->groupId == 0);
}

TEST_CASE("reminder crud and enabled filter") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t nid = s.insertNote(own::Note{}, 1000);
    own::Reminder r; r.noteId = nid; r.dueAt = 5000; r.enabled = true;
    int64_t rid = s.insertReminder(r);
    CHECK(rid > 0);
    own::Reminder r2; r2.noteId = nid; r2.dueAt = 6000; r2.enabled = false;
    s.insertReminder(r2);
    CHECK(s.remindersOfNote(nid).size() == 2);
    CHECK(s.enabledReminders().size() == 1);
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败。

- [ ] **Step 3: 实现(追加到 NoteStore)**

`NoteStore.h` 的 `class NoteStore` 内追加对应声明。`NoteStore.cpp` 追加:
```cpp
// ---- groups ----
int64_t NoteStore::upsertGroup(Group g) {
    if (g.id > 0) {
        Statement s(db_, "UPDATE groups SET name=?,order_idx=? WHERE id=?;");
        s.bind(1,g.name); s.bind(2,(int64_t)g.orderIdx); s.bind(3,g.id); s.execDone();
        return g.id;
    }
    Statement s(db_, "INSERT INTO groups(name,order_idx) VALUES(?,?);");
    s.bind(1,g.name); s.bind(2,(int64_t)g.orderIdx); s.execDone();
    return db_.lastInsertRowId();
}
std::vector<Group> NoteStore::allGroups() {
    Statement s(db_, "SELECT id,name,order_idx FROM groups ORDER BY order_idx,id;");
    std::vector<Group> out;
    while (s.step()) out.push_back({ s.columnInt64(0), s.columnText(1), (int)s.columnInt64(2) });
    return out;
}
bool NoteStore::deleteGroup(int64_t id) {
    Transaction tx(db_);
    { Statement s(db_, "UPDATE notes SET group_id=0 WHERE group_id=?;"); s.bind(1,id); s.execDone(); }
    { Statement s(db_, "DELETE FROM groups WHERE id=?;"); s.bind(1,id); s.execDone(); }
    tx.commit(); return true;
}
// ---- tags ----
int64_t NoteStore::upsertTag(const std::string& name) {
    { Statement s(db_, "SELECT id FROM tags WHERE name=?;"); s.bind(1,name);
      if (s.step()) return s.columnInt64(0); }
    Statement s(db_, "INSERT INTO tags(name) VALUES(?);"); s.bind(1,name); s.execDone();
    return db_.lastInsertRowId();
}
std::vector<Tag> NoteStore::allTags() {
    Statement s(db_, "SELECT id,name FROM tags ORDER BY name;");
    std::vector<Tag> out; while (s.step()) out.push_back({ s.columnInt64(0), s.columnText(1) });
    return out;
}
bool NoteStore::addTagToNote(int64_t noteId, int64_t tagId) {
    Statement s(db_, "INSERT OR IGNORE INTO note_tags(note_id,tag_id) VALUES(?,?);");
    s.bind(1,noteId); s.bind(2,tagId); s.execDone(); return true;
}
bool NoteStore::removeTagFromNote(int64_t noteId, int64_t tagId) {
    Statement s(db_, "DELETE FROM note_tags WHERE note_id=? AND tag_id=?;");
    s.bind(1,noteId); s.bind(2,tagId); s.execDone(); return true;
}
std::vector<Tag> NoteStore::tagsOfNote(int64_t noteId) {
    Statement s(db_, "SELECT t.id,t.name FROM tags t JOIN note_tags nt ON nt.tag_id=t.id "
                     "WHERE nt.note_id=? ORDER BY t.name;");
    s.bind(1,noteId);
    std::vector<Tag> out; while (s.step()) out.push_back({ s.columnInt64(0), s.columnText(1) });
    return out;
}
// ---- reminders ----
static void bindReminder(Statement& s, const Reminder& r, int base) {
    s.bind(base+0, r.noteId); s.bind(base+1, r.dueAt);
    s.bind(base+2,(int64_t)r.recurrence); s.bind(base+3,(int64_t)r.recurInterval);
    s.bind(base+4, r.snoozeUntil); s.bind(base+5, r.soundPath);
    s.bind(base+6,(int64_t)(r.enabled?1:0));
}
int64_t NoteStore::insertReminder(Reminder r) {
    Statement s(db_, "INSERT INTO reminders(note_id,due_at,recurrence,recur_interval,"
                     "snooze_until,sound_path,enabled) VALUES(?,?,?,?,?,?,?);");
    bindReminder(s, r, 1); s.execDone(); return db_.lastInsertRowId();
}
bool NoteStore::updateReminder(const Reminder& r) {
    Statement s(db_, "UPDATE reminders SET note_id=?,due_at=?,recurrence=?,recur_interval=?,"
                     "snooze_until=?,sound_path=?,enabled=? WHERE id=?;");
    bindReminder(s, r, 1); s.bind(8, r.id); s.execDone(); return true;
}
bool NoteStore::deleteReminder(int64_t id) {
    Statement s(db_, "DELETE FROM reminders WHERE id=?;"); s.bind(1,id); s.execDone(); return true;
}
static Reminder readReminder(Statement& s) {
    Reminder r;
    r.id=s.columnInt64(0); r.noteId=s.columnInt64(1); r.dueAt=s.columnInt64(2);
    r.recurrence=(Recurrence)s.columnInt64(3); r.recurInterval=(int)s.columnInt64(4);
    r.snoozeUntil=s.columnInt64(5); r.soundPath=s.columnText(6); r.enabled=s.columnInt64(7)!=0;
    return r;
}
std::vector<Reminder> NoteStore::remindersOfNote(int64_t noteId) {
    Statement s(db_, "SELECT id,note_id,due_at,recurrence,recur_interval,snooze_until,"
                     "sound_path,enabled FROM reminders WHERE note_id=? ORDER BY due_at;");
    s.bind(1,noteId);
    std::vector<Reminder> out; while (s.step()) out.push_back(readReminder(s)); return out;
}
std::vector<Reminder> NoteStore::enabledReminders() {
    Statement s(db_, "SELECT id,note_id,due_at,recurrence,recur_interval,snooze_until,"
                     "sound_path,enabled FROM reminders WHERE enabled=1 ORDER BY due_at;");
    std::vector<Reminder> out; while (s.step()) out.push_back(readReminder(s)); return out;
}
```
> `Statement` 需能被本文件的自由函数使用——它们已在 `own` 命名空间内、`Statement.h` 已 include。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/data/NoteStore.* tests/test_notestore.cpp
git commit -m "feat(data): groups, tags, reminders in NoteStore

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: 清单 JSON 序列化

**Files:**
- Create: `src/domain/ChecklistJson.h`, `src/domain/ChecklistJson.cpp`
- Test: `tests/test_checklist_json.cpp`

**Interfaces:**
- Consumes: `domain/Models.h`、`third_party/json/json.hpp`。
- Produces:
  - `std::vector<uint8_t> own::serializeChecklist(const std::vector<ChecklistItem>&);`(UTF-8 JSON 字节)
  - `std::vector<ChecklistItem> own::deserializeChecklist(const std::vector<uint8_t>&);`(解析失败返回空 vector,**不抛异常**)
  - `std::string own::checklistPlainText(const std::vector<ChecklistItem>&);`(各项 text 以空格连接,已小写)

- [ ] **Step 1: 写失败测试**

`tests/test_checklist_json.cpp`:
```cpp
#include "doctest.h"
#include "domain/ChecklistJson.h"

TEST_CASE("checklist roundtrip") {
    std::vector<own::ChecklistItem> items = {
        {"买牛奶", false, 0}, {"交电费", true, 1}
    };
    auto blob = own::serializeChecklist(items);
    auto back = own::deserializeChecklist(blob);
    REQUIRE(back.size() == 2);
    CHECK(back[0].text == "买牛奶");
    CHECK(back[0].checked == false);
    CHECK(back[1].checked == true);
    CHECK(back[1].order == 1);
}

TEST_CASE("deserialize garbage returns empty, no throw") {
    std::vector<uint8_t> junk = {'x','y','z'};
    auto r = own::deserializeChecklist(junk);
    CHECK(r.empty());
}

TEST_CASE("plain text joins lowercased") {
    std::vector<own::ChecklistItem> items = {{"ABC",false,0},{"Def",false,1}};
    CHECK(own::checklistPlainText(items) == "abc def");
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`ChecklistJson.h` 不存在)。

- [ ] **Step 3: 实现**

`src/domain/ChecklistJson.h`:
```cpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::vector<uint8_t> serializeChecklist(const std::vector<ChecklistItem>& items);
std::vector<ChecklistItem> deserializeChecklist(const std::vector<uint8_t>& blob);
std::string checklistPlainText(const std::vector<ChecklistItem>& items);
}
```

`src/domain/ChecklistJson.cpp`:
```cpp
#include "domain/ChecklistJson.h"
#include "json.hpp"
#include <algorithm>
#include <cctype>
using nlohmann::json;
namespace own {

std::vector<uint8_t> serializeChecklist(const std::vector<ChecklistItem>& items) {
    json arr = json::array();
    for (const auto& it : items)
        arr.push_back({ {"text", it.text}, {"checked", it.checked}, {"order", it.order} });
    std::string s = arr.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<ChecklistItem> deserializeChecklist(const std::vector<uint8_t>& blob) {
    std::vector<ChecklistItem> out;
    if (blob.empty()) return out;
    json j = json::parse(blob.begin(), blob.end(), nullptr, /*allow_exceptions*/false);
    if (!j.is_array()) return out;
    for (const auto& e : j) {
        if (!e.is_object()) continue;
        ChecklistItem it;
        it.text    = e.value("text", std::string());
        it.checked = e.value("checked", false);
        it.order   = e.value("order", 0);
        out.push_back(std::move(it));
    }
    return out;
}

std::string checklistPlainText(const std::vector<ChecklistItem>& items) {
    std::string s;
    for (size_t i=0;i<items.size();++i) { if (i) s += ' '; s += items[i].text; }
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}
} // namespace own
```
把 `ChecklistJson.cpp` 加入 `tests.vcxproj`。
> 注:`std::tolower` 仅对 ASCII 小写;中文不变,符合“已小写”约定(搜索关键字同样按 ASCII 小写处理)。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/domain/ChecklistJson.* tests/test_checklist_json.cpp tests/tests.vcxproj
git commit -m "feat(domain): checklist JSON serialization

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: 笔迹 JSON 序列化

**Files:**
- Create: `src/domain/StrokesJson.h`, `src/domain/StrokesJson.cpp`
- Test: `tests/test_strokes_json.cpp`

**Interfaces:**
- Consumes: `domain/Models.h`、json。
- Produces:
  - `std::vector<uint8_t> own::serializeStrokes(const std::vector<Stroke>&);`
  - `std::vector<Stroke> own::deserializeStrokes(const std::vector<uint8_t>&);`(失败返回空,不抛)

- [ ] **Step 1: 写失败测试**

`tests/test_strokes_json.cpp`:
```cpp
#include "doctest.h"
#include "domain/StrokesJson.h"

TEST_CASE("strokes roundtrip") {
    std::vector<own::Stroke> s = { { 0xFF0000, 4, {{1,2},{3,4},{5,6}} } };
    auto blob = own::serializeStrokes(s);
    auto back = own::deserializeStrokes(blob);
    REQUIRE(back.size() == 1);
    CHECK(back[0].color == 0xFF0000);
    CHECK(back[0].width == 4);
    REQUIRE(back[0].points.size() == 3);
    CHECK(back[0].points[1].first == 3);
    CHECK(back[0].points[1].second == 4);
}

TEST_CASE("strokes deserialize garbage returns empty") {
    CHECK(own::deserializeStrokes({'!','?'}).empty());
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败。

- [ ] **Step 3: 实现**

`src/domain/StrokesJson.h`:
```cpp
#pragma once
#include <vector>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::vector<uint8_t> serializeStrokes(const std::vector<Stroke>& strokes);
std::vector<Stroke> deserializeStrokes(const std::vector<uint8_t>& blob);
}
```

`src/domain/StrokesJson.cpp`:
```cpp
#include "domain/StrokesJson.h"
#include "json.hpp"
using nlohmann::json;
namespace own {

std::vector<uint8_t> serializeStrokes(const std::vector<Stroke>& strokes) {
    json arr = json::array();
    for (const auto& s : strokes) {
        json pts = json::array();
        for (const auto& p : s.points) pts.push_back({ p.first, p.second });
        arr.push_back({ {"color", s.color}, {"width", s.width}, {"points", pts} });
    }
    json root = { {"strokes", arr} };
    std::string str = root.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<Stroke> deserializeStrokes(const std::vector<uint8_t>& blob) {
    std::vector<Stroke> out;
    if (blob.empty()) return out;
    json j = json::parse(blob.begin(), blob.end(), nullptr, false);
    if (!j.is_object() || !j.contains("strokes") || !j["strokes"].is_array()) return out;
    for (const auto& e : j["strokes"]) {
        if (!e.is_object()) continue;
        Stroke s;
        s.color = e.value("color", 0u);
        s.width = e.value("width", 3);
        if (e.contains("points") && e["points"].is_array()) {
            for (const auto& p : e["points"]) {
                if (p.is_array() && p.size()==2 && p[0].is_number_integer() && p[1].is_number_integer())
                    s.points.emplace_back((int)p[0], (int)p[1]);
            }
        }
        out.push_back(std::move(s));
    }
    return out;
}
} // namespace own
```
把 `StrokesJson.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/domain/StrokesJson.* tests/test_strokes_json.cpp tests/tests.vcxproj
git commit -m "feat(domain): drawing strokes JSON serialization

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: 提醒重复计算纯函数

**Files:**
- Create: `src/domain/ReminderRules.h`, `src/domain/ReminderRules.cpp`
- Test: `tests/test_reminder_rules.cpp`

**Interfaces:**
- Consumes: `domain/Models.h`。
- Produces:
  - `bool own::isDue(const Reminder& r, int64_t now);` —— `enabled && ( (snoozeUntil>0 && now>=snoozeUntil) || (snoozeUntil==0 && now>=dueAt) )`。
  - `int64_t own::computeNextDue(const Reminder& r, int64_t firedAt);` —— 依 `recurrence`/`recurInterval` 在 `dueAt` 基础上滚动到 `> firedAt` 的下一个时间;`None` 返回 0(表示不再触发)。天=86400s,周=7*86400s,月按 +N 个自然月(用下方 `addMonths`)。
  - `int64_t own::snooze(int64_t now, int minutes);` —— `now + minutes*60`。

- [ ] **Step 1: 写失败测试**

`tests/test_reminder_rules.cpp`:
```cpp
#include "doctest.h"
#include "domain/ReminderRules.h"

TEST_CASE("isDue respects dueAt and snooze") {
    own::Reminder r; r.enabled = true; r.dueAt = 1000; r.snoozeUntil = 0;
    CHECK_FALSE(own::isDue(r, 999));
    CHECK(own::isDue(r, 1000));
    r.snoozeUntil = 5000;
    CHECK_FALSE(own::isDue(r, 1000));   // 被贪睡压住
    CHECK(own::isDue(r, 5000));
    r.enabled = false;
    CHECK_FALSE(own::isDue(r, 9999));
}

TEST_CASE("computeNextDue none returns 0") {
    own::Reminder r; r.recurrence = own::Recurrence::None; r.dueAt = 1000;
    CHECK(own::computeNextDue(r, 1000) == 0);
}

TEST_CASE("computeNextDue daily rolls past firedAt") {
    own::Reminder r; r.recurrence = own::Recurrence::Daily; r.recurInterval = 1; r.dueAt = 1000;
    // 一天=86400。firedAt=1000 → 次日 87400
    CHECK(own::computeNextDue(r, 1000) == 1000 + 86400);
    // 已过好几天：firedAt=1000+86400*3+5 → 下一个是 1000+86400*4
    CHECK(own::computeNextDue(r, 1000 + 86400*3 + 5) == 1000 + 86400*4);
}

TEST_CASE("computeNextDue weekly interval 2") {
    own::Reminder r; r.recurrence = own::Recurrence::Weekly; r.recurInterval = 2; r.dueAt = 1000;
    CHECK(own::computeNextDue(r, 1000) == 1000 + 14*86400);
}

TEST_CASE("snooze adds minutes") {
    CHECK(own::snooze(1000, 10) == 1000 + 600);
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`ReminderRules.h` 不存在)。

- [ ] **Step 3: 实现**

`src/domain/ReminderRules.h`:
```cpp
#pragma once
#include <cstdint>
#include "domain/Models.h"
namespace own {
bool isDue(const Reminder& r, int64_t now);
int64_t computeNextDue(const Reminder& r, int64_t firedAt);
int64_t snooze(int64_t now, int minutes);
}
```

`src/domain/ReminderRules.cpp`:
```cpp
#include "domain/ReminderRules.h"
#include <ctime>
namespace own {

bool isDue(const Reminder& r, int64_t now) {
    if (!r.enabled) return false;
    if (r.snoozeUntil > 0) return now >= r.snoozeUntil;
    return now >= r.dueAt;
}

static int64_t addMonthsUtc(int64_t t, int months) {
    time_t tt = (time_t)t;
    std::tm g{};
#if defined(_WIN32)
    gmtime_s(&g, &tt);
#else
    g = *gmtime(&tt);
#endif
    g.tm_mon += months;                 // mktime/ _mkgmtime 会规整溢出月份
#if defined(_WIN32)
    return (int64_t)_mkgmtime(&g);
#else
    return (int64_t)timegm(&g);
#endif
}

int64_t computeNextDue(const Reminder& r, int64_t firedAt) {
    if (r.recurrence == Recurrence::None) return 0;
    int interval = r.recurInterval > 0 ? r.recurInterval : 1;
    int64_t next = r.dueAt;
    int guard = 0;
    while (next <= firedAt && guard++ < 100000) {
        switch (r.recurrence) {
            case Recurrence::Daily:   next += (int64_t)86400 * interval; break;
            case Recurrence::Weekly:  next += (int64_t)7 * 86400 * interval; break;
            case Recurrence::Monthly: next = addMonthsUtc(next, interval); break;
            default: return 0;
        }
    }
    return next;
}

int64_t snooze(int64_t now, int minutes) { return now + (int64_t)minutes * 60; }

} // namespace own
```
把 `ReminderRules.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/domain/ReminderRules.* tests/test_reminder_rules.cpp tests/tests.vcxproj
git commit -m "feat(domain): reminder recurrence/snooze pure functions

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 12: 窗口越界钳制纯函数

**Files:**
- Create: `src/domain/Geometry.h`, `src/domain/Geometry.cpp`
- Test: `tests/test_geometry.cpp`

**Interfaces:**
- Consumes: `domain/Models.h`(`RectI`)。
- Produces:
  - `RectI own::clampRectToWorkArea(RectI note, const std::vector<RectI>& monitors);`
  - 规则:若 `note` 与任一 monitor 有交集则原样返回;否则把 `note` 移动(不改尺寸)到"第一个 monitor"的左上角内(留 `note` 完整可见,若 note 比屏幕大则左上对齐)。

- [ ] **Step 1: 写失败测试**

`tests/test_geometry.cpp`:
```cpp
#include "doctest.h"
#include "domain/Geometry.h"

TEST_CASE("visible rect unchanged") {
    std::vector<own::RectI> mons = { {0,0,1920,1080} };
    own::RectI n{100,100,200,150};
    auto r = own::clampRectToWorkArea(n, mons);
    CHECK(r.x==100); CHECK(r.y==100); CHECK(r.w==200); CHECK(r.h==150);
}

TEST_CASE("offscreen rect moved onto first monitor keeping size") {
    std::vector<own::RectI> mons = { {0,0,1920,1080} };
    own::RectI n{5000,5000,200,150};                 // 完全在屏外
    auto r = own::clampRectToWorkArea(n, mons);
    CHECK(r.w==200); CHECK(r.h==150);
    // 完整落在屏内
    CHECK(r.x >= 0); CHECK(r.y >= 0);
    CHECK(r.x + r.w <= 1920); CHECK(r.y + r.h <= 1080);
}

TEST_CASE("no monitors returns input unchanged") {
    own::RectI n{5000,5000,200,150};
    auto r = own::clampRectToWorkArea(n, {});
    CHECK(r.x==5000);
}
```

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`Geometry.h` 不存在)。

- [ ] **Step 3: 实现**

`src/domain/Geometry.h`:
```cpp
#pragma once
#include <vector>
#include "domain/Models.h"
namespace own {
RectI clampRectToWorkArea(RectI note, const std::vector<RectI>& monitors);
}
```

`src/domain/Geometry.cpp`:
```cpp
#include "domain/Geometry.h"
#include <algorithm>
namespace own {

static bool intersects(const RectI& a, const RectI& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

RectI clampRectToWorkArea(RectI note, const std::vector<RectI>& monitors) {
    if (monitors.empty()) return note;
    for (const auto& m : monitors) if (intersects(note, m)) return note;
    const RectI& m = monitors.front();
    RectI r = note;
    r.x = std::max(m.x, std::min(note.x, m.x + m.w - note.w));
    r.y = std::max(m.y, std::min(note.y, m.y + m.h - note.h));
    if (r.x < m.x) r.x = m.x;         // note 比屏宽时左对齐
    if (r.y < m.y) r.y = m.y;
    return r;
}
} // namespace own
```
把 `Geometry.cpp` 加入 `tests.vcxproj`。

- [ ] **Step 4: 运行验证通过**

Run: `msbuild ... && ./x64/Debug/tests.exe`
Expected: PASS(全部数据层测试)。

- [ ] **Step 5: Commit**

```bash
git add src/domain/Geometry.* tests/test_geometry.cpp tests/tests.vcxproj
git commit -m "feat(domain): clamp offscreen note geometry to visible monitors

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage(对照设计文档 §4/§5/§6/§7):**
- §4 schema(notes/groups/tags/note_tags/reminders/themes/settings + 索引)→ Task 5。✓
- §4 blob 约定(清单/涂鸦 JSON)→ Task 9/10。✓
- §4 `notes.db` 命名与搜索用 LIKE → Global Constraints + Task 7。✓
- §5 数据流里的持久化(insert/update/geometry/flags/delete/query)→ Task 6/7。分组/标签/提醒 → Task 8。✓
- §6 错误处理:`integrity_check` → Task 5;迁移事务 → Task 5;JSON 解析失败保留/不抛 → Task 9/10;off-screen 钳制 → Task 12。✓(注:DB 打开回落路径、单实例互斥、托盘重建等属应用层,置于 P2/P6,不在数据层计划内。)
- §7 测试:doctest 工程 → Task 1;`computeNextDue(now 注入)` → Task 11。✓
- §7 迁移/搜索/JSON 往返/off-screen/LIKE 均有对应测试。✓

**Placeholder scan:** 无 TBD/TODO;每个代码步骤含完整代码。Task 5 Step 1 里我保留了一段"示意错误"的 TEST_CASE 并明确标注删除——执行者须以其后的最终版本为准。✓

**Type consistency:** `NoteQuery`(Task 7)、`insertNote(Note, int64_t now)`(Task 6)、`Recurrence`/`computeNextDue`(Task 11)、`RectI`(Models/Geometry)在各处签名一致;`kCols` 列顺序与 `readRow` 索引一一对应(0..18)。`Statement`/`Transaction` 接口在 Task 4 定义,后续任务用法一致。✓

**范围外(明确留给后续计划):** DB 路径解析与 `%APPDATA%` 回落、`.corrupt` 备份重建、`.bak` 迁移前快照、`SettingsStore` 读写封装 —— 这些需要文件系统/应用启动流程,放入 **P2(应用启动)** 更合适;此处数据层只保证 `integrityOk()` 与事务化迁移可用。
