#include "game/game.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>  // key codes (the desktop window lives in platform/)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include "core/log.hpp"
#include "gl/gl.hpp"
#include "world/mesher.hpp"

namespace strata {

namespace fs = std::filesystem;

namespace {
constexpr double kStep = 1.0 / 60.0;
constexpr int64_t kDayTicks = 24000;
}  // namespace

Game::Game() : rng_(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {}

Game::~Game() { finish(); }

// ---------------------------------------------------------------------------
// Window and lifecycle
// ---------------------------------------------------------------------------
bool Game::initWindow(const GameOptions& o) {
  if (!window_.create(o.width, o.height, settings_.vsync, input_)) return false;
  window_.framebufferSize(fbWidth_, fbHeight_);
  RenderSettings rs;
  rs.shadowQuality = settings_.shadowQuality;
  rs.cloudQuality = settings_.cloudQuality;
  rs.reflections = settings_.reflections;
  rs.bloom = settings_.bloom;
  rs.antialiasing = settings_.antialiasing;
  rs.brightness = settings_.brightness;
  rs.renderScale = settings_.renderScale;
  if (!renderer_.init(fbWidth_, fbHeight_, rs)) {
    LOG_ERROR("renderer initialisation failed");
    return false;
  }
  if (o.fullscreen || settings_.fullscreen) setFullscreen(true);
  return true;
}

void Game::shutdownWindow() {
  if (!window_.valid()) return;
  renderer_.shutdown();
  window_.destroy();
}

void Game::applySettings() {
  settings_.clampValues();
  RenderSettings rs = renderer_.settings();
  rs.shadowQuality = settings_.shadowQuality;
  rs.cloudQuality = settings_.cloudQuality;
  rs.reflections = settings_.reflections;
  rs.bloom = settings_.bloom;
  rs.antialiasing = settings_.antialiasing;
  rs.brightness = settings_.brightness;
  rs.renderScale = settings_.renderScale;
  rs.fog = debugFog_;
  renderer_.setSettings(rs);
  window_.setVsync(settings_.vsync);
}

void Game::setFullscreen(bool on) {
  if (!window_.valid()) return;
  window_.setFullscreen(on);
  settings_.fullscreen = on;
}

void Game::persist() { Window::persistStorage(); }

int Game::run(const GameOptions& o) {
  options_ = o;
  if (!settings_.load(o.settingsPath) && Window::hasTouchScreen()) {
    // First start on a phone or tablet: lighter defaults.
    settings_.applyMobileDefaults();
    input_.setUsingTouch(true);
  }
  if (Window::hasTouchScreen()) input_.setUsingTouch(true);
  if (o.renderDistance > 0) settings_.renderDistance = o.renderDistance;
  if (o.shadows >= 0) settings_.shadowQuality = o.shadows;
  if (o.clouds >= 0) settings_.cloudQuality = o.clouds;
  if (o.renderScale > 0.0f) settings_.renderScale = o.renderScale;
  if (o.vsync >= 0) settings_.vsync = o.vsync == 1;
  settings_.clampValues();
  if (!initWindow(o)) return 1;
  if (!o.exec.empty()) queueCommands(o.exec);
  if (!o.world.empty()) {
    const bool exists = fs::exists(fs::path(o.savesDir) / o.world / "level.txt");
    uint64_t seed = o.hasSeed ? o.seed : splitmix64(static_cast<uint64_t>(std::time(nullptr)));
    startSession(o.world, !exists, seed, o.creative == 1);
  } else {
    setScreen(Screen::MainMenu);
  }
  lastTime_ = Window::now();
#if defined(__EMSCRIPTEN__)
  // The browser calls back once per display frame; run() returns at once and
  // the game lives on until the page is closed.
  emscripten_set_main_loop_arg(
      [](void* game) {
        Game* g = static_cast<Game*>(game);
        if (!g->loopIteration()) {
          emscripten_cancel_main_loop();
          g->finish();
        }
      },
      this, 0, 0);
#else
  while (loopIteration()) {
  }
  finish();
#endif
  return 0;
}

bool Game::loopIteration() {
  if (quit_ || window_.closeRequested()) return false;
  window_.pollEvents();
  const double now = Window::now();
  const double dt = std::min(now - lastTime_, 0.25);
  lastTime_ = now;
  time_ += dt;
  fpsTimer_ += dt;
  ++fpsFrames_;
  if (fpsTimer_ >= 0.5) {
    fps_ = fpsFrames_ / fpsTimer_;
    frameMs_ = fpsTimer_ * 1000.0 / fpsFrames_;
    fpsTimer_ = 0.0;
    fpsFrames_ = 0;
  }
  frame(dt);
  window_.swapBuffers();
  input_.endFrame();
  ++frameCount_;
  if (options_.maxFrames > 0 && frameCount_ >= static_cast<uint64_t>(options_.maxFrames)) quit_ = true;
  return true;
}

void Game::finish() {
  if (finished_) return;
  finished_ = true;
  endSession();
  if (window_.valid()) settings_.save(options_.settingsPath);
  persist();
  shutdownWindow();
}

void Game::setScreen(Screen s) {
  if ((s == Screen::Chat || s == Screen::CreateWorld) && screen_ != s) focusedField_ = s == Screen::Chat ? 0 : 1;
  if (screen_ == Screen::Inventory && s != Screen::Inventory && session_ && !session_->cursor.empty()) {
    // Never lose the stack carried by the mouse.
    const int left = session_->inventory.add(session_->cursor.id, session_->cursor.count);
    if (left > 0 && !session_->player.creative)
      spawnItem(session_->cursor.id, left, session_->player.eyePosition(1.0), dvec3());
    session_->cursor = ItemStack{};
  }
  screen_ = s;
  input_.consumePresses();
  // Fingers on the game controls or on the menu belong to the old screen.
  touch_.releaseAll();
  if (menuFinger_ >= 0) {
    input_.onButton(GLFW_MOUSE_BUTTON_LEFT, Input::kRelease, true);
    menuFinger_ = -1;
  }
  // Touch screens never capture the pointer; the mouse does while playing.
  const bool capture = s == Screen::Playing && !input_.usingTouch();
  input_.setCursorCaptured(capture);
  window_.setCursorCaptured(capture);
  if (s == Screen::MainMenu) {
    // Refresh the list of saved worlds, most recently played first.
    worldList_.clear();
    std::vector<std::pair<fs::file_time_type, std::string>> found;
    std::error_code ec;
    if (fs::is_directory(options_.savesDir, ec))
      for (const auto& e : fs::directory_iterator(options_.savesDir, ec)) {
        const fs::path level = e.path() / "level.txt";
        if (e.is_directory(ec) && fs::exists(level, ec)) found.emplace_back(fs::last_write_time(level, ec), e.path().filename().string());
      }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& f : found) worldList_.emplace_back(f.second, f.second);
  }
}

void Game::startSession(const std::string& name, bool create, uint64_t seed, bool creative) {
  endSession();
  auto s = std::make_unique<Session>();
  s->name = name;
  s->dir = fs::path(options_.savesDir) / name;
  LevelData level;
  bool loaded = false;
  {
    WorldStorage probe(s->dir);
    loaded = !create && probe.loadLevel(level);
  }
  if (!loaded) {
    level = LevelData{};
    level.seed = seed;
    level.creative = creative;
  }
  if (options_.creative >= 0) level.creative = options_.creative == 1;
  unsigned threads = settings_.workerThreads > 0 ? static_cast<unsigned>(settings_.workerThreads)
                                                  : std::max(1u, std::thread::hardware_concurrency() - 1);
  threads = std::min(threads, 8u);
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
  threads = std::min(threads, 4u);  // the page pre-starts four workers
#elif defined(__EMSCRIPTEN__)
  threads = 0;  // no threads in this build: the world runs its jobs between frames
#endif
  s->world = std::make_unique<World>(level.seed, s->dir, threads);
  s->level = level;
  s->time = level.worldTime;
  s->inventory = level.inventory;
  s->selected = level.selectedSlot;
  s->player.creative = level.creative;
  s->spawn = level.hasSpawn ? level.spawn : s->world->generator().findSpawn();
  if (level.hasPlayer) {
    s->player.teleport(level.playerPos);
    s->player.yaw = level.yaw;
    s->player.pitch = level.pitch;
    s->player.flying = level.flying && level.creative;
    s->player.health = level.health;
  } else {
    s->player.teleport(dvec3(s->spawn.x + 0.5, s->spawn.y, s->spawn.z + 0.5));
    s->time = 1000;  // start in the morning
  }
  if (level.creative) {
    bool empty = true;
    for (int i = 0; i < Inventory::kHotbar; ++i) empty = empty && s->inventory.slots[i].empty();
    if (empty) {
      const BlockId starter[9] = {block::Grass,  block::Dirt,  block::Stone,  block::Cobblestone, block::Planks,
                                  block::OakLog, block::Glass, block::Torch, block::Bricks};
      for (int i = 0; i < 9; ++i) s->inventory.slots[i] = {starter[i], 64};
    }
  }
  LOG_INFO("%s world '%s' (seed %llu)", loaded ? "loaded" : "created", name.c_str(),
           static_cast<unsigned long long>(level.seed));
  s->startTime = time_;
  session_ = std::move(s);
  saveSession();
  setScreen(Screen::Loading);
}

void Game::saveSession() {
  if (!session_) return;
  Session& s = *session_;
  LevelData& l = s.level;
  l.worldTime = s.time;
  l.creative = s.player.creative;
  l.hasPlayer = true;
  l.playerPos = s.player.pos;
  l.yaw = s.player.yaw;
  l.pitch = s.player.pitch;
  l.flying = s.player.flying;
  l.health = s.player.dead ? Player::kMaxHealth : s.player.health;
  l.hasSpawn = true;
  l.spawn = s.spawn;
  l.selectedSlot = s.selected;
  l.inventory = s.inventory;
  if (!s.cursor.empty()) l.inventory.add(s.cursor.id, s.cursor.count);
  s.world->storage().saveLevel(l);
  s.world->saveAll();
  persist();
}

void Game::endSession() {
  if (!session_) return;
  saveSession();
  renderer_.clearMeshes();
  session_.reset();
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void Game::frame(double dt) {
  runCommandQueue();
  processTouches();
  for (Message& m : messages_) m.age += static_cast<float>(dt);
  while (messages_.size() > 50) messages_.pop_front();

  if (input_.pressed(GLFW_KEY_F11)) setFullscreen(!settings_.fullscreen);
  const bool scripted = !commandQueue_.empty() || waitFrames_ > 0 || waitChunks_;
  // Switching apps or tabs may be the last thing that happens to a page.
  if (!input_.focused() && wasFocused_ && session_ && session_->ready) saveSession();
  wasFocused_ = input_.focused();
  // Losing focus, or the browser releasing the pointer (Esc), pauses the game.
  if (input_.takeCaptureLost() && screen_ == Screen::Playing && !scripted) setScreen(Screen::Paused);
  if (!input_.focused() && screen_ == Screen::Playing && !scripted) setScreen(Screen::Paused);
  // STRATA_PROFILE=1 logs how long each part of a frame takes (GPU included).
  static const bool profile = std::getenv("STRATA_PROFILE") != nullptr;
  const double t0 = Window::now();
  if (session_) updateSession(dt);
  const double t1 = Window::now();
  render(dt);
  if (profile) {
    glFinish();
    const double t2 = Window::now();
    const WorldStats st = session_ ? session_->world->stats() : WorldStats{};
    LOG_INFO("frame %llu: update %.1f ms, render %.1f ms, chunks %zu (gen %zu light %zu mesh %zu in flight)",
             static_cast<unsigned long long>(frameCount_), (t1 - t0) * 1000.0, (t2 - t1) * 1000.0, st.chunks,
             st.genInFlight, st.lightInFlight, st.meshInFlight);
  }
}

bool Game::spawnAreaReady() const {
  if (!session_) return false;
  const World& w = *session_->world;
  const int cx = floorDiv(fastFloor(session_->player.pos.x), 16);
  const int cz = floorDiv(fastFloor(session_->player.pos.z), 16);
  for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx)
      if (!w.isChunkRendered(cx + dx, cz + dz) || !w.isEditable(cx + dx, cz + dz)) return false;
  return true;
}

bool Game::playerAreaReady() const {
  // Every chunk the player's box touches must be simulated, otherwise unloaded
  // space (which counts as solid) would act like invisible walls.
  const World& w = *session_->world;
  const Aabb b = session_->player.box();
  return w.isBlockEditable(fastFloor(b.min.x), fastFloor(b.min.z)) &&
         w.isBlockEditable(fastFloor(b.max.x), fastFloor(b.min.z)) &&
         w.isBlockEditable(fastFloor(b.min.x), fastFloor(b.max.z)) &&
         w.isBlockEditable(fastFloor(b.max.x), fastFloor(b.max.z));
}

void Game::updateSession(double dt) {
  Session& s = *session_;
  World& w = *s.world;
  w.setCenter(s.player.pos, settings_.renderDistance);
  // Time for world jobs on this thread when there are no workers: generous
  // while the loading screen is up, small enough to keep frames smooth after.
  w.update(s.ready ? 7.0 : 28.0);
  std::vector<SectionMeshUpdate> updates = w.takeMeshUpdates();
  renderer_.uploadMeshes(updates);
  if (!s.ready) {
    if (spawnAreaReady()) {
      s.player.unstuck(w);
      s.ready = true;
      LOG_INFO("spawn area ready after %.1f s", time_ - s.startTime);
      if (screen_ == Screen::Loading) setScreen(Screen::Playing);
    }
    return;
  }

  const bool paused = screen_ == Screen::Paused || screen_ == Screen::Settings;
  if (screen_ == Screen::Playing) updatePlaying(dt);
  if (!paused) {
    PlayerControls c;
    if (screen_ == Screen::Playing) {
      // Keyboard and the touch screen controls both steer (a tablet may have
      // a keyboard attached).
      const TouchControls::Output& t = touch_.output();
      const float keyForward = (input_.down(GLFW_KEY_W) ? 1.0f : 0.0f) - (input_.down(GLFW_KEY_S) ? 1.0f : 0.0f);
      const float keyStrafe = (input_.down(GLFW_KEY_D) ? 1.0f : 0.0f) - (input_.down(GLFW_KEY_A) ? 1.0f : 0.0f);
      c.forward = clampv(keyForward + t.forward, -1.0f, 1.0f);
      c.strafe = clampv(keyStrafe + t.strafe, -1.0f, 1.0f);
      c.jump = input_.down(GLFW_KEY_SPACE) || t.jump;
      c.sneak = input_.down(GLFW_KEY_LEFT_SHIFT) || input_.down(GLFW_KEY_RIGHT_SHIFT) || t.sneak;
      c.sprint = input_.down(GLFW_KEY_LEFT_CONTROL) || input_.down(GLFW_KEY_RIGHT_CONTROL) || t.sprint || s.sprintTap;
    }
    // Freeze physics while the player's surroundings are still streaming in.
    const bool areaReady = playerAreaReady();
    if (areaReady && (s.needsUnstuck || s.player.collides(w, s.player.box()))) {
      // After a teleport, or when a block (e.g. falling sand) ends up inside
      // the player, move up to the first free space.
      s.player.unstuck(w);
      s.needsUnstuck = false;
    }
    s.accumulator += dt;
    int steps = 0;
    while (s.accumulator >= kStep && steps < 15) {
      if (areaReady) s.player.tick(w, c, kStep);
      else s.player.prevPos = s.player.pos;
      w.updateEntities(static_cast<float>(kStep));
      updateItems(kStep);
      if (++s.tickPhase >= 3) {
        s.tickPhase = 0;
        w.tick();
        ++s.time;
      }
      s.accumulator -= kStep;
      ++steps;
    }
    if (steps == 15) s.accumulator = 0.0;
    // A double tap only has to start sprinting (the player keeps sprinting while
    // moving forward); keep it until a physics step has seen it.
    if (steps > 0) s.sprintTap = false;
    updateParticles(dt);
  }
  processChanges();
  if (s.player.dead && screen_ != Screen::Dead) setScreen(Screen::Dead);
  s.autosave += dt;
  // A browser tab can be closed or discarded at any moment, so save more often there.
  if (s.autosave > (Window::canQuit() ? 120.0 : 30.0)) {
    s.autosave = 0.0;
    saveSession();
  }
}

void Game::updatePlaying(double dt) {
  Session& s = *session_;
  Player& p = s.player;
  const TouchControls::Output& t = touch_.output();
  // A browser may refuse or drop the pointer lock: clicking into the game
  // captures it again (that click does nothing else).
  if (!input_.usingTouch() && !window_.cursorCaptured() && input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
    input_.setCursorCaptured(true);
    window_.setCursorCaptured(true);
    input_.consumePresses();
  }
  // Mouse look (only with a captured pointer), plus touch screen drags.
  const vec2 d = window_.cursorCaptured() ? input_.mouseDelta() : vec2(0.0f, 0.0f);
  const float sens = 0.0022f * settings_.mouseSensitivity;
  const float touchSens = 0.0052f * settings_.touchSensitivity;  // radians per CSS pixel
  p.yaw += d.x * sens + t.look.x * touchSens;
  p.pitch += (settings_.invertMouse ? d.y : -d.y) * sens + (settings_.invertMouse ? t.look.y : -t.look.y) * touchSens;
  p.pitch = clampv(p.pitch, -radians(89.5f), radians(89.5f));
  if (p.yaw > kPi) p.yaw -= kTwoPi;
  if (p.yaw < -kPi) p.yaw += kTwoPi;

  // Touch screen buttons.
  if (t.hotbarSlot >= 0) {
    s.selected = t.hotbarSlot;
    s.selectionTimer = 2.0f;
  }
  if (t.toggleFly && p.creative) {
    p.flying = !p.flying;
    if (p.flying) p.vel.y = std::max(p.vel.y, 0.0);
    message(p.flying ? "Flying" : "Walking");
  }
  if (t.pause) {
    setScreen(Screen::Paused);
    return;
  }
  if (t.inventory) {
    setScreen(Screen::Inventory);
    return;
  }
  if (t.chat) {
    promptChat();
    return;
  }

  // Double-tapping W sprints (Ctrl+W closes a browser tab, so Ctrl alone
  // is no use there).
  if (input_.pressed(GLFW_KEY_W)) {
    if (static_cast<float>(time_) - s.lastForwardTap < 0.3f) s.sprintTap = true;
    s.lastForwardTap = static_cast<float>(time_);
  }

  if ((input_.pressed(GLFW_KEY_SPACE) || t.jumpPressed) && p.creative) {
    if (static_cast<float>(time_) - s.lastJump < 0.3f) {
      p.flying = !p.flying;
      s.lastJump = -10.0f;
    } else {
      s.lastJump = static_cast<float>(time_);
    }
  }
  for (int k = 0; k < 9; ++k)
    if (input_.pressed(GLFW_KEY_1 + k)) {
      s.selected = k;
      s.selectionTimer = 2.0f;
    }
  const float wheel = input_.scroll();
  if (wheel != 0.0f) {
    s.selected = floorMod(s.selected - (wheel > 0.0f ? 1 : -1), Inventory::kHotbar);
    s.selectionTimer = 2.0f;
  }
  s.selectionTimer = std::max(0.0f, s.selectionTimer - static_cast<float>(dt));
  if (input_.pressed(GLFW_KEY_ESCAPE)) {
    setScreen(Screen::Paused);
    return;
  }
  if (input_.pressed(GLFW_KEY_E)) {
    setScreen(Screen::Inventory);
    return;
  }
  if (input_.pressed(GLFW_KEY_T) || input_.pressed(GLFW_KEY_SLASH) || input_.pressed(GLFW_KEY_ENTER)) {
    chatText_ = input_.pressed(GLFW_KEY_SLASH) ? "/" : "";
    chatHistoryIndex_ = -1;
    setScreen(Screen::Chat);
    return;
  }
  if (input_.pressed(GLFW_KEY_F1)) hideHud_ = !hideHud_;
  if (input_.pressed(GLFW_KEY_F2)) pendingScreenshot_ = screenshotPath();
  if (input_.pressed(GLFW_KEY_F3)) showDebug_ = !showDebug_;
  if (input_.pressed(GLFW_KEY_Q)) dropSelected();
  handleInteraction(dt);
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------
void Game::handleInteraction(double dt) {
  Session& s = *session_;
  World& w = *s.world;
  Player& p = s.player;
  const float fdt = static_cast<float>(dt);
  const float reach = p.creative ? 5.5f : 4.6f;
  // With touch controls the player aims with the finger, not a crosshair.
  const TouchControls::Output& t = touch_.output();
  vec3 dir = p.lookDirection();
  bool aiming = true;
  if (touchUi()) {
    aiming = t.aiming || t.use;
    if (aiming) dir = aimDirection(t.aim);
  }
  s.target = aiming ? raycastBlocks(p.eyePosition(1.0), dir, reach,
                                    [&w](int x, int y, int z) { return w.getBlock(x, y, z); })
                    : RayHit{};
  s.breakCooldown = std::max(0.0f, s.breakCooldown - fdt);
  s.useCooldown = std::max(0.0f, s.useCooldown - fdt);
  if (s.swinging) {
    s.swing += fdt * 3.5f;
    if (s.swing >= 1.0f) {
      s.swing = 0.0f;
      s.swinging = false;
    }
  }
  // Mouse buttons only count while the pointer is captured (a click that
  // captures it is not an attack).
  const bool mouse = window_.cursorCaptured();
  const bool lmbPressed = mouse && input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT);
  if (lmbPressed || t.use) s.swinging = true;

  const bool lmb = (mouse && input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT)) || t.breaking;
  if (p.creative) {
    s.isBreaking = false;
    if (s.target.hit && (lmbPressed || (lmb && s.breakCooldown <= 0.0f))) {
      breakBlock(s.target.block);
      s.breakCooldown = t.breaking ? 0.25f : 0.2f;
    }
  } else if (lmb && s.target.hit) {
    if (!s.isBreaking || s.breaking != s.target.block) {
      s.isBreaking = true;
      s.breaking = s.target.block;
      s.breakProgress = 0.0f;
    }
    const float hardness = blockInfo(s.target.id).hardness;
    if (hardness >= 0.0f) {
      s.breakProgress += fdt;
      if (!s.swinging) {
        s.swinging = true;
        s.swing = 0.0f;
      }
      if (s.breakProgress >= std::max(hardness, 0.05f) && s.breakCooldown <= 0.0f) {
        breakBlock(s.target.block);
        s.isBreaking = false;
        s.breakProgress = 0.0f;
        s.breakCooldown = 0.12f;
      }
    }
  } else {
    s.isBreaking = false;
    s.breakProgress = 0.0f;
  }
  if (t.use || (mouse && (input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT) ||
                          (input_.mouseDown(GLFW_MOUSE_BUTTON_RIGHT) && s.useCooldown <= 0.0f)))) {
    placeBlock();
    s.useCooldown = 0.22f;
  }
  if (mouse && input_.mousePressed(GLFW_MOUSE_BUTTON_MIDDLE)) pickBlock();
}

