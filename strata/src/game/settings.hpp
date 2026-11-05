// User settings, stored as "key = value" lines.
#pragma once

#include <string>

namespace strata {

struct Settings {
  int renderDistance = 10;    // chunks
  float fov = 72.0f;          // degrees
  float mouseSensitivity = 1.0f;
  bool invertMouse = false;
  int shadowQuality = 2;      // 0 off .. 3 high
  int cloudQuality = 2;       // 0 off, 1 fast, 2 fancy
  bool reflections = true;
  bool bloom = true;
  bool antialiasing = true;
  float brightness = 1.0f;
  bool vsync = true;
  bool viewBobbing = true;
  int guiScale = 0;           // 0 = automatic
  bool fullscreen = false;
  int workerThreads = 0;      // 0 = automatic
  float renderScale = 1.0f;   // 3D resolution relative to the screen (0.5 .. 1)
  float touchSensitivity = 1.0f;  // camera speed of touch screen drags

  // Lighter defaults for phones and tablets (used when no settings file exists).
  void applyMobileDefaults();

  void clampValues();
  bool load(const std::string& path);
  bool save(const std::string& path) const;
};

}  // namespace strata
