#pragma once

#include <functional>
#include <string>
#include <vector>

#include <FS.h>

class StorageStager {
 public:
  struct Entry {
    std::string name;
    bool isDirectory;
  };

  struct FsOps {
    std::function<bool(const char *)> exists;
    std::function<bool(const char *)> isDirectory;
    std::function<std::vector<Entry>(const char *)> list;
    std::function<bool(const char *)> makeDir;
    std::function<bool(const char *, const char *)> copyFile;
  };

  StorageStager(FsOps source, FsOps destination);

  bool stageDirectory(const char *path, bool skipMissing = true);

  static FsOps makeSourceFsOps(fs::FS &fs);
  static FsOps makeDestinationFsOps(fs::FS &destination, fs::FS &source);

 private:
  static std::string normalizePath(const std::string &path);
  static std::string joinPath(const std::string &base, const std::string &name);
  static std::string parentPath(const std::string &path);

  bool ensureDestinationDir(const std::string &path);
  bool stageDirectoryRecursive(const std::string &path);

  FsOps source_;
  FsOps destination_;
};
