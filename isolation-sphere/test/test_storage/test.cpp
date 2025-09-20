#include <unity.h>

#include <cstdint>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "storage/StorageManager.h"
#include "storage/StorageStager.h"

namespace {
struct CallTracker {
  std::uint8_t littleAttempts = 0;
  std::uint8_t littleFormats = 0;
  std::uint8_t psAttempts = 0;
};

std::string normalizePath(const std::string &path) {
  if (path.empty()) {
    return "/";
  }
  if (path == "/") {
    return path;
  }
  if (path.back() == '/') {
    return path.substr(0, path.size() - 1);
  }
  return path;
}

std::string parentPath(const std::string &path) {
  auto normalized = normalizePath(path);
  if (normalized == "/") {
    return "/";
  }
  auto pos = normalized.find_last_of('/');
  if (pos == std::string::npos || pos == 0) {
    return "/";
  }
  return normalized.substr(0, pos);
}

class FakeFs {
 public:
  explicit FakeFs(bool failCreate = false, bool failCopy = false)
      : failCreate_(failCreate), failCopy_(failCopy) {
    directories_.insert("/");
  }

  void addDirectory(const std::string &path) {
    directories_.insert(normalizePath(path));
  }

  void addFile(const std::string &path) {
    auto normalized = normalizePath(path);
    files_.insert(normalized);
    directories_.insert(parentPath(normalized));
  }

  bool exists(const std::string &path) const {
    auto normalized = normalizePath(path);
    return directories_.count(normalized) > 0 || files_.count(normalized) > 0;
  }

  bool isDirectory(const std::string &path) const {
    return directories_.count(normalizePath(path)) > 0;
  }

  std::vector<StorageStager::Entry> list(const std::string &path) const {
    std::vector<StorageStager::Entry> entries;
    auto base = normalizePath(path);
    for (const auto &dir : directories_) {
      if (dir == base) {
        continue;
      }
      if (parentPath(dir) == base) {
        auto name = dir.substr(base == "/" ? 1 : base.size() + 1);
        entries.push_back(StorageStager::Entry{name, true});
      }
    }
    for (const auto &file : files_) {
      if (parentPath(file) == base) {
        auto name = file.substr(base == "/" ? 1 : base.size() + 1);
        entries.push_back(StorageStager::Entry{name, false});
      }
    }
    return entries;
  }

  bool makeDir(const std::string &path) {
    auto normalized = normalizePath(path);
    createdDirs.push_back(normalized);
    if (failCreate_) {
      failedDirs.push_back(normalized);
      return false;
    }
    directories_.insert(normalized);
    return true;
  }

  bool copyFile(const std::string &srcPath, const std::string &dstPath, const FakeFs &source) {
    auto normalizedDst = normalizePath(dstPath);
    copyAttempts.emplace_back(srcPath, normalizedDst);
    if (failCopy_) {
      failedCopies.emplace_back(srcPath, normalizedDst);
      return false;
    }
    if (!source.files_.count(normalizePath(srcPath))) {
      failedCopies.emplace_back(srcPath, normalizedDst);
      return false;
    }
    files_.insert(normalizedDst);
    directories_.insert(parentPath(normalizedDst));
    return true;
  }

  std::vector<std::string> createdDirs;
  std::vector<std::string> failedDirs;
  std::vector<std::pair<std::string, std::string>> copyAttempts;
  std::vector<std::pair<std::string, std::string>> failedCopies;

 private:
  std::set<std::string> directories_;
  std::set<std::string> files_;
  bool failCreate_ = false;
  bool failCopy_ = false;
};

StorageManager makeManagerForSuccessScenario(CallTracker &tracker) {
  StorageManager::Hooks hooks;
  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    if (format) {
      ++tracker.littleFormats;
    }
    return !format;  // succeed on first non-format attempt
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    TEST_ASSERT_FALSE_MESSAGE(format, "PSRamFS.begin should not request format in success path");
    return true;
  };
  return StorageManager{hooks};
}

