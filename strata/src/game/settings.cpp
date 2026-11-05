#include "game/settings.hpp"

#include <fstream>
#include <sstream>

#include "core/log.hpp"
#include "core/math.hpp"
#include "world/storage.hpp"

namespace strata {

namespace {
std::string trim(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  const size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}
bool toBool(const std::string& v) { return v == "1" || v == "true" || v == "on" || v == "yes"; }
}  // namespace

void Settings::clampValues() {
  renderDistance = clampv(renderDistance, 2, 32);
  fov = clampv(fov, 40.0f, 120.0f);
  mouseSensitivity = clampv(mouseSensitivity, 0.05f, 5.0f);
  shadowQuality = clampv(shadowQuality, 0, 3);
  cloudQuality = clampv(cloudQuality, 0, 2);
  brightness = clampv(brightness, 0.25f, 3.0f);
  guiScale = clampv(guiScale, 0, 6);
  workerThreads = clampv(workerThreads, 0, 32);
  renderScale = clampv(renderScale, 0.5f, 1.0f);
  touchSensitivity = clampv(touchSensitivity, 0.2f, 4.0f);
}

void Settings::applyMobileDefaults() {
  renderDistance = 6;
  shadowQuality = 1;
  cloudQuality = 1;
  reflections = true;
  bloom = true;
  antialiasing = true;
  renderScale = 0.75f;
  fov = 75.0f;
}

bool Settings::load(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::string line;
  while (std::getline(f, line)) {
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));
    std::istringstream in(value);
    if (key == "render_distance") in >> renderDistance;
    else if (key == "fov") in >> fov;
    else if (key == "mouse_sensitivity") in >> mouseSensitivity;
    else if (key == "invert_mouse") invertMouse = toBool(value);
    else if (key == "shadows") in >> shadowQuality;
    else if (key == "clouds") in >> cloudQuality;
    else if (key == "reflections") reflections = toBool(value);
    else if (key == "bloom") bloom = toBool(value);
    else if (key == "antialiasing") antialiasing = toBool(value);
    else if (key == "brightness") in >> brightness;
    else if (key == "vsync") vsync = toBool(value);
    else if (key == "view_bobbing") viewBobbing = toBool(value);
    else if (key == "gui_scale") in >> guiScale;
    else if (key == "fullscreen") fullscreen = toBool(value);
    else if (key == "worker_threads") in >> workerThreads;
    else if (key == "render_scale") in >> renderScale;
    else if (key == "touch_sensitivity") in >> touchSensitivity;
  }
  clampValues();
  return true;
}

bool Settings::save(const std::string& path) const {
  std::ostringstream o;
  o << "# Strata settings\n";
  o << "render_distance = " << renderDistance << "\n";
  o << "fov = " << fov << "\n";
  o << "mouse_sensitivity = " << mouseSensitivity << "\n";
  o << "invert_mouse = " << (invertMouse ? "true" : "false") << "\n";
  o << "shadows = " << shadowQuality << "   # 0 off, 1 low, 2 medium, 3 high\n";
  o << "clouds = " << cloudQuality << "    # 0 off, 1 fast, 2 fancy\n";
  o << "reflections = " << (reflections ? "true" : "false") << "\n";
  o << "bloom = " << (bloom ? "true" : "false") << "\n";
  o << "antialiasing = " << (antialiasing ? "true" : "false") << "\n";
  o << "brightness = " << brightness << "\n";
  o << "vsync = " << (vsync ? "true" : "false") << "\n";
  o << "view_bobbing = " << (viewBobbing ? "true" : "false") << "\n";
  o << "gui_scale = " << guiScale << "   # 0 = automatic\n";
  o << "fullscreen = " << (fullscreen ? "true" : "false") << "\n";
  o << "worker_threads = " << workerThreads << "   # 0 = automatic\n";
  o << "render_scale = " << renderScale << "   # 0.5 .. 1, resolution of the 3D view\n";
  o << "touch_sensitivity = " << touchSensitivity << "\n";
  if (!writeFileAtomic(path, o.str())) {
    LOG_WARN("could not save settings to %s", path.c_str());
    return false;
  }
  return true;
}

}  // namespace strata
