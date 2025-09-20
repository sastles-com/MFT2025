#include <M5Unified.h>
#include <LittleFS.h>
#include <PSRamFS.h>

namespace {
constexpr const char *kImageRoot = "/images";
constexpr size_t kCopyBufferSize = 2048;

bool copyFile(fs::FS &src, fs::FS &dst, const char *srcPath, const char *dstPath) {
  File in = src.open(srcPath, FILE_READ);
  if (!in) {
    Serial.printf("[LittleFS] failed to open %s for read\n", srcPath);
    return false;
  }
  File out = dst.open(dstPath, FILE_WRITE);
  if (!out) {
    Serial.printf("[PSRamFS] failed to open %s for write\n", dstPath);
    in.close();
    return false;
  }

  uint8_t buffer[kCopyBufferSize];
  while (in.available()) {
    size_t read = in.read(buffer, sizeof(buffer));
    if (!read) {
      break;
    }
    size_t written = out.write(buffer, read);
    if (written != read) {
      Serial.printf("[PSRamFS] short write on %s\n", dstPath);
      in.close();
      out.close();
      return false;
    }
  }

  in.close();
  out.close();
  return true;
}

bool mirrorDirectory(fs::FS &src, fs::FS &dst, const char *path) {
  File dir = src.open(path);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[LittleFS] directory %s not found\n", path);
    return false;
  }

  if (!dst.exists(path)) {
    if (!dst.mkdir(path)) {
      Serial.printf("[PSRamFS] failed to mkdir %s\n", path);
      dir.close();
      return false;
    }
  }

  File entry = dir.openNextFile();
  while (entry) {
    String entryName = entry.name();
    if (entry.isDirectory()) {
      if (!mirrorDirectory(src, dst, entryName.c_str())) {
        entry.close();
        dir.close();
        return false;
      }
    } else {
      if (!copyFile(src, dst, entryName.c_str(), entryName.c_str())) {
        entry.close();
        dir.close();
        return false;
      }
    }
    entry = dir.openNextFile();
  }

  dir.close();
  return true;
}

bool mountFileSystems() {
  if (!LittleFS.begin(false)) {
    Serial.println("[LittleFS] mount failed, formatting...");
    if (!LittleFS.begin(true)) {
      Serial.println("[LittleFS] mount failed after format");
      return false;
    }
  }

  if (!PSRamFS.begin()) {
    Serial.println("[PSRamFS] mount failed");
    return false;
  }

  return true;
}

bool stageAssets() {
  if (!LittleFS.exists(kImageRoot)) {
    Serial.println("[LittleFS] no image assets to mirror");
    return true;
  }
  return mirrorDirectory(LittleFS, PSRamFS, kImageRoot);
}
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Isolation Sphere booting...");

  if (!mountFileSystems()) {
    M5.Log.println("Filesystem init failed");
    while (true) {
      delay(1000);
    }
  }

  if (!stageAssets()) {
    M5.Log.println("Asset staging failed");
  }

  M5.Log.println("LittleFS and PSRamFS ready");
}

void loop() {
  M5.update();
  delay(16);
}
