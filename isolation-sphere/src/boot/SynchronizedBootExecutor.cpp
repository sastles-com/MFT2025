#include "boot/SynchronizedBootExecutor.h"

namespace Boot {

SynchronizedBootExecutor::SynchronizedBootExecutor(SynchronizedBootSequence& sequence)
    : sequence_(sequence) {}

bool SynchronizedBootExecutor::executeBootWithOpening(HeavyTaskFunction heavyTask,
                                                      const BootConfig& config,
                                                      ExecutionResult& outResult) {
    SynchronizedBootSequence::BootConfig bootConfig;
    bootConfig.taskName = config.taskName;
    bootConfig.estimatedDuration = config.estimatedDuration;
    bootConfig.fallbackToFastMode = config.fallbackToFastMode;
    bootConfig.showDetailed = config.showDetailed;

    SynchronizedBootSequence::HeavyTaskFunction task = heavyTask ? heavyTask
        : [](std::function<void(float)> progressCallback) {
              if (progressCallback) {
                  progressCallback(1.0f);
              }
              return true;
          };

    bool success = sequence_.executeBootWithOpening(task, bootConfig);
    auto result = sequence_.getLastResult();
    outResult.taskSuccess = result.taskSuccess;
    outResult.openingSuccess = result.openingSuccess;
    outResult.totalTimeMs = result.totalTimeMs;
    outResult.taskTimeMs = result.taskTimeMs;
    outResult.openingFps = result.openingFps;
    outResult.timeTargetMet = result.timeTargetMet;
    return success;
}

}  // namespace Boot
