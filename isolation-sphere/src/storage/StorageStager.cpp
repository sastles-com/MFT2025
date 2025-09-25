#include "storage/StorageStager.h"

#include <Arduino.h>
#include <FS.h>
#include <esp_task_wdt.h>

#include <vector>

namespace {
constexpr std::size_t kCopyBufferSize = 2048;

bool fsExists(fs::FS &fs, const char *path) {
  return fs.exists(path);
}

bool fsIsDirectory(fs::FS &fs, const char *path) {
  File entry = fs.open(path, FILE_READ);
  if (!entry) {
    return false;
  }
  bool isDir = entry.isDirectory();
  entry.close();
  return isDir;
}

std::vector<StorageStager::Entry> fsList(fs::FS &fs, const char *path) {
  std::vector<StorageStager::Entry> entries;
  File dir = fs.open(path, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return entries;
  }

  File child = dir.openNextFile();
  while (child) {
    StorageStager::Entry entry;
    entry.name = child.name();
    // child.name() returns full path, reduce to immediate name
    if (!entry.name.empty()) {
      auto pos = entry.name.find_last_of('/') ;
      if (pos != std::string::npos) {
        entry.name = entry.name.substr(pos + 1);
      }
    }
    entry.isDirectory = child.isDirectory();
    entries.push_back(entry);
    File next = dir.openNextFile();
    child.close();
    child = next;
  }
  dir.close();
  return entries;
}

bool fsMakeDir(fs::FS &fs, const char *path) {
  return fs.mkdir(path);
}

bool fsCopyFile(fs::FS &destination, fs::FS &source, const char *srcPath, const char *dstPath) {
  File in = source.open(srcPath, FILE_READ);
  if (!in) {
    Serial.printf("[StorageStager] Failed to open %s for read\n", srcPath);
    return false;
  }

  File out = destination.open(dstPath, FILE_WRITE);
  if (!out) {
    Serial.printf("[StorageStager] Failed to open %s for write\n", dstPath);
    in.close();
    return false;
  }

  std::vector<std::uint8_t> buffer(kCopyBufferSize);
  while (in.available()) {
    size_t read = in.read(buffer.data(), buffer.size());
    if (!read) {
      break;
    }
    size_t written = out.write(buffer.data(), read);
    if (written != read) {
      Serial.printf("[StorageStager] Short write to %s\n", dstPath);
      in.close();
      out.close();
      return false;
    }
  }

  in.close();
  out.close();
  return true;
}
}

StorageStager::StorageStager(FsOps source, FsOps destination)
    : source_(std::move(source)), destination_(std::move(destination)) {}

bool StorageStager::stageDirectory(const char *path, bool skipMissing) {
  if (!path || !source_.exists || !source_.isDirectory || !destination_.exists || !destination_.isDirectory) {
    return false;
  }

  const std::string root = normalizePath(path);
  if (!source_.exists(root.c_str())) {
    return skipMissing;
  }

  if (!source_.isDirectory(root.c_str())) {
    if (!destination_.makeDir || !destination_.copyFile) {
      return false;
    }
    if (!ensureDestinationDir(parentPath(root))) {
      return false;
    }
    return destination_.copyFile(root.c_str(), root.c_str());
  }

  if (!ensureDestinationDir(root)) {
    return false;
  }

  return stageDirectoryRecursive(root);
}

StorageStager::FsOps StorageStager::makeSourceFsOps(fs::FS &fs) {
  FsOps ops;
  ops.exists = [&fs](const char *path) { return fsExists(fs, path); };
  ops.isDirectory = [&fs](const char *path) { return fsIsDirectory(fs, path); };
  ops.list = [&fs](const char *path) { return fsList(fs, path); };
  return ops;
}

StorageStager::FsOps StorageStager::makeDestinationFsOps(fs::FS &destination, fs::FS &source) {
  FsOps ops;
  ops.exists = [&destination](const char *path) { return fsExists(destination, path); };
  ops.isDirectory = [&destination](const char *path) { return fsIsDirectory(destination, path); };
  ops.list = [&destination](const char *path) { return fsList(destination, path); };
  ops.makeDir = [&destination](const char *path) { return fsMakeDir(destination, path); };
  ops.copyFile = [&destination, &source](const char *srcPath, const char *dstPath) {
    return fsCopyFile(destination, source, srcPath, dstPath);
  };
  return ops;
}

std::string StorageStager::normalizePath(const std::string &path) {
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

std::string StorageStager::joinPath(const std::string &base, const std::string &name) {
  if (base.empty() || base == "/") {
    return std::string("/") + name;
  }
  if (!name.empty() && name.front() == '/') {
    return normalizePath(name);
  }
  return base + "/" + name;
}

std::string StorageStager::parentPath(const std::string &path) {
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

bool StorageStager::ensureDestinationDir(const std::string &path) {
  if (!destination_.exists || !destination_.isDirectory) {
    return false;
  }

  auto normalized = normalizePath(path);
  if (normalized.empty() || normalized == "/") {
    return true;
  }

  if (destination_.exists(normalized.c_str()) && destination_.isDirectory(normalized.c_str())) {
    return true;
  }

  if (!ensureDestinationDir(parentPath(normalized))) {
    return false;
  }

  if (!destination_.makeDir) {
    return destination_.exists(normalized.c_str()) && destination_.isDirectory(normalized.c_str());
  }

  if (!destination_.makeDir(normalized.c_str())) {
    if (!(destination_.exists(normalized.c_str()) && destination_.isDirectory(normalized.c_str()))) {
      return false;
    }
  }
  return true;
}

bool StorageStager::stageDirectoryRecursive(const std::string &path) {
  if (!source_.list) {
    return false;
  }

  auto entries = source_.list(path.c_str());

  for (const auto &entry : entries) {
    esp_task_wdt_reset();
    if (entry.isDirectory) {
      continue;
    }
    const auto srcPath = joinPath(path, entry.name);
    if (!destination_.copyFile) {
      return false;
    }
    if (!ensureDestinationDir(parentPath(srcPath))) {
      return false;
    }
    if (!destination_.copyFile(srcPath.c_str(), srcPath.c_str())) {
      return false;
    }
  }

  for (const auto &entry : entries) {
    esp_task_wdt_reset();
    if (!entry.isDirectory) {
      continue;
    }
    const auto srcPath = joinPath(path, entry.name);
    if (!ensureDestinationDir(srcPath)) {
      return false;
    }
    if (!stageDirectoryRecursive(srcPath)) {
      return false;
    }
  }
  return true;
}
