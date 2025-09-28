#include "audio/BuzzerManager.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <algorithm>

namespace buzzer {
namespace {
constexpr const char *kTag = "BUZZER_MGR";

constexpr std::array<float, static_cast<std::size_t>(Note::kSilence) + 1> kNoteFrequencies = {
    261.63f,  // C4
    277.18f,  // CS4
    293.66f,  // D4
    311.13f,  // DS4
    329.63f,  // E4
    349.23f,  // F4
    369.99f,  // FS4
    392.00f,  // G4
    415.30f,  // GS4
    440.00f,  // A4
    466.16f,  // AS4
    493.88f,  // B4
    523.25f,  // C5
    659.26f,  // E5
    0.0f,     // Silence
};

constexpr std::array<Note, 2> kStartupNotes = {
    Note::kE4,
    Note::kE5,
};

constexpr std::array<uint16_t, kStartupNotes.size()> kStartupDurations = {
    200,
    500,
};

constexpr std::array<Note, 8> kShutdownNotes = {
    Note::kC5,
    Note::kB4,
    Note::kA4,
    Note::kG4,
    Note::kF4,
    Note::kE4,
    Note::kD4,
    Note::kC4,
};

constexpr std::array<uint16_t, kShutdownNotes.size()> kShutdownDurations = {
    400, 400, 400, 400, 400, 400, 400, 600,
};

constexpr float kBeepFrequency = 800.0f;
constexpr float kSuccessFrequency = 1200.0f;
constexpr float kNotificationFrequency = 600.0f;
constexpr uint16_t kBeepDurationMs = 200;
constexpr uint16_t kSuccessDurationMs = 300;
constexpr uint16_t kNotificationDurationMs = 150;
constexpr TickType_t kMutexWaitTicks = pdMS_TO_TICKS(1000);

constexpr std::array<Note, 5> kErrorNotes = {
    Note::kC4,
    Note::kSilence,
    Note::kC4,
    Note::kSilence,
    Note::kC4,
};

constexpr std::array<uint16_t, kErrorNotes.size()> kErrorDurations = {
    100, 100, 100, 100, 100,
};

uint8_t ClampVolume(uint8_t volume) {
  return std::min<uint8_t>(volume, kMaxVolume);
}
}  // namespace

Manager::~Manager() {
  Deinit();
}

Result Manager::Init(gpio_num_t gpio) {
  if (initialized_) {
    return Result::kOk;
  }

  Result validation = ValidateGpio(gpio);
  if (validation != Result::kOk) {
    return validation;
  }

  gpio_ = gpio;
  ledc_channel_ = kLedcChannel;
  ledc_frequency_ = kLedcBaseFrequency;
  ledc_resolution_bits_ = kLedcResolutionBits;
  volume_ = 50;
  muted_ = false;
  playing_ = false;
  stop_requested_ = false;
  current_tone_ = {};
  stats_ = {};

  if (!mutex_) {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
      ESP_LOGE(kTag, "Failed to create buzzer mutex");
      return Result::kMutexFailed;
    }
  }

  Result ledc_result = ConfigureLedc();
  if (ledc_result != Result::kOk) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
    return ledc_result;
  }

  initialized_ = true;
  ESP_LOGI(kTag, "Buzzer manager initialized: GPIO%d, channel %d", static_cast<int>(gpio_), ledc_channel_);
  return Result::kOk;
}

Result Manager::Deinit() {
  if (!initialized_) {
    return Result::kOk;
  }

  Stop();

  if (task_handle_) {
    vTaskDelete(task_handle_);
    task_handle_ = nullptr;
  }

  StopTone();

  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }

  initialized_ = false;
  ESP_LOGI(kTag, "Buzzer manager deinitialized");
  return Result::kOk;
}

Result Manager::PlayEffect(Effect effect_type) {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  switch (effect_type) {
    case Effect::kBeep:
      return PlayToneInternal(kBeepFrequency, kBeepDurationMs, volume_, true);
    case Effect::kSuccess:
      return PlayToneInternal(kSuccessFrequency, kSuccessDurationMs, volume_, true);
    case Effect::kError: {
      Melody error_melody;
      error_melody.notes = kErrorNotes.data();
      error_melody.durations_ms = kErrorDurations.data();
      error_melody.note_count = kErrorNotes.size();
      error_melody.volume = volume_;
      return PlayMelodyInternal(error_melody, true);
    }
    case Effect::kNotification:
      return PlayToneInternal(kNotificationFrequency, kNotificationDurationMs, volume_, true);
    case Effect::kStartup: {
      Melody melody = StartupMelody();
      return PlayMelodyInternal(melody, true);
    }
    case Effect::kShutdown: {
      Melody melody = ShutdownMelody();
      return PlayMelodyInternal(melody, true);
    }
    case Effect::kCustom:
      return Result::kInvalidArgument;
  }

  return Result::kInvalidArgument;
}

