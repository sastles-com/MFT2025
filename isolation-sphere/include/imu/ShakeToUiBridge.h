#pragma once
#include <Arduino.h>
#include "core/SharedState.h"

class ShakeToUiBridge {
public:
  explicit ShakeToUiBridge(SharedState &state, int requiredCount = 3)
    : state_(state), requiredCount_(requiredCount), count_(0) {}

  void onShakeDetected() {
    count_++;
    if (count_ >= requiredCount_) {
      // Toggle UI mode instead of always setting true so each shake sequence
      // flips UI mode: false->true->false->true ...
      bool current = false;
      if (!state_.getUiMode(current)) {
        // If no value present, assume false
        current = false;
      }
      bool next = !current;
      state_.setUiMode(next);
      // Log for easier observation on serial monitor
      Serial.printf("[ShakeToUiBridge] UI mode toggled -> %s\n", next ? "ON" : "OFF");
      count_ = 0;
    }
  }

  void reset() { count_ = 0; }

private:
  SharedState &state_;
  int requiredCount_;
  int count_;
};
