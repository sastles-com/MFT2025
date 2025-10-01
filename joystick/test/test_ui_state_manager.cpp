#include <unity.h>
#include "core/UIStateManager.h"

class UIStateManagerTest {
public:
    void test_initial_state();
    void test_mode_switch();
    void test_function_selection();
    void test_value_update();
    void test_confirm_action();
};

UIStateManagerTest uiTest;

void test_initial_state() { uiTest.test_initial_state(); }
void test_mode_switch() { uiTest.test_mode_switch(); }
void test_function_selection() { uiTest.test_function_selection(); }
void test_value_update() { uiTest.test_value_update(); }
void test_confirm_action() { uiTest.test_confirm_action(); }

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_mode_switch);
    RUN_TEST(test_function_selection);
    RUN_TEST(test_value_update);
    RUN_TEST(test_confirm_action);
    UNITY_END();
    return 0;
}

// --- テストメソッド定義 ---
void UIStateManagerTest::test_initial_state() {
    UIStateManager ui;
    TEST_ASSERT_EQUAL((int)UIMode::Live, (int)ui.getMode());
    TEST_ASSERT_EQUAL(8, ui.getFunctionCount());
    TEST_ASSERT_EQUAL(0, ui.getSelectedFunctionIndex());
    const UIFunctionState& f = ui.getSelectedFunction();
    TEST_ASSERT_EQUAL(0, f.index);
    TEST_ASSERT_EQUAL_STRING("func0", f.name.c_str());
}
void UIStateManagerTest::test_mode_switch() {
    UIStateManager ui;
    ui.setMode(UIMode::Live);
    ui.nextMode();
    TEST_ASSERT_EQUAL((int)UIMode::Control, (int)ui.getMode());
    ui.nextMode();
    TEST_ASSERT_EQUAL((int)UIMode::Video, (int)ui.getMode());
    ui.nextMode();
    TEST_ASSERT_EQUAL((int)UIMode::Maintenance, (int)ui.getMode());
    ui.nextMode();
    TEST_ASSERT_EQUAL((int)UIMode::System, (int)ui.getMode());
    ui.nextMode();
    TEST_ASSERT_EQUAL((int)UIMode::Live, (int)ui.getMode());
}
void UIStateManagerTest::test_function_selection() {
    UIStateManager ui;
    ui.selectFunction(3);
    TEST_ASSERT_EQUAL(3, ui.getSelectedFunctionIndex());
    const UIFunctionState& f = ui.getSelectedFunction();
    TEST_ASSERT_EQUAL(3, f.index);
    TEST_ASSERT_EQUAL_STRING("func3", f.name.c_str());
}
void UIStateManagerTest::test_value_update() {
    UIStateManager ui;
    ui.selectFunction(2);
    ui.updateAnalogValue(42.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 42.5f, ui.getSelectedFunction().analogValue);
    ui.updateDiscreteValue(5);
    TEST_ASSERT_EQUAL(5, ui.getSelectedFunction().discreteIndex);
    ui.updateBooleanValue(true);
    TEST_ASSERT_TRUE(ui.getSelectedFunction().boolValue);
}
void UIStateManagerTest::test_confirm_action() {
    UIStateManager ui;
    // confirmSelection()は現状ダミーなので呼び出しのみ
    ui.confirmSelection();
    TEST_PASS();
}
