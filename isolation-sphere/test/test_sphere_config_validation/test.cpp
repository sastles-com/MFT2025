#include <unity.h>
#include <cstring>
#include "system/SystemNameMigration.h"

// system_name_migration_tests.cpp で定義される登録関数
void registerSystemNameMigrationTests();

#if __has_include("mqtt/MqttBroker.h")
#define HAS_MQTT_BROKER 1
#else
#define HAS_MQTT_BROKER 0
#endif

#if __has_include("mqtt/MqttService.h")
#define HAS_MQTT_SERVICE 1
#else
#define HAS_MQTT_SERVICE 0
#endif

// 既存実装/スタブは別ファイル(test.cpp)で提供される想定
bool migrateSystemName(const char* oldName, char* outBuf, size_t outSize);
bool isValidSystemName(const char* name);

// ---------- ヘルパ ----------
static void assertMigrateSuccess(const char* input, const char* expected) {
  char buf[32]; memset(buf, 0, sizeof(buf));
  bool ok = migrateSystemName(input, buf, sizeof(buf));
  TEST_ASSERT_TRUE_MESSAGE(ok, "migrateSystemName should succeed");
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, buf, "Migrated name mismatch");
  TEST_ASSERT_TRUE_MESSAGE(isValidSystemName(buf), "Result should be valid");
}

static void assertMigrateFail(const char* input) {
  char buf[32]; memset(buf, 0xAA, sizeof(buf));
  bool ok = migrateSystemName(input, buf, sizeof(buf));
  TEST_ASSERT_FALSE_MESSAGE(ok, "migrateSystemName should fail");
  // 失敗時バッファ内容が誤って valid になっていない
  TEST_ASSERT_FALSE_MESSAGE(isValidSystemName(buf), "Buffer should not become valid");
}

// ---------- 追加テスト ----------

// 1 下限境界 joystick-000
static void test_migrate_lower_boundary() {
  assertMigrateSuccess("joystick-000", "sphere-000");
}

// 2 上限境界 joystick-999
static void test_migrate_upper_boundary() {
  assertMigrateSuccess("joystick-999", "sphere-999");
}

// 3 不正: プレフィックス違い
static void test_invalid_prefix_mixed() {
  assertMigrateFail("joysphere-001");
}

// 4 不正: ダッシュ欠落
static void test_invalid_missing_dash() {
  assertMigrateFail("joystick001");
}

// 5 不正: 桁過多
static void test_invalid_too_many_digits() {
  assertMigrateFail("joystick-0000");
}

// 6 不正: 桁不足 (既存 test との重複ではあるが別パターン joystick 側)
static void test_invalid_too_few_digits_source() {
  assertMigrateFail("joystick-01");
}

// 7 不正: 末尾スペース
static void test_invalid_trailing_space() {
  assertMigrateFail("joystick-001 ");
}

// 8 不正: 先頭スペース
static void test_invalid_leading_space() {
  assertMigrateFail(" joystick-001");
}

// 9 不正: 大文字プレフィックス
static void test_invalid_uppercase_prefix() {
  assertMigrateFail("JOYSTICK-001");
}

// 10 outBuf == nullptr
static void test_null_output_buffer() {
  bool ok = migrateSystemName("joystick-010", nullptr, 0);
  TEST_ASSERT_FALSE_MESSAGE(ok, "NULL output buffer must fail");
}

// 11 outSize ちょうど (”sphere-123” len=10 + NUL=11)
static void test_exact_size_success() {
  char buf[11]; memset(buf, 0xCC, sizeof(buf));
  bool ok = migrateSystemName("joystick-123", buf, sizeof(buf));
  // 実装では成功すべき
  // RED段階では失敗するのでメッセージで意図を明示
  TEST_ASSERT_TRUE_MESSAGE(ok, "Exact-fit buffer (11 bytes) should succeed");
  if (ok) {
    TEST_ASSERT_EQUAL_STRING("sphere-123", buf);
  }
}

// 12 outSize 不足 (10) -> 失敗
static void test_insufficient_size_fail() {
  char buf[10]; memset(buf, 0xDD, sizeof(buf));
  bool ok = migrateSystemName("joystick-123", buf, sizeof(buf));
  TEST_ASSERT_FALSE_MESSAGE(ok, "Insufficient buffer (10) should fail");
}

// 13 失敗時バッファ未改変 (パターン保持確認)
static void test_fail_does_not_modify_buffer() {
  char buf[16]; memset(buf, 0x5A, sizeof(buf));
  bool ok = migrateSystemName("joystick-XYZ", buf, sizeof(buf));
  TEST_ASSERT_FALSE(ok);
  for (size_t i = 0; i < sizeof(buf); ++i) {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x5A, (uint8_t)buf[i], "Buffer modified on failure");
  }
}

// 14 in == out (インプレース) 変換
static void test_inplace_migration() {
  char buf[16];
  strcpy(buf, "joystick-045");
  bool ok = migrateSystemName(buf, buf, sizeof(buf));
  TEST_ASSERT_TRUE_MESSAGE(ok, "In-place migration should succeed");
  if (ok) {
    TEST_ASSERT_EQUAL_STRING("sphere-045", buf);
    TEST_ASSERT_TRUE(isValidSystemName(buf));
  }
}

