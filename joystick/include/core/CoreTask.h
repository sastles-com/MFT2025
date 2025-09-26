#pragma once

#include <cstdint>
#include <functional>

/**
 * @brief 基底 FreeRTOS タスク。タスクの立ち上げと周期スリープを共通化する。
 */
class CoreTask {
 public:
  struct TaskConfig {
    const char *name = "CoreTask";
    std::uint32_t stackSize = 4096;
    std::uint32_t priority = 1;
    int coreId = 0;
    std::uint32_t loopIntervalMs = 10;
  };

  struct Hooks {
    std::function<bool(CoreTask &)> launch;
    std::function<void(std::uint32_t)> delay;
  };

  explicit CoreTask(TaskConfig config);
  virtual ~CoreTask() = default;

  bool start();
  bool isStarted() const;

  void setHooks(Hooks hooks);
  Hooks hooks() const;

  void runOnceForTest();

  const TaskConfig &config() const;

  static Hooks makeDefaultHooks();

 protected:
  virtual void setup() = 0;
  virtual void loop() = 0;

  void sleep(std::uint32_t ms);

 private:
  static void taskEntry(void *param);
  void runTaskLoop();

  TaskConfig config_;
  Hooks hooks_{};
  bool started_ = false;
  bool setupDone_ = false;
};