void Game::breakBlock(const ivec3& b) {
  Session& s = *session_;
  const BlockId id = s.world->getBlock(b.x, b.y, b.z);
  if (id == block::Air || isLiquid(id)) return;
  if (!s.player.creative && blockInfo(id).hardness < 0.0f) return;
  s.world->setBlock(b.x, b.y, b.z, block::Air, 0, World::kNotify | World::kSyncMesh | World::kByPlayer);
  s.swinging = true;
}

void Game::placeBlock() {
  Session& s = *session_;
  World& w = *s.world;
  if (!s.target.hit) return;
  ItemStack& stack = s.inventory.slots[s.selected];
  if (stack.empty()) return;
  const ItemInfo info = itemInfo(stack.id);
  if (!info.isBlock) return;
  const BlockId place = info.block;
  ivec3 t = s.target.block;
  if (!blockInfo(s.target.id).replaceable) {
    if (s.target.face < 0) return;
    t = t + ivec3(kFaceDir[s.target.face][0], kFaceDir[s.target.face][1], kFaceDir[s.target.face][2]);
  }
  if (t.y < 0 || t.y >= kChunkHeight) return;
  const BlockId existing = w.getBlock(t.x, t.y, t.z);
  if (!blockInfo(existing).replaceable || existing == place) return;
  const BlockInfo& pi = blockInfo(place);
  if (pi.needsSupport && !canSurviveOn(place, w.getBlock(t.x, t.y - 1, t.z))) return;
  if (pi.solid) {
    const Aabb cell{dvec3(t.x, t.y, t.z), dvec3(t.x + 1, t.y + 1, t.z + 1)};
    if (cell.intersects(s.player.box())) return;
  }
  if (w.setBlock(t.x, t.y, t.z, place, 0, World::kNotify | World::kSyncMesh | World::kByPlayer)) {
    if (!s.player.creative) s.inventory.takeOne(s.selected);
    s.swinging = true;
    s.swing = 0.0f;
  }
}

