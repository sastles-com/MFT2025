#pragma once

#include <functional>

class StorageManager {
 public:
  struct Hooks {
    std::function<bool(bool)> littlefsBegin;
    std::function<bool(bool)> psramfsBegin;
  };

  explicit StorageManager(Hooks hooks = Hooks{});

  bool begin(bool formatOnLittleFail = true, bool formatOnPsFail = true);

  bool isLittleFsMounted() const;

  bool isPsRamFsMounted() const;

 private:
  Hooks hooks_;
  bool littleMounted_ = false;
  bool psMounted_ = false;
};
