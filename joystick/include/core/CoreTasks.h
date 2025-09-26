#pragma once

#include "core/CoreTask.h"
#include "core/SharedState.h"

#include <cstdint>

class Core0Task : public CoreTask {
 public:
  Core0Task(const CoreTask::TaskConfig &config, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
  SharedState &sharedState_;
  std::uint32_t sequence_ = 0;
};

class Core1Task : public CoreTask {
 public:
  Core1Task(const CoreTask::TaskConfig &config, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
 SharedState &sharedState_;
 std::uint32_t lastLoggedSequence_ = 0;
 bool hasLogged_ = false;
 std::uint32_t lastLogMs_ = 0;
  std::uint32_t lastCommLogMs_ = 0;
};
