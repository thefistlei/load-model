#pragma once

#include <string>

// Root directory for config/models/shaders (exe dir on desktop, extracted assets on Android).
std::string platformDataRoot();

bool platformFileExists(const std::string& path);
bool platformIsDirectory(const std::string& path);

// Android only: extract APK assets to internal storage (no-op on desktop).
void platformInitAssets();
