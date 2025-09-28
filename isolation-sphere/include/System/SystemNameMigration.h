#pragma once
#include <cstddef>

bool isValidSystemName(const char* name);
bool migrateSystemName(const char* oldName, char* outBuf, std::size_t outSize);