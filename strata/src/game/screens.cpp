// HUD, menus, inventory, chat and console commands.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "core/log.hpp"
#include "game/game.hpp"
#include "world/worldgen.hpp"

namespace strata {

namespace fs = std::filesystem;

namespace {
constexpr int64_t kDayTicks = 24000;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string keyName(const std::string& displayName) {
  std::string k = lower(displayName);
  for (char& c : k)
    if (c == ' ') c = '_';
  return k;
}

// Resolves "oak_log", "Oak Log", "stick" or a numeric id.
ItemId findItem(const std::string& name) {
  const std::string want = keyName(name);
  bool numeric = !want.empty();
  for (char c : want) numeric = numeric && std::isdigit(static_cast<unsigned char>(c));
  if (numeric) {
    const int v = std::atoi(want.c_str());
    return isValidItem(static_cast<ItemId>(v)) ? static_cast<ItemId>(v) : static_cast<ItemId>(item::None);
  }
  for (int i = 1; i < block::Count; ++i)
    if (keyName(blockInfo(static_cast<BlockId>(i)).name) == want) return static_cast<ItemId>(i);
  for (ItemId i = item::Stick; i < item::End; ++i)
    if (keyName(itemInfo(i).name) == want) return i;
  return item::None;
}

std::vector<std::string> split(const std::string& s) {
  std::istringstream in(s);
  std::vector<std::string> out;
  std::string t;
  while (in >> t) out.push_back(t);
  return out;
}

// Parses an absolute or relative ("~", "~3") coordinate.
bool parseCoord(const std::string& s, double base, double& out) {
  if (!s.empty() && s[0] == '~') {
    out = base + (s.size() > 1 ? std::atof(s.c_str() + 1) : 0.0);
    return true;
  }
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end && *end == '\0' && !s.empty();
}

std::string formatTime(int64_t ticks) {
  // Tick 0 is 06:00.
  const int64_t t = ((ticks % kDayTicks) + kDayTicks) % kDayTicks;
  const int minutes = static_cast<int>(((t + 6000) % kDayTicks) * 1440 / kDayTicks);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "day %lld, %02d:%02d", static_cast<long long>(ticks / kDayTicks + 1), minutes / 60,
                minutes % 60);
  return buf;
}

const char* facingName(float yaw) {
  const float deg = degrees(yaw);
  const float a = deg < 0 ? deg + 360.0f : deg;
  if (a < 45 || a >= 315) return "north (-Z)";
  if (a < 135) return "east (+X)";
  if (a < 225) return "south (+Z)";
  return "west (-X)";
}

void heart(UiRenderer& ui, float x, float y, float px, float fill, uint32_t full, uint32_t empty) {
  static const char* const kShape[6] = {".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."};
  for (int r = 0; r < 6; ++r)
    for (int c = 0; c < 7; ++c) {
      if (kShape[r][c] != '#') continue;
      const bool lit = (static_cast<float>(c) + 0.5f) / 7.0f <= fill;
      ui.rect(x + static_cast<float>(c) * px, y + static_cast<float>(r) * px, px, px, lit ? full : empty);
    }
}
}  // namespace

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------

bool Game::button(float x, float y, float w, float h, const std::string& label, bool enabled) {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const vec2 m = input_.mousePos();
  const bool hover = enabled && m.x >= x && m.x < x + w && m.y >= y && m.y < y + h;
  ui.rect(x, y, w, h, !enabled ? rgba(30, 32, 38, 200) : hover ? rgba(72, 94, 122, 240) : rgba(38, 44, 56, 230));
  ui.frame(x, y, w, h, g, hover ? rgba(210, 225, 245) : rgba(92, 104, 122));
  const float tw = ui.textWidth(label, g);
  ui.text(x + (w - tw) * 0.5f, y + (h - 8.0f * g) * 0.5f, label, enabled ? rgba(240, 240, 240) : rgba(130, 130, 130), g);
  return hover && input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT);
}

bool Game::slider(float x, float y, float w, float h, const std::string& label, float& value, float lo, float hi,
                  int decimals) {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const vec2 m = input_.mousePos();
  const bool hover = m.x >= x && m.x < x + w && m.y >= y && m.y < y + h;
  bool changed = false;
  if (hover && input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
    float t = saturate((m.x - x - 4.0f * g) / (w - 8.0f * g));
    float v = lo + (hi - lo) * t;
    if (decimals == 0) v = std::round(v);
    if (v != value) {
      value = v;
      changed = true;
    }
  }
  ui.rect(x, y, w, h, hover ? rgba(52, 62, 78, 240) : rgba(34, 40, 50, 230));
  ui.frame(x, y, w, h, g, rgba(92, 104, 122));
  const float t = saturate((value - lo) / (hi - lo));
  const float kx = x + 4.0f * g + t * (w - 8.0f * g);
  // Filled track plus a slim marker; both stay behind the (shadowed) label.
  ui.rect(x + g, y + h - 3.0f * g, (kx - x - g), 2.0f * g, rgba(110, 150, 200, 230));
  ui.rect(kx - g, y + g, 2.0f * g, h - 2.0f * g, rgba(150, 175, 210, 170));
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s: %.*f", label.c_str(), decimals, static_cast<double>(value));
  const float tw = ui.textWidth(buf, g);
  ui.text(x + (w - tw) * 0.5f, y + (h - 8.0f * g) * 0.5f, buf, rgba(240, 240, 240), g);
  return changed;
}

bool Game::textField(float x, float y, float w, float h, std::string& text, int id, size_t maxLen,
                     const char* title) {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const vec2 m = input_.mousePos();
  const bool hover = m.x >= x && m.x < x + w && m.y >= y && m.y < y + h;
  bool changed = false;
  if (input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
    if (hover) focusedField_ = id;
    else if (focusedField_ == id) focusedField_ = -1;
    // Touch screens send no key events for their on-screen keyboard: ask with
    // a native dialog instead.
    std::string v = text;
    if (hover && touchUi() && Window::promptText(title, v)) {
      text = v.substr(0, maxLen);
      changed = true;
    }
  }
  const bool focused = focusedField_ == id;
  if (focused) {
    for (char c : input_.typed())
      if (text.size() < maxLen) {
        text.push_back(c);
        changed = true;
      }
    if (input_.repeated(GLFW_KEY_BACKSPACE) && !text.empty()) {
      text.pop_back();
      changed = true;
    }
  }
  ui.rect(x, y, w, h, rgba(14, 16, 20, 235));
  ui.frame(x, y, w, h, g, focused ? rgba(220, 230, 250) : rgba(92, 104, 122));
  std::string shown = text;
  while (!shown.empty() && ui.textWidth(shown, g) > w - 10.0f * g) shown.erase(shown.begin());
  const float tx = x + 4.0f * g, ty = y + (h - 8.0f * g) * 0.5f;
  const float tw = ui.text(tx, ty, shown, rgba(235, 235, 235), g);
  if (focused && std::fmod(time_, 1.0) < 0.55) ui.rect(tx + tw + g, ty, g, 8.0f * g, rgba(235, 235, 235));
  return changed;
}