void Game::pickBlock() {
  Session& s = *session_;
  if (!s.target.hit) return;
  BlockId id = s.target.id;
  if (id == block::Grass || id == block::SnowyGrass) id = s.player.creative ? id : static_cast<BlockId>(block::Dirt);
  if (!isValidItem(id) || !blockInfo(id).inCreative) return;
  for (int i = 0; i < Inventory::kHotbar; ++i)
    if (s.inventory.slots[i].id == id) {
      s.selected = i;
      s.selectionTimer = 2.0f;
      return;
    }
  if (s.player.creative) {
    s.inventory.slots[s.selected] = {id, 64};
  } else {
    for (int i = Inventory::kHotbar; i < Inventory::kSlots; ++i)
      if (s.inventory.slots[i].id == id) {
        std::swap(s.inventory.slots[i], s.inventory.slots[s.selected]);
        break;
      }
  }
  s.selectionTimer = 2.0f;
}

void Game::dropSelected() {
  Session& s = *session_;
  ItemStack& stack = s.inventory.slots[s.selected];
  if (stack.empty() || s.player.creative) return;
  const ItemId id = stack.id;
  s.inventory.takeOne(s.selected);
  const vec3 dir = s.player.lookDirection();
  spawnItem(id, 1, s.player.eyePosition(1.0) - dvec3(0, 0.3, 0), dvec3(dir) * 5.0 + dvec3(0, 1.5, 0));
  s.items.back().pickupDelay = 1.5f;
}