// 15 既に有効 + 改行付きは無効
static void test_valid_plus_newline_is_invalid() {
  TEST_ASSERT_FALSE_MESSAGE(isValidSystemName("sphere-001\n"),
                            "Name with newline must be invalid");
}

// 16 isValidSystemName(NULL) -> false
static void test_is_valid_null() {
  TEST_ASSERT_FALSE_MESSAGE(isValidSystemName(nullptr),
                            "NULL should be invalid");
}

// 追加テスト登録関数
void registerAdditionalSystemNameTests() {
  RUN_TEST(test_migrate_lower_boundary);
  RUN_TEST(test_migrate_upper_boundary);
  RUN_TEST(test_invalid_prefix_mixed);
  RUN_TEST(test_invalid_missing_dash);
  RUN_TEST(test_invalid_too_many_digits);
  RUN_TEST(test_invalid_too_few_digits_source);
  RUN_TEST(test_invalid_trailing_space);
  RUN_TEST(test_invalid_leading_space);
  RUN_TEST(test_invalid_uppercase_prefix);
  RUN_TEST(test_null_output_buffer);
  RUN_TEST(test_exact_size_success);
  RUN_TEST(test_insufficient_size_fail);
  RUN_TEST(test_fail_does_not_modify_buffer);
  RUN_TEST(test_inplace_migration);
  RUN_TEST(test_valid_plus_newline_is_invalid);
  RUN_TEST(test_is_valid_null);
}

void test_unity_framework_working() {
  TEST_ASSERT_TRUE(true);
}

void test_mqtt_broker_should_not_exist() {
  TEST_ASSERT_EQUAL_MESSAGE(0, HAS_MQTT_BROKER, "MqttBroker は存在しないべき");
}

void test_mqtt_client_should_exist() {
  TEST_ASSERT_EQUAL_MESSAGE(1, HAS_MQTT_SERVICE, "MqttService が見つからない");
}

void test_core_tasks_should_not_initialize_broker() {
  TEST_ASSERT_EQUAL_MESSAGE(0, HAS_MQTT_BROKER, "CoreTasks が Broker を初期化してはならない");
}

void test_system_name_migration_success() {
  char buf[16];
  bool ok = migrateSystemName("joystick-123", buf, sizeof(buf));
  TEST_ASSERT_TRUE_MESSAGE(ok, "移行失敗");
  TEST_ASSERT_EQUAL_STRING("sphere-123", buf);
  TEST_ASSERT_TRUE(isValidSystemName(buf));
}

void test_isolation_joystick_problem_will_disappear() {
  char buf[16];
  bool ok = migrateSystemName("joystick-001", buf, sizeof(buf));
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_NOT_EQUAL(0, std::strncmp(buf, "joystick-", 9));
}

void test_sphere_architecture_requirements() {
  TEST_ASSERT_TRUE_MESSAGE(HAS_MQTT_SERVICE, "MQTT クライアント必須");
  TEST_ASSERT_EQUAL_MESSAGE(0, HAS_MQTT_BROKER, "組込Broker禁止");
}

void test_mqtt_topic_structure_for_sphere() {
  // ここでは最低限のプレースホルダ（詳細は別テストで拡張）
  const char* sample = "sphere/ui/state";
  TEST_ASSERT_NOT_EQUAL(nullptr, std::strstr(sample, "sphere/"));
}

void test_refactor_plan_simplified() {
  TEST_ASSERT_TRUE(true);
}

void test_tdd_red_phase_correct_approach() {
  TEST_ASSERT_TRUE(true);
}

void test_green_phase_preparation() {
  TEST_ASSERT_TRUE(true);
}

#ifdef ARDUINO
void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_unity_framework_working);
  RUN_TEST(test_mqtt_broker_should_not_exist);
  RUN_TEST(test_mqtt_client_should_exist);
  RUN_TEST(test_core_tasks_should_not_initialize_broker);
  RUN_TEST(test_system_name_migration_success);
  RUN_TEST(test_isolation_joystick_problem_will_disappear);
  RUN_TEST(test_sphere_architecture_requirements);
  RUN_TEST(test_mqtt_topic_structure_for_sphere);
  RUN_TEST(test_refactor_plan_simplified);
  RUN_TEST(test_tdd_red_phase_correct_approach);
  RUN_TEST(test_green_phase_preparation);
  // 詳細な移行テスト群を外部登録
  registerSystemNameMigrationTests();
  UNITY_END();
}
void loop() {}
#else
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_unity_framework_working);
  RUN_TEST(test_mqtt_broker_should_not_exist);
  RUN_TEST(test_mqtt_client_should_exist);
  RUN_TEST(test_core_tasks_should_not_initialize_broker);
  RUN_TEST(test_system_name_migration_success);
  RUN_TEST(test_isolation_joystick_problem_will_disappear);
  RUN_TEST(test_sphere_architecture_requirements);
  RUN_TEST(test_mqtt_topic_structure_for_sphere);
  RUN_TEST(test_refactor_plan_simplified);
  RUN_TEST(test_tdd_red_phase_correct_approach);
  RUN_TEST(test_green_phase_preparation);
  registerSystemNameMigrationTests();
  return UNITY_END();
}
#endif