bool Game::slotWidget(float x, float y, float size, ItemStack& stack, bool creativePalette) {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const vec2 m = input_.mousePos();
  const bool hover = m.x >= x && m.x < x + size && m.y >= y && m.y < y + size;
  ui.rect(x, y, size, size, hover ? rgba(96, 104, 120, 220) : rgba(42, 46, 56, 220));
  ui.frame(x, y, size, size, g, rgba(20, 22, 28, 255));
  if (!stack.empty()) {
    ui.icon(stack.id, x + 2.0f * g, y + 2.0f * g, size - 4.0f * g);
    if (stack.count > 1 && !creativePalette) {
      const std::string c = std::to_string(stack.count);
      ui.text(x + size - ui.textWidth(c, g) - g, y + size - 9.0f * g, c, rgba(255, 255, 255), g);
    }
  }
  return hover;
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
void Game::drawHotbar(float g) {
  UiRenderer& ui = renderer_.ui();
  Session& s = *session_;
  const float slot = 20.0f * g;
  const float w = slot * 9.0f;
  const float x0 = (static_cast<float>(fbWidth_) - w) * 0.5f;
  const float y0 = static_cast<float>(fbHeight_) - slot - 4.0f * g;
  ui.rect(x0 - g, y0 - g, w + 2.0f * g, slot + 2.0f * g, rgba(10, 12, 16, 150));
  for (int i = 0; i < Inventory::kHotbar; ++i) {
    const float x = x0 + slot * static_cast<float>(i);
    ui.rect(x + g, y0 + g, slot - 2.0f * g, slot - 2.0f * g, rgba(40, 44, 54, 170));
    const ItemStack& st = s.inventory.slots[i];
    if (!st.empty()) {
      ui.icon(st.id, x + 2.0f * g, y0 + 2.0f * g, slot - 4.0f * g);
      if (st.count > 1 && !s.player.creative) {
        const std::string c = std::to_string(st.count);
        ui.text(x + slot - ui.textWidth(c, g) - g, y0 + slot - 9.0f * g, c, rgba(255, 255, 255), g);
      }
    }
    if (i == s.selected) ui.frame(x - g, y0 - g, slot + 2.0f * g, slot + 2.0f * g, 2.0f * g, rgba(245, 245, 245, 240));
  }
  if (!s.player.creative) {
    // Health (hearts) and breath.
    const float hy = y0 - 9.0f * g;
    for (int i = 0; i < 10; ++i) {
      const float fill = clampv(s.player.health / 2.0f - static_cast<float>(i), 0.0f, 1.0f);
      heart(ui, x0 + static_cast<float>(i) * 8.0f * g, hy, g, fill, rgba(214, 38, 38), rgba(52, 20, 20, 200));
    }
    if (s.player.headInWater || s.player.air < Player::kMaxAir) {
      const int bubbles = static_cast<int>(std::ceil(s.player.air / Player::kMaxAir * 10.0f));
      for (int i = 0; i < bubbles; ++i) {
        const float bx = x0 + w - static_cast<float>(i + 1) * 8.0f * g;
        ui.rect(bx + g, hy, 5.0f * g, 6.0f * g, rgba(120, 190, 255, 220));
        ui.frame(bx + g, hy, 5.0f * g, 6.0f * g, g, rgba(40, 70, 140, 240));
      }
    }
  }
  if (s.selectionTimer > 0.0f && !s.inventory.slots[s.selected].empty()) {
    const std::string name = itemInfo(s.inventory.slots[s.selected].id).name;
    const float a = std::min(1.0f, s.selectionTimer);
    const float tw = ui.textWidth(name, g);
    ui.text((static_cast<float>(fbWidth_) - tw) * 0.5f, y0 - (s.player.creative ? 12.0f : 22.0f) * g, name,
            rgba(255, 255, 255, static_cast<int>(a * 255)), g);
  }
}

void Game::drawMessages(float g, bool all) {
  UiRenderer& ui = renderer_.ui();
  // Newest at the bottom, above the hotbar; with touch controls the lower
  // left belongs to the movement stick, so messages run down from the top
  // left instead (newest first).
  const bool top = touchUi() && screen_ != Screen::Chat;
  float y = top ? 4.0f * g : static_cast<float>(fbHeight_) - 40.0f * g;
  int shown = 0;
  for (auto it = messages_.rbegin(); it != messages_.rend() && shown < (all ? 14 : 6); ++it) {
    if (!all && it->age > 8.0f) break;
    const float a = all ? 1.0f : std::min(1.0f, (8.0f - it->age) * 2.0f);
    const float tw = ui.textWidth(it->text, g);
    ui.rect(2.0f * g, y - g, tw + 4.0f * g, 10.0f * g, rgba(0, 0, 0, static_cast<int>(110 * a)));
    ui.text(4.0f * g, y, it->text, rgba(240, 240, 240, static_cast<int>(255 * a)), g);
    y += top ? 10.0f * g : -10.0f * g;
    ++shown;
  }
}

void Game::drawHud() {
  if (!session_ || !session_->ready) return;
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  if (!hideHud_) {
    // Touch screens aim with the finger instead of a crosshair.
    if (screen_ == Screen::Playing && !touchUi())
      ui.crosshair(static_cast<float>(fbWidth_) * 0.5f, static_cast<float>(fbHeight_) * 0.5f, 5.0f * g, g);
    drawHotbar(hudScale());
  }
  // The touch controls stay visible with the HUD hidden: they are the only way out.
  if (screen_ == Screen::Playing && touchUi()) touch_.draw(ui, touchLayout());
  if (screen_ != Screen::Chat && !hideHud_) drawMessages(g, false);
  if (showDebug_) drawDebug();
}

void Game::drawDebug() {
  Session& s = *session_;
  World& w = *s.world;
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const Player& p = s.player;
  const int bx = fastFloor(p.pos.x), by = fastFloor(p.pos.y), bz = fastFloor(p.pos.z);
  const WorldStats st = w.stats();
  const RenderStats& rs = renderer_.stats();
  std::vector<std::string> lines;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "Strata 1.0   %.0f fps (%.1f ms)", fps_, frameMs_);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "XYZ: %.2f / %.2f / %.2f", p.pos.x, p.pos.y, p.pos.z);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "Block: %d %d %d   Chunk: %d %d", bx, by, bz, floorDiv(bx, 16), floorDiv(bz, 16));
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "Facing: %s (yaw %.1f, pitch %.1f)", facingName(p.yaw), static_cast<double>(degrees(p.yaw)),
                static_cast<double>(degrees(p.pitch)));
  lines.emplace_back(buf);
  const ColumnClimate* cl = w.climateAt(bx, bz);
  const uint8_t l = w.getLight(bx, fastFloor(p.pos.y + 1.0), bz);
  std::snprintf(buf, sizeof(buf), "Biome: %s   Light: sky %d, block %d", cl ? biomeName(static_cast<Biome>(cl->biome)) : "?",
                skyOf(l), blockLightOf(l));
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "Time: %s   Mode: %s%s", formatTime(s.time).c_str(), p.creative ? "creative" : "survival",
                p.flying ? " (flying)" : "");
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "Chunks: %zu loaded, %zu lit   Sections: %d/%d drawn", st.chunks, st.litChunks,
                rs.sectionsVisible, rs.sectionsTotal);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "Jobs: gen %zu  light %zu  mesh %zu   Fluid ticks: %zu   Draws: %d", st.genInFlight,
                st.lightInFlight, st.meshInFlight, st.scheduledTicks, rs.drawCalls);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "GPU mesh memory: %.1f MB   Seed: %llu", static_cast<double>(rs.gpuBytes) / 1048576.0,
                static_cast<unsigned long long>(w.seed()));
  lines.emplace_back(buf);
  if (s.target.hit) {
    std::snprintf(buf, sizeof(buf), "Looking at: %s (%d, %d, %d)", blockInfo(s.target.id).name, s.target.block.x,
                  s.target.block.y, s.target.block.z);
    lines.emplace_back(buf);
  }
  lines.push_back(renderer_.rendererName());
  float y = 2.0f * g;
  for (const std::string& line : lines) {
    const float tw = ui.textWidth(line, g);
    ui.rect(g, y - g, tw + 4.0f * g, 10.0f * g, rgba(0, 0, 0, 130));
    ui.text(3.0f * g, y, line, rgba(235, 235, 235), g);
    y += 10.0f * g;
  }
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
void Game::drawBackground() {
  UiRenderer& ui = renderer_.ui();
  const float h = static_cast<float>(fbHeight_);
  const int bands = 24;
  for (int i = 0; i < bands; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(bands - 1);
    const int r = static_cast<int>(lerpf(26.0f, 8.0f, t)), gg = static_cast<int>(lerpf(52.0f, 12.0f, t)),
              b = static_cast<int>(lerpf(86.0f, 18.0f, t));
    ui.rect(0, h * static_cast<float>(i) / bands, static_cast<float>(fbWidth_), h / bands + 1.0f, rgba(r, gg, b));
  }
}

