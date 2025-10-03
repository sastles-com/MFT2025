#include <unity.h>

#include <vector>

#include "boot/ProceduralOpeningPlayer.h"
#include "../../src/boot/ProceduralOpeningPlayer.cpp"

namespace {

class FakeExecutor : public Boot::ProceduralBootExecutor {
public:
    bool executeBootWithOpening(HeavyTaskFunction heavyTask,
                                const BootConfig& config,
                                ExecutionResult& outResult) override {
        called = true;
        lastConfig = config;
        lastHeavyTask = heavyTask;
        if (invokeHeavyTask && heavyTask) {
            heavyTaskCalled = true;
            bool result = heavyTask([&](float progress) {
                progressHistory.push_back(progress);
            });
            lastHeavyTaskResult = result;
        }
        outResult = syntheticResult;
        return returnValue;
    }

    bool called = false;
    bool invokeHeavyTask = false;
    bool heavyTaskCalled = false;
    bool returnValue = true;
    bool lastHeavyTaskResult = false;
    BootConfig lastConfig{};
    HeavyTaskFunction lastHeavyTask{};
    std::vector<float> progressHistory;
    ExecutionResult syntheticResult{};
};

} // namespace

void test_play_standard_opening_passes_expected_config() {
    FakeExecutor executor;
    Boot::ProceduralOpeningPlayer player(executor);

    auto task = [](std::function<void(float)> progressCallback) {
        progressCallback(0.25f);
        progressCallback(0.75f);
        return true;
    };

    TEST_ASSERT_FALSE(executor.called);

    bool result = player.playStandardOpening(task);

    TEST_ASSERT_TRUE(executor.called);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(static_cast<bool>(executor.lastHeavyTask));
    TEST_ASSERT_EQUAL_STRING("Procedural Opening", executor.lastConfig.taskName);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, executor.lastConfig.estimatedDuration);
    TEST_ASSERT_TRUE(executor.lastConfig.showDetailed);
    TEST_ASSERT_TRUE(executor.lastConfig.fallbackToFastMode);
}

void test_play_standard_opening_invokes_provided_heavy_task() {
    FakeExecutor executor;
    executor.invokeHeavyTask = true;
    Boot::ProceduralOpeningPlayer player(executor);

    bool heavyTaskWasInvoked = false;
    auto task = [&](std::function<void(float)> progressCallback) {
        heavyTaskWasInvoked = true;
        progressCallback(0.4f);
        progressCallback(0.9f);
        return true;
    };

    bool result = player.playStandardOpening(task);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(executor.heavyTaskCalled);
    TEST_ASSERT_TRUE(heavyTaskWasInvoked);
    TEST_ASSERT_EQUAL_UINT32(2u, static_cast<unsigned>(executor.progressHistory.size()));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4f, executor.progressHistory[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.9f, executor.progressHistory[1]);
}

void test_play_standard_opening_returns_executor_result() {
    FakeExecutor executor;
    Boot::ProceduralOpeningPlayer player(executor);

    executor.returnValue = false;
    auto failingTask = [](std::function<void(float)>) {
        return false;
    };

    TEST_ASSERT_FALSE(player.playStandardOpening(failingTask));

    executor.returnValue = true;
    TEST_ASSERT_TRUE(player.playStandardOpening(failingTask));
}

void test_last_execution_reflects_executor_output() {
    FakeExecutor executor;
    Boot::ProceduralOpeningPlayer player(executor);

    executor.syntheticResult.openingSuccess = true;
    executor.syntheticResult.taskSuccess = true;
    executor.syntheticResult.totalTimeMs = 1234;

    auto task = [](std::function<void(float)> progressCallback) {
        progressCallback(0.5f);
        return true;
    };

    TEST_ASSERT_TRUE(player.playStandardOpening(task));

    auto result = player.lastExecution();
    TEST_ASSERT_TRUE(result.taskSuccess);
    TEST_ASSERT_TRUE(result.openingSuccess);
    TEST_ASSERT_EQUAL_UINT32(1234, result.totalTimeMs);
}

void test_default_heavy_task_reaches_completion() {
    FakeExecutor executor;
    executor.invokeHeavyTask = true;
    Boot::ProceduralOpeningPlayer player(executor);

    bool result = player.playStandardOpening();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(executor.heavyTaskCalled);
    TEST_ASSERT_NOT_EQUAL(0, executor.progressHistory.size());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, executor.progressHistory.back());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_play_standard_opening_passes_expected_config);
    RUN_TEST(test_play_standard_opening_invokes_provided_heavy_task);
    RUN_TEST(test_play_standard_opening_returns_executor_result);
    RUN_TEST(test_last_execution_reflects_executor_output);
    RUN_TEST(test_default_heavy_task_reaches_completion);
    return UNITY_END();
}
