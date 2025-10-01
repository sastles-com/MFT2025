#pragma once
#include <FastLED.h>
#include <cstdint>
#include <vector>

namespace led {

struct Coord3 {
  float x;
  float y;
  float z;
};

struct PerStripInfo {
  uint8_t index;
  int gpio;
  uint32_t length;
  int offset[3];
};

class StripController {
public:
  StripController();
  ~StripController();

  // Initialize controller. ledsPerStrip must have numStrips entries.
  // Does not register buffers with FastLED; caller should call FastLED.addLeds with getFrontBuffer() pointer.
  bool init(uint8_t numStrips,
            const std::vector<uint32_t>& ledsPerStrip,
            const std::vector<int>& gpios,
            bool usePsram,
            bool doubleBuffer,
            uint8_t maxBrightness = 128);

  // Release allocated buffers
  void deinit();

  // Access render buffer (back buffer) where the renderer should draw.
  // If double buffering disabled, front==back.
  CRGB* getRenderBuffer() { return backBuffer_; }

  // Access front buffer (the one FastLED should be hooked to)
  CRGB* getFrontBuffer() { return frontBuffer_; }

  // Swap front/back buffers (no copy). Safe to call from main loop.
  void swapBuffers();

  // Basic pixel APIs (operate on render/back buffer)
  void setPixel(uint8_t stripIdx, uint32_t idx, const CRGB &c);
  void fillStrip(uint8_t stripIdx, const CRGB &c);
  void fillAll(const CRGB &c);

  // Show is just a wrapper that calls FastLED.show(); caller must ensure FastLED has been configured with frontBuffer_
  void show();

  // Layout / coords: caller can set per-LED coordinates (total length = sum ledsPerStrip)
  void setLedCoords(const std::vector<Coord3> &coords);

  uint32_t totalLeds() const { return totalLeds_; }
  bool isDoubleBuffered() const { return doubleBuffered_; }

private:
  bool allocateBuffers(bool usePsram);
  void freeBuffers();

  uint8_t numStrips_ = 0;
  std::vector<uint32_t> ledsPerStrip_;
  std::vector<uint32_t> stripOffsets_;
  std::vector<int> gpios_;
  std::vector<PerStripInfo> perStripInfo_;

  uint32_t totalLeds_ = 0;
  CRGB *frontBuffer_ = nullptr;
  CRGB *backBuffer_ = nullptr;
  bool allocatedInPsram_ = false;
  bool doubleBuffered_ = false;
  uint8_t maxBrightness_ = 128;

  // Spatial layout: per-LED coordinates in order (strip0 idx0... stripN idxM)
  std::vector<Coord3> ledCoords_;
};

} // namespace led