void Game::drawMainMenu() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  drawBackground();
  const std::string title = "STRATA";
  const float ts = 6.0f * g;
  ui.text((W - ui.textWidth(title, ts)) * 0.5f, H * 0.12f, title, rgba(240, 236, 220), ts);
  const std::string sub = "an endless world of stone, water and sky";
  ui.text((W - ui.textWidth(sub, g)) * 0.5f, H * 0.12f + 56.0f * g, sub, rgba(170, 190, 210), g);
  const float bw = 200.0f * g, bh = 20.0f * g;
  float y = H * 0.12f + 80.0f * g;
  const float x = (W - bw) * 0.5f;
  // As many saved worlds as fit above the remaining buttons and the hint.
  const int buttons = Window::canQuit() ? 3 : 2;
  const int fit = static_cast<int>((H - 18.0f * g - y) / (bh + 4.0f * g)) - buttons;
  const int maxWorlds = clampv(fit, 1, 5);
  int shown = 0;
  for (const auto& wl : worldList_) {
    if (shown++ >= maxWorlds) break;
    if (button(x, y, bw, bh, "Play: " + wl.second)) {
      startSession(wl.first, false, 0, false);
      return;
    }
    y += bh + 4.0f * g;
  }
  if (button(x, y, bw, bh, "Create New World")) {
    newWorldName_ = "New World";
    newWorldSeed_.clear();
    setScreen(Screen::CreateWorld);
    return;
  }
  y += bh + 4.0f * g;
  if (button(x, y, bw, bh, "Settings")) {
    settingsReturn_ = Screen::MainMenu;
    setScreen(Screen::Settings);
    return;
  }
  y += bh + 4.0f * g;
  if (Window::canQuit() && button(x, y, bw, bh, "Quit")) quit_ = true;  // a browser tab is closed instead
  const std::string hint = touchUi() ? "Left thumb moves, drag to look, tap to place, hold to break"
                                     : "WASD move, mouse look, LMB break, RMB place, E inventory, Esc pause";
  // Shrink the hint on narrow (portrait) screens.
  const float hs = std::min(g, (W - 8.0f) / std::max(1.0f, ui.textWidth(hint, 1.0f)));
  ui.text((W - ui.textWidth(hint, hs)) * 0.5f, H - 14.0f * g, hint, rgba(150, 160, 175), hs);
}