void Game::processChanges() {
  Session& s = *session_;
  for (const BlockChange& c : s.world->takeChanges()) {
    if (c.before == block::Air || isLiquid(c.before)) continue;
    if (c.after != block::Air) continue;  // placements
    spawnBreakParticles(c.before, c.pos);
    if (!s.player.creative) spawnDrops(c.before, c.pos);
  }
}

void Game::spawnDrops(BlockId b, const ivec3& p) {
  const BlockInfo& info = blockInfo(b);
  ItemId drop = info.drop;
  int count = info.dropCount;
  if (info.wave == Wave::Leaves && rng_.chance(0.08f)) {
    drop = item::Stick;
    count = 1;
  }
  if (!isValidItem(drop) || count <= 0) return;
  spawnItem(drop, count, dvec3(p.x + 0.5, p.y + 0.3, p.z + 0.5),
            dvec3(rng_.range(-1.2f, 1.2f), rng_.range(2.0f, 3.5f), rng_.range(-1.2f, 1.2f)));
}

void Game::spawnItem(ItemId item, int count, const dvec3& pos, const dvec3& vel) {
  if (!session_ || !isValidItem(item) || count <= 0) return;
  ItemEntity e;
  e.item = item;
  e.count = count;
  e.pos = pos;
  e.vel = vel;
  session_->items.push_back(e);
  if (session_->items.size() > 400) session_->items.erase(session_->items.begin());
}