Result Manager::PlayNote(Note note, uint16_t duration_ms) {
  float frequency = 0.0f;
  Result freq_result = GetNoteFrequency(note, &frequency);
  if (freq_result != Result::kOk) {
    return freq_result;
  }

  return PlayToneInternal(frequency, duration_ms, volume_, false);
}

Result Manager::PlayTone(float frequency_hz, uint16_t duration_ms) {
  return PlayToneInternal(frequency_hz, duration_ms, volume_, false);
}

Result Manager::PlayMelody(const Melody &melody) {
  return PlayMelodyInternal(melody, false);
}

Result Manager::Stop() {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (!TakeMutex(kMutexWaitTicks)) {
    return Result::kMutexFailed;
  }

  if (playing_) {
    stop_requested_ = true;
    StopTone();
    playing_ = false;
    stats_.is_playing = false;
  }

  GiveMutex();
  return Result::kOk;
}

Result Manager::SetVolume(uint8_t volume) {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  volume_ = ClampVolume(volume);
  return Result::kOk;
}

Result Manager::GetVolume(uint8_t *volume) const {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (!volume) {
    return Result::kInvalidArgument;
  }

  *volume = volume_;
  return Result::kOk;
}

Result Manager::SetMute(bool mute) {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (!TakeMutex(kMutexWaitTicks)) {
    return Result::kMutexFailed;
  }

  muted_ = mute;
  if (muted_ && playing_) {
    StopTone();
  }

  stats_.is_muted = muted_;
  GiveMutex();
  return Result::kOk;
}

Result Manager::IsPlaying(bool *playing) const {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (!playing) {
    return Result::kInvalidArgument;
  }

  if (!TakeMutex(kMutexWaitTicks)) {
    return Result::kMutexFailed;
  }

  *playing = playing_;
  GiveMutex();
  return Result::kOk;
}

Result Manager::GetStats(Stats *stats) const {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (!stats) {
    return Result::kInvalidArgument;
  }

  if (!TakeMutex(kMutexWaitTicks)) {
    return Result::kMutexFailed;
  }

  *stats = stats_;
  stats->current_frequency = current_tone_.frequency_hz;
  stats->current_volume = current_tone_.volume;
  stats->is_playing = playing_;
  stats->is_muted = muted_;

  GiveMutex();
  return Result::kOk;
}

Result Manager::GetNoteFrequency(Note note, float *frequency_hz) {
  if (!frequency_hz) {
    return Result::kInvalidArgument;
  }

  std::size_t index = static_cast<std::size_t>(note);
  if (index >= kNoteFrequencies.size()) {
    return Result::kInvalidArgument;
  }

  *frequency_hz = kNoteFrequencies[index];
  return Result::kOk;
}

Melody Manager::StartupMelody() {
  Melody melody;
  melody.notes = kStartupNotes.data();
  melody.durations_ms = kStartupDurations.data();
  melody.note_count = kStartupNotes.size();
  melody.volume = 70;
  return melody;
}

Melody Manager::ShutdownMelody() {
  Melody melody;
  melody.notes = kShutdownNotes.data();
  melody.durations_ms = kShutdownDurations.data();
  melody.note_count = kShutdownNotes.size();
  melody.volume = 70;
  return melody;
}

const char *Manager::ResultToString(Result result) {
  switch (result) {
    case Result::kOk:
      return "OK";
    case Result::kInvalidArgument:
      return "INVALID_ARG";
    case Result::kGpioConfigFailed:
      return "GPIO_CONFIG_FAILED";
    case Result::kLedcConfigFailed:
      return "LEDC_CONFIG_FAILED";
    case Result::kNotInitialized:
      return "NOT_INITIALIZED";
    case Result::kAlreadyPlaying:
      return "ALREADY_PLAYING";
    case Result::kMutexFailed:
      return "MUTEX_FAILED";
    case Result::kTaskCreateFailed:
      return "TASK_CREATE_FAILED";
  }

  return "UNKNOWN";
}

Result Manager::ConfigureLedc() {
  ledc_timer_config_t timer_config = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = static_cast<ledc_timer_bit_t>(ledc_resolution_bits_),
      .timer_num = LEDC_TIMER_0,
      .freq_hz = ledc_frequency_,
      .clk_cfg = LEDC_AUTO_CLK,
  };

  esp_err_t err = ledc_timer_config(&timer_config);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "LEDC timer config failed: %s", esp_err_to_name(err));
    return Result::kLedcConfigFailed;
  }

  ledc_channel_config_t channel_config = {
      .gpio_num = gpio_,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = static_cast<ledc_channel_t>(ledc_channel_),
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_0,
      .duty = 0,
      .hpoint = 0,
  };

  err = ledc_channel_config(&channel_config);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "LEDC channel config failed: %s", esp_err_to_name(err));
    return Result::kLedcConfigFailed;
  }

  return Result::kOk;
}

Result Manager::ValidateGpio(gpio_num_t gpio) const {
  if (gpio < 0 || gpio >= GPIO_NUM_MAX) {
    return Result::kInvalidArgument;
  }

  if (gpio >= GPIO_NUM_34 && gpio <= GPIO_NUM_38) {
    return Result::kGpioConfigFailed;
  }

  return Result::kOk;
}