void Game::drawCreateWorld() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  drawBackground();
  const std::string title = "Create a New World";
  ui.text((W - ui.textWidth(title, 2.0f * g)) * 0.5f, H * 0.15f, title, rgba(240, 240, 240), 2.0f * g);
  const float fw = 200.0f * g, fh = 20.0f * g, x = (W - fw) * 0.5f;
  float y = H * 0.15f + 34.0f * g;
  ui.text(x, y, "World name", rgba(190, 200, 215), g);
  textField(x, y + 10.0f * g, fw, fh, newWorldName_, 1, 32, "World name");
  y += 38.0f * g;
  ui.text(x, y, "Seed (leave empty for a random world)", rgba(190, 200, 215), g);
  textField(x, y + 10.0f * g, fw, fh, newWorldSeed_, 2, 32, "Seed (leave empty for a random world)");
  y += 38.0f * g;
  if (button(x, y, fw, fh, std::string("Game mode: ") + (newWorldCreative_ ? "Creative" : "Survival")))
    newWorldCreative_ = !newWorldCreative_;
  y += fh + 10.0f * g;
  const bool create = button(x, y, fw / 2 - 2.0f * g, fh, "Create") || input_.pressed(GLFW_KEY_ENTER);
  if (button(x + fw / 2 + 2.0f * g, y, fw / 2 - 2.0f * g, fh, "Cancel") || input_.pressed(GLFW_KEY_ESCAPE)) {
    setScreen(Screen::MainMenu);
    return;
  }
  if (create) {
    // Folder name: letters, digits, '-' and '_' only; made unique.
    std::string folder;
    for (char c : newWorldName_) {
      if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') folder.push_back(c);
      else if (c == ' ') folder.push_back('_');
    }
    if (folder.empty()) folder = "world";
    std::string unique = folder;
    for (int i = 2; fs::exists(fs::path(options_.savesDir) / unique); ++i) unique = folder + "_" + std::to_string(i);
    uint64_t seed;
    if (newWorldSeed_.empty()) {
      seed = rng_.next() | (static_cast<uint64_t>(rng_.next()) << 32);
    } else {
      char* end = nullptr;
      const unsigned long long v = std::strtoull(newWorldSeed_.c_str(), &end, 10);
      if (end && *end == '\0') {
        seed = v;
      } else {
        seed = 1469598103934665603ull;  // FNV-1a of the text
        for (char c : newWorldSeed_) seed = (seed ^ static_cast<unsigned char>(c)) * 1099511628211ull;
      }
    }
    startSession(unique, true, seed, newWorldCreative_);
  }
}

void Game::drawLoading() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  drawBackground();
  const std::string t = "Shaping the world...";
  ui.text((W - ui.textWidth(t, 2.0f * g)) * 0.5f, H * 0.42f, t, rgba(235, 235, 235), 2.0f * g);
  if (session_) {
    const WorldStats st = session_->world->stats();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%zu chunks generated, %zu lit", st.chunks, st.litChunks);
    ui.text((W - ui.textWidth(buf, g)) * 0.5f, H * 0.42f + 26.0f * g, buf, rgba(170, 185, 200), g);
    const float bw = 160.0f * g;
    const float p = std::min(1.0f, static_cast<float>(st.litChunks) / 25.0f);
    ui.rect((W - bw) * 0.5f, H * 0.42f + 40.0f * g, bw, 4.0f * g, rgba(30, 36, 46));
    ui.rect((W - bw) * 0.5f, H * 0.42f + 40.0f * g, bw * p, 4.0f * g, rgba(120, 190, 120));
  }
}

void Game::drawPause() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  ui.rect(0, 0, W, H, rgba(0, 0, 0, 120));
  const std::string t = "Game paused";
  ui.text((W - ui.textWidth(t, 2.0f * g)) * 0.5f, H * 0.25f, t, rgba(240, 240, 240), 2.0f * g);
  const float bw = 200.0f * g, bh = 20.0f * g, x = (W - bw) * 0.5f;
  float y = H * 0.25f + 30.0f * g;
  if (button(x, y, bw, bh, "Back to Game") || input_.pressed(GLFW_KEY_ESCAPE)) {
    setScreen(Screen::Playing);
    return;
  }
  y += bh + 4.0f * g;
  if (button(x, y, bw, bh, "Settings")) {
    settingsReturn_ = Screen::Paused;
    setScreen(Screen::Settings);
    return;
  }
  y += bh + 4.0f * g;
  if (button(x, y, bw, bh, "Save and Quit to Title")) {
    endSession();
    setScreen(Screen::MainMenu);
  }
}

void Game::drawSettings() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  if (session_ && session_->ready)
    ui.rect(0, 0, W, H, rgba(0, 0, 0, 150));
  else
    drawBackground();
  const std::string t = "Settings";
  ui.text((W - ui.textWidth(t, 2.0f * g)) * 0.5f, 12.0f * g, t, rgba(240, 240, 240), 2.0f * g);
  const float cw = 150.0f * g, ch = 20.0f * g, gap = 4.0f * g;
  const float x0 = W * 0.5f - cw - gap, x1 = W * 0.5f + gap;
  float y = 40.0f * g;
  bool changed = false;
  float rd = static_cast<float>(settings_.renderDistance);
  if (slider(x0, y, cw, ch, "Render distance", rd, 2.0f, 24.0f, 0)) {
    settings_.renderDistance = static_cast<int>(rd);
    changed = true;
  }
  changed |= slider(x1, y, cw, ch, "Field of view", settings_.fov, 50.0f, 110.0f, 0);
  y += ch + gap;
  changed |= slider(x0, y, cw, ch, "Mouse sensitivity", settings_.mouseSensitivity, 0.2f, 3.0f, 2);
  changed |= slider(x1, y, cw, ch, "Brightness", settings_.brightness, 0.5f, 2.0f, 2);
  y += ch + gap;
  static const char* const kQuality[4] = {"Off", "Low", "Medium", "High"};
  static const char* const kClouds[3] = {"Off", "Fast", "Fancy"};
  if (button(x0, y, cw, ch, std::string("Shadows: ") + kQuality[settings_.shadowQuality])) {
    settings_.shadowQuality = (settings_.shadowQuality + 1) % 4;
    changed = true;
  }
  if (button(x1, y, cw, ch, std::string("Clouds: ") + kClouds[settings_.cloudQuality])) {
    settings_.cloudQuality = (settings_.cloudQuality + 1) % 3;
    changed = true;
  }
  y += ch + gap;
  auto toggle = [&](float x, const char* name, bool& v) {
    if (button(x, y, cw, ch, std::string(name) + (v ? ": On" : ": Off"))) {
      v = !v;
      changed = true;
    }
  };
  toggle(x0, "Water reflections", settings_.reflections);
  toggle(x1, "Bloom", settings_.bloom);
  y += ch + gap;
  toggle(x0, "Anti-aliasing", settings_.antialiasing);
  toggle(x1, "View bobbing", settings_.viewBobbing);
  y += ch + gap;
  toggle(x0, "VSync", settings_.vsync);
  toggle(x1, "Invert mouse", settings_.invertMouse);
  y += ch + gap;
  float scalePercent = settings_.renderScale * 100.0f;
  if (slider(x0, y, cw, ch, "Render scale %", scalePercent, 50.0f, 100.0f, 0)) {
    settings_.renderScale = scalePercent / 100.0f;
    changed = true;
  }
  changed |= slider(x1, y, cw, ch, "Touch sensitivity", settings_.touchSensitivity, 0.3f, 3.0f, 2);
  y += ch + gap;
  const std::string gui = settings_.guiScale == 0 ? "Auto" : std::to_string(settings_.guiScale);
  if (button(x0, y, cw, ch, "GUI scale: " + gui)) {
    settings_.guiScale = (settings_.guiScale + 1) % 5;
    changed = true;
  }
  if (button(x1, y, cw, ch, std::string("Fullscreen: ") + (settings_.fullscreen ? "On" : "Off"))) setFullscreen(!settings_.fullscreen);
  y += ch + gap * 3.0f;
  if (changed) applySettings();
  if (button((W - cw) * 0.5f, y, cw, ch, "Done") || input_.pressed(GLFW_KEY_ESCAPE)) {
    settings_.save(options_.settingsPath);
    persist();
    setScreen(settingsReturn_);
  }
}

