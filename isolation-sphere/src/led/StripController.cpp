#include "led/StripController.h"
#include <cstring>
#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32
#include "esp_heap_caps.h"
#endif

namespace led {

StripController::StripController() {}

StripController::~StripController() { deinit(); }

bool StripController::init(uint8_t numStrips,
                          const std::vector<uint32_t>& ledsPerStrip,
                          const std::vector<int>& gpios,
                          bool usePsram,
                          bool doubleBuffer,
                          uint8_t maxBrightness) {
  deinit();
  numStrips_ = numStrips;
  ledsPerStrip_ = ledsPerStrip;
  gpios_ = gpios;
  doubleBuffered_ = doubleBuffer;
  maxBrightness_ = maxBrightness;

  // compute offsets and total
  totalLeds_ = 0;
  stripOffsets_.resize(numStrips_);
  for (uint8_t i = 0; i < numStrips_; ++i) {
    stripOffsets_[i] = totalLeds_;
    totalLeds_ += ledsPerStrip_[i];
  }

  // per-strip info
  perStripInfo_.clear();
  for (uint8_t i = 0; i < numStrips_; ++i) {
    PerStripInfo ps;
    ps.index = i;
    ps.gpio = (i < gpios_.size()) ? gpios_[i] : -1;
    ps.length = ledsPerStrip_[i];
    ps.offset[0] = ps.offset[1] = ps.offset[2] = 0;
    perStripInfo_.push_back(ps);
  }

  // allocate buffers
  bool ok = allocateBuffers(usePsram);
  if (!ok) {
    Serial.println("[StripController] buffer allocation failed");
    freeBuffers();
    return false;
  }

  return true;
}

void StripController::deinit() { freeBuffers(); }

bool StripController::allocateBuffers(bool usePsram) {
  size_t bytes = (size_t)totalLeds_ * sizeof(CRGB);
  if (bytes == 0) return false;

  allocatedInPsram_ = false;
  frontBuffer_ = nullptr;
  backBuffer_ = nullptr;

#ifdef ARDUINO_ARCH_ESP32
  if (usePsram) {
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (p) {
      frontBuffer_ = reinterpret_cast<CRGB*>(p);
      allocatedInPsram_ = true;
    }
  }
#endif

  if (!frontBuffer_) {
    // fallback to heap
    frontBuffer_ = reinterpret_cast<CRGB*>(malloc(bytes));
    if (!frontBuffer_) return false;
  }

  if (doubleBuffered_) {
#ifdef ARDUINO_ARCH_ESP32
    if (usePsram && !allocatedInPsram_) {
      // try allocating second buffer in PSRAM
      void *p2 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
      if (p2) {
        backBuffer_ = reinterpret_cast<CRGB*>(p2);
      }
    }
#endif
    if (!backBuffer_) {
      backBuffer_ = reinterpret_cast<CRGB*>(malloc(bytes));
      if (!backBuffer_) {
        free(frontBuffer_);
        frontBuffer_ = nullptr;
        return false;
      }
    }
  } else {
    backBuffer_ = frontBuffer_; // single buffer mode
  }

  // clear buffers
  memset(frontBuffer_, 0, bytes);
  if (backBuffer_ != frontBuffer_) memset(backBuffer_, 0, bytes);

  return true;
}

void StripController::freeBuffers() {
  size_t bytes = (size_t)totalLeds_ * sizeof(CRGB);
  if (frontBuffer_) {
#ifdef ARDUINO_ARCH_ESP32
    if (allocatedInPsram_) {
      heap_caps_free(frontBuffer_);
    } else {
      free(frontBuffer_);
    }
#else
    free(frontBuffer_);
#endif
    frontBuffer_ = nullptr;
  }
  if (backBuffer_ && backBuffer_ != frontBuffer_) {
#ifdef ARDUINO_ARCH_ESP32
    // we don't track which buffer is in PSRAM for the backBuffer separately; free safely
    free(backBuffer_);
#else
    free(backBuffer_);
#endif
    backBuffer_ = nullptr;
  }
  totalLeds_ = 0;
}

void StripController::swapBuffers() {
  if (!doubleBuffered_) return;
  CRGB* tmp = frontBuffer_;
  frontBuffer_ = backBuffer_;
  backBuffer_ = tmp;
}

void StripController::setPixel(uint8_t stripIdx, uint32_t idx, const CRGB &c) {
  if (!backBuffer_) return;
  if (stripIdx >= numStrips_) return;
  if (idx >= ledsPerStrip_[stripIdx]) return;
  uint32_t pos = stripOffsets_[stripIdx] + idx;
  backBuffer_[pos] = c;
}

void StripController::fillStrip(uint8_t stripIdx, const CRGB &c) {
  if (!backBuffer_) return;
  if (stripIdx >= numStrips_) return;
  uint32_t start = stripOffsets_[stripIdx];
  uint32_t len = ledsPerStrip_[stripIdx];
  for (uint32_t i = 0; i < len; ++i) backBuffer_[start + i] = c;
}

void StripController::fillAll(const CRGB &c) {
  if (!backBuffer_) return;
  for (uint32_t i = 0; i < totalLeds_; ++i) backBuffer_[i] = c;
}

void StripController::show() {
  // FastLED.show() must be called by caller after hooking FastLED to frontBuffer_
  FastLED.show();
}

void StripController::setLedCoords(const std::vector<Coord3> &coords) {
  ledCoords_ = coords;
}

} // namespace led