void Game::spawnBreakParticles(BlockId b, const ivec3& p) {
  Session& s = *session_;
  const BlockInfo& info = blockInfo(b);
  const uint16_t texId = info.tex[FacePosZ];
  for (int i = 0; i < 26; ++i) {
    Particle q;
    q.pos = dvec3(p.x + rng_.range(0.15f, 0.85f), p.y + rng_.range(0.15f, 0.85f), p.z + rng_.range(0.15f, 0.85f));
    q.vel = vec3(rng_.range(-1.8f, 1.8f), rng_.range(0.5f, 3.5f), rng_.range(-1.8f, 1.8f));
    q.life = rng_.range(0.5f, 1.3f);
    q.tex = texId;
    q.u0 = rng_.range(0.0f, 0.75f);
    q.v0 = rng_.range(0.0f, 0.75f);
    q.size = rng_.range(0.045f, 0.085f);
    s.particles.push_back(q);
  }
  if (s.particles.size() > 2000) s.particles.erase(s.particles.begin(), s.particles.begin() + 500);
}

void Game::updateParticles(double dt) {
  Session& s = *session_;
  World& w = *s.world;
  const float fdt = static_cast<float>(dt);
  for (Particle& q : s.particles) {
    q.life -= fdt;
    if (q.resting) continue;
    q.vel.y -= 14.0f * fdt;
    const dvec3 np = q.pos + dvec3(q.vel) * dt;
    if (w.isSolidAt(fastFloor(np.x), fastFloor(np.y), fastFloor(np.z))) {
      q.resting = true;
      continue;
    }
    q.pos = np;
  }
  s.particles.erase(std::remove_if(s.particles.begin(), s.particles.end(), [](const Particle& q) { return q.life <= 0.0f; }),
                    s.particles.end());
}