void Game::drawDeath() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  ui.rect(0, 0, W, H, rgba(90, 0, 0, 120));
  const std::string t = "You died";
  ui.text((W - ui.textWidth(t, 3.0f * g)) * 0.5f, H * 0.3f, t, rgba(255, 230, 230), 3.0f * g);
  const float bw = 200.0f * g, bh = 20.0f * g, x = (W - bw) * 0.5f;
  if (button(x, H * 0.3f + 40.0f * g, bw, bh, "Respawn")) {
    respawn();
    return;
  }
  if (button(x, H * 0.3f + 64.0f * g, bw, bh, "Title Screen")) {
    respawn();
    endSession();
    setScreen(Screen::MainMenu);
  }
}

// ---------------------------------------------------------------------------
// Inventory
// ---------------------------------------------------------------------------
void Game::drawInventory() {
  Session& s = *session_;
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  const vec2 m = input_.mousePos();
  const bool creative = s.player.creative;
  const bool left = input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT);
  const bool right = input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT);
  const bool shift = input_.down(GLFW_KEY_LEFT_SHIFT) || input_.down(GLFW_KEY_RIGHT_SHIFT);
  ui.rect(0, 0, W, H, rgba(0, 0, 0, 130));

  const float slot = 18.0f * g;
  std::vector<ItemId> palette;
  if (creative) {
    for (int i = 1; i < block::Count; ++i)
      if (blockInfo(static_cast<BlockId>(i)).inCreative) palette.push_back(static_cast<ItemId>(i));
    for (ItemId i = item::Stick; i < item::End; ++i) palette.push_back(i);
  }
  const int paletteRows = static_cast<int>((palette.size() + 8) / 9);
  const std::vector<Recipe>& rec = recipes();
  const float invW = slot * 9.0f + 8.0f * g;
  const float craftW = creative ? 0.0f : 120.0f * g;
  const float topH = creative ? static_cast<float>(paletteRows) * slot + 14.0f * g : 14.0f * g;
  const float invH = topH + slot * 4.0f + 24.0f * g;
  const float panelH = std::max(invH, creative ? 0.0f : static_cast<float>(rec.size()) * (slot + 2.0f * g) + 20.0f * g);
  const float px = (W - invW - craftW) * 0.5f, py = (H - panelH) * 0.5f;
  ui.rect(px, py, invW, panelH, rgba(24, 28, 36, 240));
  ui.frame(px, py, invW, panelH, g, rgba(100, 112, 130));
  ui.text(px + 4.0f * g, py + 3.0f * g, creative ? "Creative palette" : "Inventory", rgba(225, 225, 225), g);
  bool clickedSomething = false;
  std::string tooltip;

  auto handleSlot = [&](ItemStack& st) {
    if (left && shift && !creative) {
      // Quick move between the hotbar and the main inventory.
      clickedSomething = true;
      if (st.empty()) return;
      const int idx = static_cast<int>(&st - s.inventory.slots);
      const int from = idx < Inventory::kHotbar ? Inventory::kHotbar : 0;
      const int to = idx < Inventory::kHotbar ? Inventory::kSlots : Inventory::kHotbar;
      for (int i = from; i < to && st.count > 0; ++i) {
        ItemStack& d = s.inventory.slots[i];
        if (d.id == st.id && d.count < 64) {
          const int moved = std::min<int>(64 - d.count, st.count);
          d.count = static_cast<uint16_t>(d.count + moved);
          st.count = static_cast<uint16_t>(st.count - moved);
        }
      }
      for (int i = from; i < to && st.count > 0; ++i)
        if (s.inventory.slots[i].empty()) {
          s.inventory.slots[i] = st;
          st.count = 0;
        }
      if (st.count == 0) st = ItemStack{};
      return;
    }
    if (left) {
      clickedSomething = true;
      if (s.cursor.empty()) {
        std::swap(s.cursor, st);
      } else if (!st.empty() && st.id == s.cursor.id) {
        const int moved = std::min<int>(64 - st.count, s.cursor.count);
        st.count = static_cast<uint16_t>(st.count + moved);
        s.cursor.count = static_cast<uint16_t>(s.cursor.count - moved);
        if (s.cursor.count == 0) s.cursor = ItemStack{};
      } else {
        std::swap(s.cursor, st);
      }
    } else if (right) {
      clickedSomething = true;
      if (s.cursor.empty() && !st.empty()) {
        const uint16_t half = static_cast<uint16_t>((st.count + 1) / 2);
        s.cursor = {st.id, half};
        st.count = static_cast<uint16_t>(st.count - half);
        if (st.count == 0) st = ItemStack{};
      } else if (!s.cursor.empty() && (st.empty() || (st.id == s.cursor.id && st.count < 64))) {
        if (st.empty()) st = {s.cursor.id, 0};
        st.count = static_cast<uint16_t>(st.count + 1);
        s.cursor.count = static_cast<uint16_t>(s.cursor.count - 1);
        if (s.cursor.count == 0) s.cursor = ItemStack{};
      }
    }
  };

  float y = py + 14.0f * g;
  const float x0 = px + 4.0f * g;
  if (creative) {
    for (size_t i = 0; i < palette.size(); ++i) {
      ItemStack st{palette[i], 1};
      const float sx = x0 + slot * static_cast<float>(i % 9), sy = y + slot * static_cast<float>(i / 9);
      if (slotWidget(sx, sy, slot, st, true)) {
        tooltip = itemInfo(palette[i]).name;
        if (left || right) {
          clickedSomething = true;
          if (!s.cursor.empty()) s.cursor = ItemStack{};  // dropping onto the palette deletes the stack
          else s.cursor = {palette[i], static_cast<uint16_t>(right ? 1 : itemInfo(palette[i]).maxStack)};
        }
      }
    }
    y += static_cast<float>(paletteRows) * slot + 8.0f * g;
  }
  if (!creative || true) {
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 9; ++c) {
        ItemStack& st = s.inventory.slots[Inventory::kHotbar + r * 9 + c];
        if (slotWidget(x0 + slot * static_cast<float>(c), y + slot * static_cast<float>(r), slot, st, false)) {
          if (!st.empty()) tooltip = itemInfo(st.id).name;
          handleSlot(st);
        }
      }
    y += slot * 3.0f + 6.0f * g;
    for (int c = 0; c < 9; ++c) {
      ItemStack& st = s.inventory.slots[c];
      if (slotWidget(x0 + slot * static_cast<float>(c), y, slot, st, false)) {
        if (!st.empty()) tooltip = itemInfo(st.id).name;
        handleSlot(st);
      }
      if (c == s.selected) ui.frame(x0 + slot * static_cast<float>(c), y, slot, slot, g, rgba(240, 240, 240));
    }
  }

  if (!creative) {
    // Crafting list.
    const float cx = px + invW + 4.0f * g;
    ui.rect(cx - 2.0f * g, py, craftW, panelH, rgba(24, 28, 36, 240));
    ui.frame(cx - 2.0f * g, py, craftW, panelH, g, rgba(100, 112, 130));
    ui.text(cx + 2.0f * g, py + 3.0f * g, "Crafting", rgba(225, 225, 225), g);
    float ry = py + 14.0f * g;
    for (const Recipe& r : rec) {
      const bool can = canCraft(s.inventory, r);
      const float rw = craftW - 8.0f * g;
      const bool hover = m.x >= cx && m.x < cx + rw && m.y >= ry && m.y < ry + slot;
      ui.rect(cx, ry, rw, slot, hover && can ? rgba(70, 90, 110, 230) : rgba(36, 40, 50, 220));
      ui.icon(r.output.id, cx + g, ry + g, slot - 2.0f * g);
      ui.text(cx + slot, ry + 5.0f * g, "x" + std::to_string(r.output.count), can ? rgba(240, 240, 240) : rgba(120, 120, 120), g);
      float ix = cx + slot + 18.0f * g;
      for (const ItemStack& in : r.inputs) {
        ui.icon(in.id, ix, ry + 2.0f * g, slot - 4.0f * g);
        ui.text(ix + slot - 4.0f * g, ry + 5.0f * g, std::to_string(in.count), can ? rgba(220, 220, 220) : rgba(120, 120, 120), g);
        ix += slot + 10.0f * g;
      }
      if (hover) {
        tooltip = std::string(itemInfo(r.output.id).name) + (can ? " (click to craft)" : " (missing ingredients)");
        if (left && can) {
          clickedSomething = true;
          craft(s.inventory, r);
        }
      }
      ry += slot + 2.0f * g;
    }
  }

  // Clicking outside every panel drops the carried stack.
  const bool insidePanels = m.x >= px && m.x < px + invW + craftW && m.y >= py && m.y < py + panelH;
  if ((left || right) && !clickedSomething && !insidePanels && !s.cursor.empty()) {
    if (!creative) {
      const vec3 dir = s.player.lookDirection();
      spawnItem(s.cursor.id, s.cursor.count, s.player.eyePosition(1.0) - dvec3(0, 0.3, 0), dvec3(dir) * 4.0);
      s.items.back().pickupDelay = 1.5f;
    }
    s.cursor = ItemStack{};
  }
  if (!s.cursor.empty()) {
    ui.icon(s.cursor.id, m.x - slot * 0.4f, m.y - slot * 0.4f, slot * 0.8f);
    if (s.cursor.count > 1) ui.text(m.x + slot * 0.2f, m.y + slot * 0.1f, std::to_string(s.cursor.count), rgba(255, 255, 255), g);
  } else if (!tooltip.empty()) {
    const float tw = ui.textWidth(tooltip, g);
    ui.rect(m.x + 8.0f * g, m.y - 12.0f * g, tw + 6.0f * g, 11.0f * g, rgba(16, 10, 30, 235));
    ui.text(m.x + 11.0f * g, m.y - 10.0f * g, tooltip, rgba(240, 240, 240), g);
  }
  // Close button in the panel's corner (the way out on a touch screen).
  const float cb = 12.0f * g;
  if (button(px + invW - cb - 2.0f * g, py + 2.0f * g, cb, cb, "x")) {
    setScreen(Screen::Playing);
    return;
  }
  if (input_.pressed(GLFW_KEY_ESCAPE) || input_.pressed(GLFW_KEY_E)) setScreen(Screen::Playing);
}

