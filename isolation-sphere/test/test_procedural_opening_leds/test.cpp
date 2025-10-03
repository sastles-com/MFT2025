#include <unity.h>

#include <vector>
#include <string>
#include <algorithm>

#include "led/LEDSphereManager.h"
#include "boot/ProceduralOpeningSequence.h"
#include "../../src/led/LEDSphereManager.cpp"
#include "../../src/boot/ProceduralOpeningSequence.cpp"

using LEDSphere::LEDSphereManager;
using ProceduralOpeningSequence = ::ProceduralOpeningSequence;

namespace {

LEDSphereManager makeManager() {
  LEDSphereManager manager;
  std::vector<uint16_t> lengths{20};
  std::vector<uint8_t> pins{5};
  TEST_ASSERT_TRUE(manager.initializeLedHardware(static_cast<uint8_t>(lengths.size()), lengths, pins));
  manager.resetOperationLogForTest();
  manager.resetShowFlagForTest();
  return manager;
}

bool containsPrefix(const std::vector<std::string> &ops, const char *prefix) {
  return std::any_of(ops.begin(), ops.end(), [&](const std::string &op) {
    return op.rfind(prefix, 0) == 0; // starts with
  });
}

std::string joinOps(const std::vector<std::string> &ops) {
  std::string joined;
  for (size_t i = 0; i < ops.size(); ++i) {
    if (i) joined += ",";
    joined += ops[i];
  }
  return joined;
}

void assertPhaseInvokesLines(ProceduralOpeningSequence::SequencePhase phase,
                             bool expectLat,
                             bool expectLon) {
  auto manager = makeManager();
  ProceduralOpeningSequence::renderPhaseForTest(phase, 0.5f, 100.0f, manager);
  const auto &ops = manager.operationsForTest();
  TEST_ASSERT_FALSE_MESSAGE(ops.empty(), "operation log should not be empty");
  TEST_ASSERT_EQUAL_STRING("clear", ops.front().c_str());
  auto joined = joinOps(ops);
  TEST_ASSERT_TRUE_MESSAGE(containsPrefix(ops, "show"), joined.c_str());
  if (expectLat) {
    TEST_ASSERT_TRUE_MESSAGE(containsPrefix(ops, "lat"), joined.c_str());
  }
  if (expectLon) {
    TEST_ASSERT_TRUE_MESSAGE(containsPrefix(ops, "lon"), joined.c_str());
  }
}

}  // namespace

void test_boot_splash_draws_lat_lon() {
  assertPhaseInvokesLines(ProceduralOpeningSequence::SequencePhase::PHASE_BOOT_SPLASH, true, true);
}

void test_system_check_draws_lat_lon() {
  assertPhaseInvokesLines(ProceduralOpeningSequence::SequencePhase::PHASE_SYSTEM_CHECK, true, true);
}

void test_sphere_emerge_draws_lat_lon() {
  assertPhaseInvokesLines(ProceduralOpeningSequence::SequencePhase::PHASE_SPHERE_EMERGE, true, true);
}

void test_axis_calibrate_draws_lat_lon() {
  assertPhaseInvokesLines(ProceduralOpeningSequence::SequencePhase::PHASE_AXIS_CALIBRATE, false, true);
}

void test_ready_pulse_draws_lat_lon() {
  assertPhaseInvokesLines(ProceduralOpeningSequence::SequencePhase::PHASE_READY_PULSE, true, false);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_boot_splash_draws_lat_lon);
  RUN_TEST(test_system_check_draws_lat_lon);
  RUN_TEST(test_sphere_emerge_draws_lat_lon);
  RUN_TEST(test_axis_calibrate_draws_lat_lon);
  RUN_TEST(test_ready_pulse_draws_lat_lon);
  return UNITY_END();
}
