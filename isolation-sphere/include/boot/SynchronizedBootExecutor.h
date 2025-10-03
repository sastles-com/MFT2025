#pragma once

#include "boot/ProceduralOpeningPlayer.h"
#include "boot/ProceduralOpeningSequence.h"

namespace Boot {

class SynchronizedBootExecutor : public ProceduralBootExecutor {
public:
    explicit SynchronizedBootExecutor(SynchronizedBootSequence& sequence);

    bool executeBootWithOpening(HeavyTaskFunction heavyTask,
                                const BootConfig& config,
                                ExecutionResult& outResult) override;

private:
    SynchronizedBootSequence& sequence_;
};

}  // namespace Boot