void Game::updateItems(double dt) {
  Session& s = *session_;
  World& w = *s.world;
  const Player& p = s.player;
  auto solid = [&w](const dvec3& q) { return w.isSolidAt(fastFloor(q.x), fastFloor(q.y), fastFloor(q.z)); };
  for (ItemEntity& e : s.items) {
    e.age += static_cast<float>(dt);
    e.pickupDelay -= static_cast<float>(dt);
    if (solid(e.pos)) e.pos.y = std::floor(e.pos.y) + 1.0;  // pushed out by a placed block
    const dvec3 toPlayer = p.pos + dvec3(0, 0.8, 0) - e.pos;
    const double dist = length(toPlayer);
    if (!p.dead && e.pickupDelay <= 0.0f && dist < 1.8) {
      if (dist < 0.7) {
        e.count = s.inventory.add(e.item, e.count);
        continue;
      }
      e.vel = e.vel * 0.7 + toPlayer * (9.0 / dist) * 0.3;
      e.pos += e.vel * dt;
      continue;
    }
    const BlockId here = w.getBlock(fastFloor(e.pos.x), fastFloor(e.pos.y), fastFloor(e.pos.z));
    if (here == block::Water) {
      e.vel.y = e.vel.y * 0.9 + 3.0 * dt;
      e.vel.x *= 0.95;
      e.vel.z *= 0.95;
    } else {
      e.vel.y -= 18.0 * dt;
    }
    dvec3 q = e.pos;
    q.y += e.vel.y * dt;
    if (solid(q)) {
      if (e.vel.y < 0.0) q.y = std::floor(q.y) + 1.0;
      else q.y = e.pos.y;
      e.vel.y = 0.0;
      e.vel.x *= 0.6;
      e.vel.z *= 0.6;
    }
    q.x += e.vel.x * dt;
    if (solid(q)) {
      q.x = e.pos.x;
      e.vel.x = 0.0;
    }
    q.z += e.vel.z * dt;
    if (solid(q)) {
      q.z = e.pos.z;
      e.vel.z = 0.0;
    }
    e.pos = q;
  }
  s.items.erase(std::remove_if(s.items.begin(), s.items.end(),
                               [](const ItemEntity& e) { return e.count <= 0 || e.age > 300.0f || e.pos.y < -64.0; }),
                s.items.end());
}