// ---------------------------------------------------------------------------
// Chat and commands
// ---------------------------------------------------------------------------
void Game::drawChat() {
  UiRenderer& ui = renderer_.ui();
  const float g = guiScale();
  const float W = static_cast<float>(fbWidth_), H = static_cast<float>(fbHeight_);
  drawMessages(g, true);
  for (char c : input_.typed())
    if (chatText_.size() < 200) chatText_.push_back(c);
  if (input_.repeated(GLFW_KEY_BACKSPACE) && !chatText_.empty()) chatText_.pop_back();
  if (input_.pressed(GLFW_KEY_UP) && !chatHistory_.empty()) {
    chatHistoryIndex_ = chatHistoryIndex_ < 0 ? static_cast<int>(chatHistory_.size()) - 1 : std::max(0, chatHistoryIndex_ - 1);
    chatText_ = chatHistory_[static_cast<size_t>(chatHistoryIndex_)];
  }
  if (input_.pressed(GLFW_KEY_DOWN) && chatHistoryIndex_ >= 0) {
    ++chatHistoryIndex_;
    if (chatHistoryIndex_ >= static_cast<int>(chatHistory_.size())) {
      chatHistoryIndex_ = -1;
      chatText_.clear();
    } else {
      chatText_ = chatHistory_[static_cast<size_t>(chatHistoryIndex_)];
    }
  }
  ui.rect(2.0f * g, H - 26.0f * g, W - 4.0f * g, 12.0f * g, rgba(0, 0, 0, 170));
  const float tw = ui.text(4.0f * g, H - 24.0f * g, chatText_, rgba(255, 255, 255), g);
  if (std::fmod(time_, 1.0) < 0.55) ui.rect(5.0f * g + tw, H - 24.0f * g, g, 8.0f * g, rgba(255, 255, 255));
  if (input_.pressed(GLFW_KEY_ESCAPE)) {
    setScreen(Screen::Playing);
    return;
  }
  if (input_.pressed(GLFW_KEY_ENTER) || input_.pressed(GLFW_KEY_KP_ENTER)) {
    const std::string text = chatText_;
    chatText_.clear();
    setScreen(Screen::Playing);
    if (!text.empty()) {
      chatHistory_.push_back(text);
      if (text[0] == '/') {
        if (!executeCommand(text.substr(1))) message("Unknown command. Type /help for a list.");
      } else {
        message("<you> " + text);
      }
    }
  }
}