StorageStager::FsOps makeSourceOps(FakeFs &fs) {
  StorageStager::FsOps ops;
  ops.exists = [&fs](const char *path) { return fs.exists(path); };
  ops.isDirectory = [&fs](const char *path) { return fs.isDirectory(path); };
  ops.list = [&fs](const char *path) { return fs.list(path); };
  ops.makeDir = nullptr;
  ops.copyFile = nullptr;
  return ops;
}

StorageStager::FsOps makeDestOps(FakeFs &dest, FakeFs &source) {
  StorageStager::FsOps ops;
  ops.exists = [&dest](const char *path) { return dest.exists(path); };
  ops.isDirectory = [&dest](const char *path) { return dest.isDirectory(path); };
  ops.list = [&dest](const char *path) { return dest.list(path); };
  ops.makeDir = [&dest](const char *path) { return dest.makeDir(path); };
  ops.copyFile = [&dest, &source](const char *srcPath, const char *dstPath) {
    return dest.copyFile(srcPath, dstPath, source);
  };
  return ops;
}
}

void test_begin_successfully_mounts_all() {
  CallTracker tracker;
  auto manager = makeManagerForSuccessScenario(tracker);

  TEST_ASSERT_TRUE(manager.begin());
  TEST_ASSERT_TRUE(manager.isLittleFsMounted());
  TEST_ASSERT_TRUE(manager.isPsRamFsMounted());
  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(0, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.psAttempts);
}

void test_begin_formats_littlefs_when_initial_mount_fails() {
  StorageManager::Hooks hooks;
  CallTracker tracker;

  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    if (format) {
      ++tracker.littleFormats;
      return true;
    }
    return false;
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    TEST_ASSERT_FALSE_MESSAGE(format, "PSRamFS should not request format in retry path");
    return true;
  };

  StorageManager manager{hooks};
  TEST_ASSERT_TRUE(manager.begin());
  TEST_ASSERT_TRUE(manager.isLittleFsMounted());
  TEST_ASSERT_TRUE(manager.isPsRamFsMounted());
  TEST_ASSERT_EQUAL_UINT8(2, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.psAttempts);
}

void test_begin_fails_when_littlefs_never_mounts() {
  StorageManager::Hooks hooks;
  CallTracker tracker;

  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    if (format) {
      ++tracker.littleFormats;
    }
    return false;
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    TEST_FAIL_MESSAGE("PSRamFS.begin should not be called when LittleFS mount fails");
    return false;
  };

  StorageManager manager{hooks};
  TEST_ASSERT_FALSE(manager.begin(true, false));
  TEST_ASSERT_FALSE(manager.isLittleFsMounted());
  TEST_ASSERT_FALSE(manager.isPsRamFsMounted());
  TEST_ASSERT_EQUAL_UINT8(2, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(0, tracker.psAttempts);
}

void test_begin_fails_when_psramfs_mount_fails() {
  StorageManager::Hooks hooks;
  CallTracker tracker;

  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    TEST_ASSERT_FALSE_MESSAGE(format, "LittleFS should not require format in this scenario");
    return true;
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    if (format) {
      TEST_FAIL_MESSAGE("PSRamFS format retry not supported yet");
    }
    return false;
  };

  StorageManager manager{hooks};
  TEST_ASSERT_FALSE(manager.begin(true, false));
  TEST_ASSERT_TRUE(manager.isLittleFsMounted());
  TEST_ASSERT_FALSE(manager.isPsRamFsMounted());
  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(0, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.psAttempts);
}