void Game::respawn() {
  Session& s = *session_;
  s.player.dead = false;
  s.player.health = Player::kMaxHealth;
  s.player.air = Player::kMaxAir;
  s.player.teleport(dvec3(s.spawn.x + 0.5, s.spawn.y, s.spawn.z + 0.5));
  s.player.unstuck(*s.world);
  s.ready = false;  // wait for the spawn area again
  setScreen(Screen::Loading);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
float Game::guiScale() const {
  if (settings_.guiScale > 0) return static_cast<float>(settings_.guiScale);
  if (touchUi()) {
    // Touch screens: as large as the screen allows. Their pixels are tiny,
    // so a fractional scale does not look uneven.
    const float s = std::min(static_cast<float>(fbWidth_) / 400.0f, static_cast<float>(fbHeight_) / 280.0f);
    return clampv(s, 1.0f, 2.5f * window_.pixelRatio());
  }
  const int s = std::min(fbWidth_ / 400, fbHeight_ / 280);
  return static_cast<float>(clampv(s, 1, 4));
}

float Game::hudScale() const {
  const float g = guiScale();
  if (!touchUi()) return g;
  // Finger-sized hotbar slots (about 40 CSS pixels), as long as nine fit.
  const float want = 2.0f * window_.pixelRatio();
  const float fit = static_cast<float>(fbWidth_) / (9.0f * 20.0f + 12.0f);
  return std::max(g, std::min(want, fit));
}

// ---------------------------------------------------------------------------
// Touch screens
// ---------------------------------------------------------------------------
TouchControls::Layout Game::touchLayout() const {
  TouchControls::Layout l;
  l.width = static_cast<float>(fbWidth_);
  l.height = static_cast<float>(fbHeight_);
  l.unit = window_.pixelRatio();
  // Must match drawHotbar().
  const float g = hudScale();
  l.hotbarSlot = 20.0f * g;
  l.hotbarX = (l.width - l.hotbarSlot * 9.0f) * 0.5f;
  l.hotbarY = l.height - l.hotbarSlot - 4.0f * g;
  if (session_) {
    l.creative = session_->player.creative;
    l.flying = session_->player.flying;
  }
  return l;
}

void Game::processTouches() {
  const std::vector<Input::TouchEvent>& events = input_.touches();
  if (screen_ == Screen::Playing && session_ && session_->ready) {
    touch_.update(events, touchLayout(), time_);
    return;
  }
  // Menus: the first finger drives a virtual mouse, so a tap clicks and a
  // drag moves sliders.
  for (const Input::TouchEvent& e : events) {
    if (e.phase == Input::TouchPhase::Began) {
      if (menuFinger_ >= 0) continue;
      menuFinger_ = e.id;
      input_.onCursor(e.pos.x, e.pos.y, 1.0f, 1.0f, true);
      input_.onButton(GLFW_MOUSE_BUTTON_LEFT, Input::kPress, true);
    } else if (e.id == menuFinger_) {
      input_.onCursor(e.pos.x, e.pos.y, 1.0f, 1.0f, true);
      if (e.phase == Input::TouchPhase::Ended || e.phase == Input::TouchPhase::Cancelled) {
        input_.onButton(GLFW_MOUSE_BUTTON_LEFT, Input::kRelease, true);
        menuFinger_ = -1;
      }
    }
  }
}

vec3 Game::aimDirection(vec2 screen) const {
  const Player& p = session_->player;
  const vec3 f = p.lookDirection();
  const vec3 r(std::cos(p.yaw), 0.0f, std::sin(p.yaw));  // the camera's right
  const vec3 u = cross(r, f);
  const float tanHalf = std::tan(radians(lastFov_) * 0.5f);
  const float w = static_cast<float>(std::max(fbWidth_, 1)), h = static_cast<float>(std::max(fbHeight_, 1));
  const float x = (2.0f * screen.x / w - 1.0f) * tanHalf * (w / h);
  const float y = (1.0f - 2.0f * screen.y / h) * tanHalf;
  return normalize(f + r * x + u * y);
}

void Game::promptChat() {
  std::string text = "/";
  if (!Window::promptText("Chat message or /command (try /help)", text)) return;
  const size_t a = text.find_first_not_of(' ');
  if (a == std::string::npos || text == "/") return;
  text = text.substr(a);
  // Same as pressing Enter in the chat screen.
  chatHistory_.push_back(text);
  if (text[0] == '/') {
    if (!executeCommand(text.substr(1))) message("Unknown command. Type /help for a list.");
  } else {
    message("<you> " + text);
  }
}

void Game::buildFrameInput(FrameInput& fi, double alpha) {
  Session& s = *session_;
  World& w = *s.world;
  const Player& p = s.player;
  const float fdt = static_cast<float>(std::min(frameMs_ / 1000.0, 0.1));
  // Smooth the eye when sneaking.
  const float targetEye = static_cast<float>(p.eyeHeight());
  s.eyeOffset += (targetEye - s.eyeOffset) * std::min(1.0f, fdt * 12.0f);
  if (std::fabs(targetEye - s.eyeOffset) > 0.5f) s.eyeOffset = targetEye;
  dvec3 eye = p.prevPos + (p.pos - p.prevPos) * alpha;
  eye.y += s.eyeOffset;
  float bob = 0.0f;
  if (settings_.viewBobbing && p.onGround && !p.flying) {
    const double speed = std::sqrt(p.vel.x * p.vel.x + p.vel.z * p.vel.z);
    const float amount = static_cast<float>(std::min(speed / 5.0, 1.0));
    bob = p.walkDistance * 1.9f;
    const vec3 rightV(std::cos(p.yaw), 0.0f, std::sin(p.yaw));
    eye += dvec3(rightV) * (std::sin(bob) * 0.035 * amount);
    eye.y += std::fabs(std::cos(bob)) * 0.05 * amount;
  }
  fi.cameraPos = eye;
  fi.yaw = p.yaw;
  fi.pitch = p.pitch;
  const int ex = fastFloor(eye.x), ey = fastFloor(eye.y), ez = fastFloor(eye.z);
  const BlockId eyeBlock = w.getBlock(ex, ey, ez);
  auto liquidTop = [&](BlockId id) {
    return ey + (w.getBlock(ex, ey + 1, ez) == id ? 1.0 : static_cast<double>(liquidHeight(w.getMeta(ex, ey, ez))));
  };
  fi.underwater = eyeBlock == block::Water && eye.y < liquidTop(block::Water);
  fi.inLava = eyeBlock == block::Lava && eye.y < liquidTop(block::Lava);
  float fov = settings_.fov;
  if (p.sprinting) fov *= 1.12f;
  if (fi.underwater) fov *= 0.92f;
  fi.fovDegrees = fov;
  lastFov_ = fov;
  fi.time = static_cast<float>(time_);
  fi.timeOfDay = static_cast<float>(floorMod(static_cast<int>(s.time % kDayTicks), static_cast<int>(kDayTicks))) /
                 static_cast<float>(kDayTicks);
  const int64_t day = s.time / kDayTicks;
  fi.moonPhase = static_cast<float>(day % 8) / 8.0f + 0.5f;
  if (fi.moonPhase >= 1.0f) fi.moonPhase -= 1.0f;
  fi.dt = fdt;
  fi.renderDistance = settings_.renderDistance;
  const uint8_t light = w.getLight(ex, ey, ez);
  s.cameraSky += (static_cast<float>(skyOf(light)) / 15.0f - s.cameraSky) * std::min(1.0f, fdt * 2.0f);
  fi.cameraSky = s.cameraSky;
  if (s.target.hit && screen_ == Screen::Playing && !hideHud_) {
    fi.hasSelection = true;
    fi.selection = s.target.block;
    fi.selectionBlock = s.target.id;
    if (s.isBreaking && !p.creative) {
      const float hardness = std::max(0.05f, blockInfo(s.target.id).hardness);
      fi.breakStage = std::min(9, static_cast<int>(s.breakProgress / hardness * 10.0f));
    }
  }
  if (!hideHud_) {
    fi.heldItem = s.inventory.slots[s.selected].empty() ? static_cast<ItemId>(item::None) : s.inventory.slots[s.selected].id;
    fi.heldSwing = s.swinging ? s.swing : 0.0f;
    fi.heldBob = bob;
    fi.heldSky = static_cast<uint8_t>(skyOf(light) * 17);
    fi.heldBlk = static_cast<uint8_t>(blockLightOf(light) * 17);
  }
  fi.damageFlash = p.damageFlash;
  for (const Particle& q : s.particles) {
    ParticleDraw d;
    d.pos = q.pos;
    d.size = q.size;
    d.tex = q.tex;
    d.u0 = q.u0;
    d.v0 = q.v0;
    d.uvSize = 0.22f;
    const uint8_t l = w.getLight(fastFloor(q.pos.x), fastFloor(q.pos.y), fastFloor(q.pos.z));
    d.sky = static_cast<uint8_t>(skyOf(l) * 17);
    d.blk = static_cast<uint8_t>(blockLightOf(l) * 17);
    fi.particles.push_back(d);
  }
  for (const ItemEntity& e : s.items) {
    EntityDraw d;
    d.item = e.item;
    const bool block3d = itemInfo(e.item).isBlock && blockInfo(itemInfo(e.item).block).shape != Shape::Cross;
    d.scale = block3d ? 0.25f : 0.4f;
    d.pos = e.pos + dvec3(0, 0.12 + 0.06 * std::sin(e.age * 2.5f), 0);
    d.yaw = e.age * 1.6f;
    const uint8_t l = w.getLight(fastFloor(e.pos.x), fastFloor(e.pos.y + 0.2), fastFloor(e.pos.z));
    d.sky = static_cast<uint8_t>(skyOf(l) * 17);
    d.blk = static_cast<uint8_t>(blockLightOf(l) * 17);
    fi.entities.push_back(d);
  }
  for (const FallingBlock& f : w.fallingBlocks()) {
    EntityDraw d;
    d.item = f.id;
    d.pos = f.pos;
    d.scale = 1.0f;
    const uint8_t l = w.getLight(fastFloor(f.pos.x), fastFloor(f.pos.y + 0.5), fastFloor(f.pos.z));
    d.sky = static_cast<uint8_t>(skyOf(l) * 17);
    d.blk = static_cast<uint8_t>(blockLightOf(l) * 17);
    fi.entities.push_back(d);
  }
}

void Game::render(double dt) {
  (void)dt;
  int fbw = 1, fbh = 1;
  window_.framebufferSize(fbw, fbh);
  if (fbw <= 0 || fbh <= 0) return;  // minimised
  if (fbw != fbWidth_ || fbh != fbHeight_) {
    fbWidth_ = fbw;
    fbHeight_ = fbh;
    renderer_.resize(fbw, fbh);
  }
  if (session_ && session_->ready) {
    FrameInput fi;
    const bool paused = screen_ == Screen::Paused || screen_ == Screen::Settings;
    const double alpha = paused ? 1.0 : clampv(session_->accumulator / kStep, 0.0, 1.0);
    buildFrameInput(fi, alpha);
    renderer_.render(fi);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbWidth_, fbHeight_);
    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  UiRenderer& ui = renderer_.ui();
  ui.begin(fbWidth_, fbHeight_);
  switch (screen_) {
    case Screen::MainMenu: drawMainMenu(); break;
    case Screen::CreateWorld: drawCreateWorld(); break;
    case Screen::Loading: drawLoading(); break;
    case Screen::Playing: drawHud(); break;
    case Screen::Paused: drawHud(); drawPause(); break;
    case Screen::Settings: if (session_ && session_->ready) drawHud(); drawSettings(); break;
    case Screen::Inventory: drawHud(); drawInventory(); break;
    case Screen::Chat: drawHud(); drawChat(); break;
    case Screen::Dead: drawHud(); drawDeath(); break;
  }
  ui.flush(renderer_.blockTextures());
  if (!pendingScreenshot_.empty()) {
    std::error_code ec;
    const fs::path parent = fs::path(pendingScreenshot_).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);
    if (renderer_.saveScreenshot(pendingScreenshot_)) message("Saved screenshot " + pendingScreenshot_);
    pendingScreenshot_.clear();
  }
}

std::string Game::screenshotPath() const {
  const std::time_t t = std::time(nullptr);
  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
  return std::string("screenshots/strata_") + buf + ".png";
}

void Game::message(const std::string& text) {
  LOG_INFO("%s", text.c_str());
  messages_.push_back({text, 0.0f});
}

}  // namespace strata