void Game::queueCommands(const std::string& script) {
  std::string cur;
  for (char c : script) {
    if (c == ';' || c == '\n') {
      if (!cur.empty()) commandQueue_.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) commandQueue_.push_back(cur);
}

void Game::runCommandQueue() {
  if (waitFrames_ > 0) {
    --waitFrames_;
    return;
  }
  if (waitChunks_) {
    if (!session_ || !session_->ready) return;
    const WorldStats st = session_->world->stats();
    if (st.genInFlight || st.lightInFlight || st.meshInFlight) return;
    const int rd = settings_.renderDistance;
    const int cx = floorDiv(fastFloor(session_->player.pos.x), 16);
    const int cz = floorDiv(fastFloor(session_->player.pos.z), 16);
    for (int dz = -rd; dz <= rd; ++dz)
      for (int dx = -rd; dx <= rd; ++dx)
        if (dx * dx + dz * dz <= rd * rd && !session_->world->isChunkRendered(cx + dx, cz + dz)) return;
    waitChunks_ = false;
    waitFrames_ = 3;  // let streamed meshes reach the GPU
    return;
  }
  while (!commandQueue_.empty()) {
    std::string cmd = commandQueue_.front();
    commandQueue_.pop_front();
    while (!cmd.empty() && (cmd[0] == ' ' || cmd[0] == '/')) cmd.erase(cmd.begin());
    if (cmd.empty()) continue;
    if (!executeCommand(cmd)) message("Unknown command: " + cmd);
    if (waitFrames_ > 0 || waitChunks_ || !pendingScreenshot_.empty()) break;
  }
}

bool Game::executeCommand(const std::string& line) {
  const std::vector<std::string> a = split(line);
  if (a.empty()) return true;
  const std::string cmd = lower(a[0]);
  if (cmd == "quit" || cmd == "exit") {
    quit_ = true;
    return true;
  }
  if (cmd == "wait") {
    waitFrames_ = a.size() > 1 ? std::max(0, std::atoi(a[1].c_str())) : 1;
    return true;
  }
  if (cmd == "waitchunks") {
    waitChunks_ = true;
    return true;
  }
  if (cmd == "fps") {
    char buf[200];
    const RenderStats& rs = renderer_.stats();
    std::snprintf(buf, sizeof(buf), "%.1f fps (%.1f ms/frame), %d/%d sections drawn, %d draw calls", fps_, frameMs_,
                  rs.sectionsVisible, rs.sectionsTotal, rs.drawCalls);
    message(buf);
    return true;
  }
  if (cmd == "screenshot") {
    pendingScreenshot_ = a.size() > 1 ? a[1] : screenshotPath();
    return true;
  }
  if (cmd == "help") {
    message("Commands: tp, time, gamemode, give, fill, setblock, spawn, setspawn, heal, kill, fly, rd, fov,");
    message("  look, select, use, break, seed, pos, save, hud, debug, ui, settings, touch, fps, screenshot, wait,");
    message("  waitchunks, quit");
    return true;
  }
  if (cmd == "settings" && a.size() >= 3) {
    const std::string key = lower(a[1]);
    const double v = std::atof(a[2].c_str());
    const bool on = lower(a[2]) == "on" || lower(a[2]) == "true" || a[2] == "1";
    if (key == "shadows") settings_.shadowQuality = static_cast<int>(v);
    else if (key == "clouds") settings_.cloudQuality = static_cast<int>(v);
    else if (key == "reflections") settings_.reflections = on;
    else if (key == "bloom") settings_.bloom = on;
    else if (key == "antialiasing") settings_.antialiasing = on;
    else if (key == "brightness") settings_.brightness = static_cast<float>(v);
    else if (key == "fov") settings_.fov = static_cast<float>(v);
    else if (key == "render_distance") settings_.renderDistance = static_cast<int>(v);
    else if (key == "view_bobbing") settings_.viewBobbing = on;
    else if (key == "render_scale") settings_.renderScale = static_cast<float>(v);
    else if (key == "touch_sensitivity") settings_.touchSensitivity = static_cast<float>(v);
    else if (key == "fog") debugFog_ = on;
    else return false;
    applySettings();
    return true;
  }
  if (cmd == "touch") {
    // Show (or hide) the touch screen controls regardless of the input device.
    const bool on = a.size() < 2 ? !input_.usingTouch() : (lower(a[1]) == "on" || a[1] == "1");
    input_.setUsingTouch(on);
    setScreen(screen_);  // re-evaluates pointer capture
    message(on ? "Touch controls on" : "Touch controls off");
    return true;
  }
  if (!session_) return cmd == "hud" || cmd == "debug";
  Session& s = *session_;
  Player& p = s.player;
  World& w = *s.world;
  if (cmd == "tp" && a.size() >= 4) {
    double x, y, z;
    if (!parseCoord(a[1], p.pos.x, x) || !parseCoord(a[2], p.pos.y, y) || !parseCoord(a[3], p.pos.z, z)) return false;
    p.teleport(dvec3(x, y, z));
    s.needsUnstuck = true;
    message("Teleported");
    return true;
  }
  if (cmd == "look" && a.size() >= 3) {
    p.yaw = radians(static_cast<float>(std::atof(a[1].c_str())));
    p.pitch = clampv(radians(static_cast<float>(std::atof(a[2].c_str()))), -radians(89.5f), radians(89.5f));
    return true;
  }
  if (cmd == "time" && a.size() >= 3) {
    const std::string v = lower(a[2]);
    int64_t t = 0;
    if (v == "sunrise" || v == "dawn") t = 0;
    else if (v == "day" || v == "morning") t = 1000;
    else if (v == "noon") t = 6000;
    else if (v == "afternoon") t = 9000;
    else if (v == "sunset" || v == "dusk") t = 12000;
    else if (v == "night") t = 14000;
    else if (v == "midnight") t = 18000;
    else t = std::atoll(v.c_str());
    if (lower(a[1]) == "add") s.time += t;
    else s.time = (s.time / kDayTicks) * kDayTicks + floorMod(static_cast<int>(t % kDayTicks), static_cast<int>(kDayTicks));
    message("Time is now " + formatTime(s.time));
    return true;
  }
  if (cmd == "gamemode" && a.size() >= 2) {
    const std::string m = lower(a[1]);
    p.creative = m == "creative" || m == "c" || m == "1";
    if (!p.creative) p.flying = false;
    message(std::string("Game mode: ") + (p.creative ? "creative" : "survival"));
    return true;
  }
  if (cmd == "give" && a.size() >= 2) {
    const ItemId id = findItem(a[1]);
    if (!isValidItem(id)) {
      message("Unknown item: " + a[1]);
      return true;
    }
    const int count = a.size() >= 3 ? std::max(1, std::atoi(a[2].c_str())) : 1;
    const int left = s.inventory.add(id, count);
    message("Gave " + std::to_string(count - left) + " " + itemInfo(id).name);
    return true;
  }
  if ((cmd == "setblock" && a.size() >= 5) || (cmd == "fill" && a.size() >= 8)) {
    const bool fill = cmd == "fill";
    double c[6];
    const dvec3 base = p.pos;
    const double bases[3] = {base.x, base.y, base.z};
    for (int i = 0; i < (fill ? 6 : 3); ++i)
      if (!parseCoord(a[static_cast<size_t>(i + 1)], bases[i % 3], c[i])) return false;
    const std::string name = a[fill ? 7 : 4];
    const ItemId id = lower(name) == "air" ? static_cast<ItemId>(block::Air) : findItem(name);
    if (id != block::Air && (!isValidItem(id) || id >= 256)) {
      message("Unknown block: " + name);
      return true;
    }
    int x0 = fastFloor(c[0]), y0 = fastFloor(c[1]), z0 = fastFloor(c[2]);
    int x1 = fill ? fastFloor(c[3]) : x0, y1 = fill ? fastFloor(c[4]) : y0, z1 = fill ? fastFloor(c[5]) : z0;
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    if (z0 > z1) std::swap(z0, z1);
    const long long volume = static_cast<long long>(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1);
    if (volume > 65536) {
      message("Too many blocks (" + std::to_string(volume) + "), the limit is 65536");
      return true;
    }
    int changed = 0;
    for (int y = y0; y <= y1; ++y)
      for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x)
          if (w.setBlock(x, y, z, static_cast<BlockId>(id), 0, World::kNotify)) ++changed;
    message("Changed " + std::to_string(changed) + " block(s)");
    return true;
  }
  if (cmd == "spawn") {
    p.teleport(dvec3(s.spawn.x + 0.5, s.spawn.y, s.spawn.z + 0.5));
    p.unstuck(w);
    return true;
  }
  if (cmd == "setspawn") {
    s.spawn = ivec3(fastFloor(p.pos.x), fastFloor(p.pos.y), fastFloor(p.pos.z));
    message("Spawn point set");
    return true;
  }
  if (cmd == "heal") {
    p.health = Player::kMaxHealth;
    p.air = Player::kMaxAir;
    return true;
  }
  if (cmd == "kill") {
    const bool wasCreative = p.creative;
    p.creative = false;
    p.damage(1000.0f);
    p.creative = wasCreative;
    return true;
  }
  if (cmd == "fly") {
    if (a.size() >= 2) p.flying = lower(a[1]) == "on" || a[1] == "1" || lower(a[1]) == "true";
    else p.flying = !p.flying;
    message(p.flying ? "Flying" : "Walking");
    return true;
  }
  if ((cmd == "rd" || cmd == "renderdistance") && a.size() >= 2) {
    settings_.renderDistance = std::atoi(a[1].c_str());
    applySettings();
    message("Render distance " + std::to_string(settings_.renderDistance));
    return true;
  }
  if (cmd == "fov" && a.size() >= 2) {
    settings_.fov = static_cast<float>(std::atof(a[1].c_str()));
    applySettings();
    return true;
  }
  if (cmd == "select" && a.size() >= 2) {
    s.selected = clampv(std::atoi(a[1].c_str()) - 1, 0, Inventory::kHotbar - 1);
    return true;
  }
  if (cmd == "use" || cmd == "break") {
    s.target = raycastBlocks(p.eyePosition(1.0), p.lookDirection(), p.creative ? 5.5f : 4.6f,
                             [&w](int x, int y, int z) { return w.getBlock(x, y, z); });
    if (cmd == "use") placeBlock();
    else if (s.target.hit) breakBlock(s.target.block);
    return true;
  }
  if (cmd == "seed") {
    message("Seed: " + std::to_string(w.seed()));
    return true;
  }
  if (cmd == "pos") {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Position %.2f %.2f %.2f", p.pos.x, p.pos.y, p.pos.z);
    message(buf);
    return true;
  }
  if (cmd == "save") {
    saveSession();
    message("World saved");
    return true;
  }
  if (cmd == "ui" && a.size() >= 2) {
    const std::string n = lower(a[1]);
    if (n == "inventory") setScreen(Screen::Inventory);
    else if (n == "pause") setScreen(Screen::Paused);
    else if (n == "settings") {
      settingsReturn_ = Screen::Paused;
      setScreen(Screen::Settings);
    } else if (n == "play") setScreen(Screen::Playing);
    else return false;
    return true;
  }
  if (cmd == "hud") {
    hideHud_ = a.size() >= 2 && lower(a[1]) == "off";
    return true;
  }
  if (cmd == "debug") {
    showDebug_ = !(a.size() >= 2 && lower(a[1]) == "off");
    return true;
  }
  return false;
}

}  // namespace strata
