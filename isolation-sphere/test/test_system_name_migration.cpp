#include <unity.h>
#include <cstring>
#include "system/SystemNameMigration.h"

static void assertMigrateSuccess(const char* in, const char* expected) {
  char buf[32]; std::memset(buf, 0, sizeof(buf));
  bool ok = migrateSystemName(in, buf, sizeof(buf));
  TEST_ASSERT_TRUE_MESSAGE(ok, "migrateSystemName 失敗");
  TEST_ASSERT_EQUAL_STRING(expected, buf);
  TEST_ASSERT_TRUE(isValidSystemName(buf));
}

static void assertMigrateFail(const char* in) {
  char buf[32]; std::memset(buf, 0xAA, sizeof(buf));
  bool ok = migrateSystemName(in, buf, sizeof(buf));
  TEST_ASSERT_FALSE(ok);
}

void test_migrate_lower_boundary() { assertMigrateSuccess("joystick-000", "sphere-000"); }
void test_migrate_upper_boundary() { assertMigrateSuccess("joystick-999", "sphere-999"); }
void test_inplace() {
  char buf[16]; std::strcpy(buf, "joystick-045");
  bool ok = migrateSystemName(buf, buf, sizeof(buf));
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("sphere-045", buf);
  TEST_ASSERT_TRUE(isValidSystemName(buf));
}
void test_exact_fit() {
  char buf[11];
  bool ok = migrateSystemName("joystick-123", buf, sizeof(buf));
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("sphere-123", buf);
}
void test_insufficient() {
  char buf[10];
  bool ok = migrateSystemName("joystick-123", buf, sizeof(buf));
  TEST_ASSERT_FALSE(ok);
}
void test_already_valid() {
  char buf[16];
  bool ok = migrateSystemName("sphere-010", buf, sizeof(buf));
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("sphere-010", buf);
}
void test_invalid_prefix() { assertMigrateFail("joysphere-001"); }
void test_missing_dash() { assertMigrateFail("joystick001"); }
void test_too_many_digits() { assertMigrateFail("joystick-0000"); }
void test_too_few_digits() { assertMigrateFail("joystick-01"); }
void test_trailing_space() { assertMigrateFail("joystick-001 "); }
void test_leading_space() { assertMigrateFail(" joystick-001"); }
void test_uppercase_prefix() { assertMigrateFail("JOYSTICK-001"); }
void test_newline_invalid() { TEST_ASSERT_FALSE(isValidSystemName("sphere-001\n")); }
void test_null_name_invalid() { TEST_ASSERT_FALSE(isValidSystemName(nullptr)); }

#ifdef ARDUINO
void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_migrate_lower_boundary);
  RUN_TEST(test_migrate_upper_boundary);
  RUN_TEST(test_inplace);
  RUN_TEST(test_exact_fit);
  RUN_TEST(test_insufficient);
  RUN_TEST(test_already_valid);
  RUN_TEST(test_invalid_prefix);
  RUN_TEST(test_missing_dash);
  RUN_TEST(test_too_many_digits);
  RUN_TEST(test_too_few_digits);
  RUN_TEST(test_trailing_space);
  RUN_TEST(test_leading_space);
  RUN_TEST(test_uppercase_prefix);
  RUN_TEST(test_newline_invalid);
  RUN_TEST(test_null_name_invalid);
  UNITY_END();
}
void loop() {}
#else
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_migrate_lower_boundary);
  RUN_TEST(test_migrate_upper_boundary);
  RUN_TEST(test_inplace);
  RUN_TEST(test_exact_fit);
  RUN_TEST(test_insufficient);
  RUN_TEST(test_already_valid);
  RUN_TEST(test_invalid_prefix);
  RUN_TEST(test_missing_dash);
  RUN_TEST(test_too_many_digits);
  RUN_TEST(test_too_few_digits);
  RUN_TEST(test_trailing_space);
  RUN_TEST(test_leading_space);
  RUN_TEST(test_uppercase_prefix);
  RUN_TEST(test_newline_invalid);
  RUN_TEST(test_null_name_invalid);
  return UNITY_END();
}
#endif