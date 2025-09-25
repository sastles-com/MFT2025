#include <unity.h>

#include "audio/BuzzerManager.h"

void test_get_note_frequency_valid() {
  float frequency = 0.0f;
  TEST_ASSERT_EQUAL(buzzer::Result::kOk, buzzer::Manager::GetNoteFrequency(buzzer::Note::kA4, &frequency));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 440.0f, frequency);
}

void test_get_note_frequency_invalid_pointer() {
  TEST_ASSERT_EQUAL(buzzer::Result::kInvalidArgument, buzzer::Manager::GetNoteFrequency(buzzer::Note::kA4, nullptr));
}

void test_startup_melody_structure() {
  auto melody = buzzer::Manager::StartupMelody();
  TEST_ASSERT_NOT_NULL(melody.notes);
  TEST_ASSERT_NOT_NULL(melody.durations_ms);
  TEST_ASSERT_GREATER_THAN_UINT32(0, melody.note_count);
}

void setUp() {}
void tearDown() {}

int runUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(test_get_note_frequency_valid);
  RUN_TEST(test_get_note_frequency_invalid_pointer);
  RUN_TEST(test_startup_melody_structure);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}

