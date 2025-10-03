#pragma once

#include <functional>

namespace Boot {

class ProceduralBootExecutor {
public:
    using HeavyTaskFunction = std::function<bool(std::function<void(float)>)>;

    struct BootConfig {
        const char* taskName = "Procedural Opening";
        float estimatedDuration = 3.0f;
        bool fallbackToFastMode = true;
        bool showDetailed = true;
    };

    struct ExecutionResult {
        bool taskSuccess = false;
        bool openingSuccess = false;
        uint32_t totalTimeMs = 0;
        uint32_t taskTimeMs = 0;
        float openingFps = 0.0f;
        bool timeTargetMet = false;
    };

    virtual ~ProceduralBootExecutor() = default;
    virtual bool executeBootWithOpening(HeavyTaskFunction heavyTask,
                                        const BootConfig& config,
                                        ExecutionResult& outResult) = 0;
};

class ProceduralOpeningPlayer {
public:
    explicit ProceduralOpeningPlayer(ProceduralBootExecutor& executor);

    bool playStandardOpening(ProceduralBootExecutor::HeavyTaskFunction heavyTask);
    bool playStandardOpening();

    ProceduralBootExecutor::ExecutionResult lastExecution() const;

private:
    ProceduralBootExecutor& executor_;
    ProceduralBootExecutor::ExecutionResult lastResult_{};
};

} // namespace Boot
