#pragma once

#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace buzzer {

constexpr gpio_num_t kDefaultGpio = static_cast<gpio_num_t>(8);
constexpr uint8_t kLedcChannel = 1;
constexpr uint32_t kLedcBaseFrequency = 12000;
constexpr uint8_t kLedcResolutionBits = 8;
constexpr uint32_t kDefaultBeatMs = 500;
constexpr uint8_t kMaxVolume = 100;
constexpr std::size_t kMaxMelodyNotes = 32;

enum class Result {
  kOk = 0,
  kInvalidArgument,
  kGpioConfigFailed,
  kLedcConfigFailed,
  kNotInitialized,
  kAlreadyPlaying,
  kMutexFailed,
  kTaskCreateFailed,
};

enum class Effect {
  kBeep,
  kSuccess,
  kError,
  kNotification,
  kStartup,
  kShutdown,
  kCustom,
};

enum class Note {
  kC4 = 0,
  kCs4,
  kD4,
  kDs4,
  kE4,
  kF4,
  kFs4,
  kG4,
  kGs4,
  kA4,
  kAs4,
  kB4,
  kC5,
  kE5,
  kSilence,
};

struct Tone {
  float frequency_hz = 0.0f;
  uint16_t duration_ms = 0;
  uint8_t volume = 0;
};

struct Melody {
  const Note *notes = nullptr;
  const uint16_t *durations_ms = nullptr;
  std::size_t note_count = 0;
  uint8_t volume = kMaxVolume;
};

struct Stats {
  uint32_t total_plays = 0;
  uint32_t effect_plays = 0;
  uint32_t melody_plays = 0;
  uint32_t last_play_time = 0;
  float current_frequency = 0.0f;
  uint8_t current_volume = 0;
  bool is_playing = false;
  bool is_muted = false;
};

class Manager {
 public:
  Manager() = default;
  ~Manager();

  Result Init(gpio_num_t gpio = kDefaultGpio);
  Result Deinit();

  Result PlayEffect(Effect effect_type);
  Result PlayNote(Note note, uint16_t duration_ms);
  Result PlayTone(float frequency_hz, uint16_t duration_ms);
  Result PlayMelody(const Melody &melody);
  Result Stop();

  Result SetVolume(uint8_t volume);
  Result GetVolume(uint8_t *volume) const;

  Result SetMute(bool mute);
  Result IsPlaying(bool *playing) const;
  Result GetStats(Stats *stats) const;

  bool IsInitialized() const { return initialized_; }

  static Result GetNoteFrequency(Note note, float *frequency_hz);
  static Melody StartupMelody();
  static Melody ShutdownMelody();
  static const char *ResultToString(Result result);

 private:
  Result ConfigureLedc();
  Result ValidateGpio(gpio_num_t gpio) const;
  Result StartTone(float frequency_hz, uint8_t volume);
  Result StopTone();
  void UpdateStats(bool is_effect);
  Result PlayToneInternal(float frequency_hz, uint16_t duration_ms, uint8_t volume, bool is_effect);
  Result PlayMelodyInternal(const Melody &melody, bool is_effect);
  bool TakeMutex(TickType_t timeout_ticks) const;
  void GiveMutex() const;

  gpio_num_t gpio_ = kDefaultGpio;
  uint8_t ledc_channel_ = kLedcChannel;
  uint32_t ledc_frequency_ = kLedcBaseFrequency;
  uint8_t ledc_resolution_bits_ = kLedcResolutionBits;

  uint8_t volume_ = 50;
  bool muted_ = false;
  Tone current_tone_{};

  SemaphoreHandle_t mutex_ = nullptr;
  TaskHandle_t task_handle_ = nullptr;
  bool initialized_ = false;
  bool stop_requested_ = false;
  bool playing_ = false;

  Stats stats_{};
};

}  // namespace buzzer

