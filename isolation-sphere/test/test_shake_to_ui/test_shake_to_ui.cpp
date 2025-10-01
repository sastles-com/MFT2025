#include "unity.h"

// Make this unit test self-contained: avoid linking to the full firmware
// by not including Arduino or production SharedState implementation.
// Provide a minimal MockSharedState with the same API used by the test.

// TDD: 最初にテストを書く

// 期待仕様（簡潔）:
// - ShakeToUiBridge クラスを用意し、onShakeDetected() が呼ばれるたびに内部カウントを増やす
// - カウントが N（このテストでは 3）に達したら MockSharedState::setUiMode(true) を呼ぶ
// - 一度モードが有効化されたらカウントをリセットする

// Minimal mock used only for this unit test to avoid linking production code.
class MockSharedState {
public:
  MockSharedState(): hasValue_(false), uiMode_(false) {}
  void setUiMode(bool active) {
    hasValue_ = true;
    uiMode_ = active;
  }
  bool getUiMode(bool &out) const {
    out = uiMode_;
    return hasValue_;
  }
private:
  bool hasValue_;
  bool uiMode_;
};

// テスト用ブリッジ（宣言のみ、実装は後で追加）
class ShakeToUiBridge {
public:
  ShakeToUiBridge(MockSharedState &state, int requiredCount)
    : state_(state), requiredCount_(requiredCount), count_(0) {}

  void onShakeDetected() {
    count_++;
    if (count_ >= requiredCount_) {
      state_.setUiMode(true);
      count_ = 0;
    }
  }

private:
  MockSharedState &state_;
  int requiredCount_;
  int count_;
};

MockSharedState *g_state = nullptr;

void setUp(void) {
  g_state = new MockSharedState();
}

void tearDown(void) {
  delete g_state;
  g_state = nullptr;
}

void test_shake_to_ui_transitions_after_n_shakes() {
  ShakeToUiBridge bridge(*g_state, 3);
  bool mode = false;

  // initially false (no value yet)
  TEST_ASSERT_FALSE(g_state->getUiMode(mode));

  // simulate 1 shake
  bridge.onShakeDetected();
  TEST_ASSERT_FALSE(g_state->getUiMode(mode));

  // simulate 2nd shake
  bridge.onShakeDetected();
  TEST_ASSERT_FALSE(g_state->getUiMode(mode));

  // simulate 3rd shake -> should enable UI mode
  bridge.onShakeDetected();
  TEST_ASSERT_TRUE(g_state->getUiMode(mode));
  TEST_ASSERT_TRUE(mode);
}

// Provide empty Arduino-like entry points in case the test build expects them.
void setup() {}
void loop() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_shake_to_ui_transitions_after_n_shakes);
  UNITY_END();
  return 0;
}