void test_begin_formats_psramfs_when_retry_enabled() {
  StorageManager::Hooks hooks;
  CallTracker tracker;

  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    TEST_ASSERT_FALSE_MESSAGE(format, "LittleFS should not request format in PS retry test");
    return true;
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    if (tracker.psAttempts == 1) {
      TEST_ASSERT_FALSE_MESSAGE(format, "First PSRamFS attempt should not format");
      return false;
    }
    TEST_ASSERT_TRUE_MESSAGE(format, "Second PSRamFS attempt should request format");
    return true;
  };

  StorageManager manager{hooks};
  TEST_ASSERT_TRUE(manager.begin(true, true));
  TEST_ASSERT_TRUE(manager.isLittleFsMounted());
  TEST_ASSERT_TRUE(manager.isPsRamFsMounted());
  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(0, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(2, tracker.psAttempts);
}

void test_stage_returns_true_when_source_missing() {
  FakeFs source;
  FakeFs dest;

  StorageStager stager(makeSourceOps(source), makeDestOps(dest, source));
  TEST_ASSERT_TRUE(stager.stageDirectory("/images"));
  TEST_ASSERT_TRUE(dest.createdDirs.empty());
  TEST_ASSERT_TRUE(dest.copyAttempts.empty());
}

void test_stage_copies_nested_files() {
  FakeFs source;
  source.addDirectory("/images");
  source.addFile("/images/a.jpg");
  source.addDirectory("/images/sub");
  source.addFile("/images/sub/b.jpg");

  FakeFs dest;
  StorageStager stager(makeSourceOps(source), makeDestOps(dest, source));

  TEST_ASSERT_TRUE(stager.stageDirectory("/images"));
  TEST_ASSERT_GREATER_OR_EQUAL(1, static_cast<int>(dest.createdDirs.size()));
  TEST_ASSERT_TRUE(dest.createdDirs.end() != std::find(dest.createdDirs.begin(), dest.createdDirs.end(), "/images"));
  TEST_ASSERT_TRUE(dest.createdDirs.end() != std::find(dest.createdDirs.begin(), dest.createdDirs.end(), "/images/sub"));
  TEST_ASSERT_EQUAL_UINT32(2, dest.copyAttempts.size());
  TEST_ASSERT_EQUAL_STRING("/images/a.jpg", dest.copyAttempts[0].first.c_str());
  TEST_ASSERT_EQUAL_STRING("/images/a.jpg", dest.copyAttempts[0].second.c_str());
  TEST_ASSERT_EQUAL_STRING("/images/sub/b.jpg", dest.copyAttempts[1].first.c_str());
  TEST_ASSERT_EQUAL_STRING("/images/sub/b.jpg", dest.copyAttempts[1].second.c_str());
}

void test_stage_fails_when_mkdir_fails() {
  FakeFs source;
  source.addDirectory("/images");
  source.addFile("/images/a.jpg");

  FakeFs dest(/*failCreate*/ true, /*failCopy*/ false);
  StorageStager stager(makeSourceOps(source), makeDestOps(dest, source));

  TEST_ASSERT_FALSE(stager.stageDirectory("/images"));
  TEST_ASSERT_FALSE(dest.failedDirs.empty());
}

void test_stage_fails_when_copy_fails() {
  FakeFs source;
  source.addDirectory("/images");
  source.addFile("/images/a.jpg");

  FakeFs dest(/*failCreate*/ false, /*failCopy*/ true);
  StorageStager stager(makeSourceOps(source), makeDestOps(dest, source));

  TEST_ASSERT_FALSE(stager.stageDirectory("/images"));
  TEST_ASSERT_FALSE(dest.failedCopies.empty());
}

void setUp() {}

void tearDown() {}

int runUnityTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_successfully_mounts_all);
  RUN_TEST(test_begin_formats_littlefs_when_initial_mount_fails);
  RUN_TEST(test_begin_fails_when_littlefs_never_mounts);
  RUN_TEST(test_begin_fails_when_psramfs_mount_fails);
  RUN_TEST(test_begin_formats_psramfs_when_retry_enabled);
  RUN_TEST(test_stage_returns_true_when_source_missing);
  RUN_TEST(test_stage_copies_nested_files);
  RUN_TEST(test_stage_fails_when_mkdir_fails);
  RUN_TEST(test_stage_fails_when_copy_fails);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}