Result Manager::StartTone(float frequency_hz, uint8_t volume) {
  if (muted_ || frequency_hz <= 0.0f) {
    return Result::kOk;
  }

  uint32_t freq = static_cast<uint32_t>(frequency_hz);
  esp_err_t err = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "Failed to set frequency: %s", esp_err_to_name(err));
    return Result::kLedcConfigFailed;
  }

  const uint32_t max_duty = (1u << ledc_resolution_bits_) - 1u;
  uint32_t duty = max_duty / 2u;
  duty = (duty * volume) / kMaxVolume;
  duty = std::min(duty, max_duty);

  err = ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(ledc_channel_), duty);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "Failed to set duty: %s", esp_err_to_name(err));
    return Result::kLedcConfigFailed;
  }

  err = ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(ledc_channel_));
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "Failed to update duty: %s", esp_err_to_name(err));
    return Result::kLedcConfigFailed;
  }

  return Result::kOk;
}

Result Manager::StopTone() {
  esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(ledc_channel_), 0);
  if (err != ESP_OK) {
    return Result::kLedcConfigFailed;
  }

  err = ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(ledc_channel_));
  if (err != ESP_OK) {
    return Result::kLedcConfigFailed;
  }

  return Result::kOk;
}

void Manager::UpdateStats(bool is_effect) {
  stats_.total_plays++;
  if (is_effect) {
    stats_.effect_plays++;
  }
  stats_.last_play_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  stats_.current_frequency = current_tone_.frequency_hz;
  stats_.current_volume = current_tone_.volume;
  stats_.is_playing = playing_;
  stats_.is_muted = muted_;
}

Result Manager::PlayToneInternal(float frequency_hz, uint16_t duration_ms, uint8_t volume, bool is_effect) {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (frequency_hz < 0.0f || frequency_hz > 20000.0f) {
    return Result::kInvalidArgument;
  }

  if (duration_ms == 0) {
    return Result::kInvalidArgument;
  }

  if (!TakeMutex(kMutexWaitTicks)) {
    return Result::kMutexFailed;
  }

  if (playing_) {
    GiveMutex();
    return Result::kAlreadyPlaying;
  }

  playing_ = true;
  stop_requested_ = false;

  current_tone_.frequency_hz = frequency_hz;
  current_tone_.duration_ms = duration_ms;
  current_tone_.volume = ClampVolume(volume);

  Result start_result = StartTone(frequency_hz, current_tone_.volume);
  if (start_result != Result::kOk) {
    playing_ = false;
    GiveMutex();
    return start_result;
  }

  vTaskDelay(pdMS_TO_TICKS(duration_ms));
  StopTone();

  playing_ = false;
  UpdateStats(is_effect);

  GiveMutex();
  return Result::kOk;
}

Result Manager::PlayMelodyInternal(const Melody &melody, bool is_effect) {
  if (!initialized_) {
    return Result::kNotInitialized;
  }

  if (!melody.notes || !melody.durations_ms || melody.note_count == 0 || melody.note_count > kMaxMelodyNotes) {
    return Result::kInvalidArgument;
  }

  if (!TakeMutex(kMutexWaitTicks)) {
    return Result::kMutexFailed;
  }

  if (playing_) {
    GiveMutex();
    return Result::kAlreadyPlaying;
  }

  playing_ = true;
  stop_requested_ = false;

  const uint8_t melody_volume = melody.volume == 0 ? volume_ : ClampVolume(melody.volume);

  for (std::size_t i = 0; i < melody.note_count && !stop_requested_; ++i) {
    float frequency = 0.0f;
    Result freq_result = GetNoteFrequency(melody.notes[i], &frequency);
    if (freq_result != Result::kOk) {
      playing_ = false;
      GiveMutex();
      return freq_result;
    }

    current_tone_.frequency_hz = frequency;
    current_tone_.duration_ms = melody.durations_ms[i];
    current_tone_.volume = melody_volume;

    if (frequency > 0.0f) {
      Result start_result = StartTone(frequency, melody_volume);
      if (start_result != Result::kOk) {
        playing_ = false;
        GiveMutex();
        return start_result;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(melody.durations_ms[i]));

    if (frequency > 0.0f) {
      StopTone();
    }

    if (i + 1 < melody.note_count) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  StopTone();
  UpdateStats(is_effect);
  stats_.melody_plays++;

  playing_ = false;
  GiveMutex();

  ESP_LOGI(kTag, "Melody playback completed (%zu notes)", melody.note_count);
  return Result::kOk;
}

bool Manager::TakeMutex(TickType_t timeout_ticks) const {
  if (!mutex_) {
    return false;
  }
  return xSemaphoreTake(mutex_, timeout_ticks) == pdTRUE;
}

void Manager::GiveMutex() const {
  if (mutex_) {
    xSemaphoreGive(mutex_);
  }
}

}  // namespace buzzer
