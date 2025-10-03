#include "boot/ProceduralOpeningPlayer.h"

#include <algorithm>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#if !defined(UNIT_TEST) && !defined(ARDUINO)
#include <chrono>
#include <thread>
#endif

namespace Boot {

namespace {

constexpr float kDefaultProgressStepSeconds = 0.1f;

ProceduralBootExecutor::HeavyTaskFunction makeDefaultHeavyTask(float estimatedDurationSeconds) {
    return [estimatedDurationSeconds](std::function<void(float)> progressCallback) -> bool {
        if (!progressCallback) {
            return false;
        }

        const float duration = estimatedDurationSeconds > 0.0f ? estimatedDurationSeconds : 3.0f;
        const int steps = std::max(1, static_cast<int>(duration / kDefaultProgressStepSeconds));
        const float progressStep = 1.0f / static_cast<float>(steps);

        for (int i = 0; i < steps; ++i) {
            float progress = std::min(1.0f, progressStep * static_cast<float>(i + 1));
            progressCallback(progress);
#if !defined(UNIT_TEST)
#if defined(ARDUINO)
            const uint32_t delayMs = static_cast<uint32_t>((duration * 1000.0f) / steps);
            delay(delayMs);
#else
            // Host実行環境では最小限のスリープでCPU負荷を抑える
            using namespace std::chrono;
            const int delayMs = static_cast<int>((duration * 1000.0f) / steps);
            if (delayMs > 0) {
                std::this_thread::sleep_for(milliseconds(delayMs));
            }
#endif
#endif
        }
        return true;
    };
}

}  // namespace

ProceduralOpeningPlayer::ProceduralOpeningPlayer(ProceduralBootExecutor& executor)
    : executor_(executor) {}

bool ProceduralOpeningPlayer::playStandardOpening(ProceduralBootExecutor::HeavyTaskFunction heavyTask) {
    ProceduralBootExecutor::BootConfig config;
    ProceduralBootExecutor::HeavyTaskFunction task = heavyTask;
    if (!task) {
        task = makeDefaultHeavyTask(config.estimatedDuration);
    }

    lastResult_ = {};
    return executor_.executeBootWithOpening(task, config, lastResult_);
}

bool ProceduralOpeningPlayer::playStandardOpening() {
    return playStandardOpening(nullptr);
}

ProceduralBootExecutor::ExecutionResult ProceduralOpeningPlayer::lastExecution() const {
    return lastResult_;
}

} // namespace Boot
