// The application: window, main loop, world session, interaction and menus.
#pragma once

#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/random.hpp"
#include "game/input.hpp"
#include "game/player.hpp"
#include "game/settings.hpp"
#include "game/touch.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "world/items.hpp"
#include "world/raycast.hpp"
#include "world/storage.hpp"
#include "world/world.hpp"

namespace strata {

struct GameOptions {
  int width = 1280, height = 720;
  bool fullscreen = false;
  std::string world;          // open (or create) this world directly
  bool hasSeed = false;
  uint64_t seed = 0;
  int creative = -1;          // -1 keep, 0 survival, 1 creative
  std::string exec;           // ';' separated commands to run
  int maxFrames = 0;          // quit after this many frames (0 = never)
  int renderDistance = 0;     // overrides the settings file when > 0
  int shadows = -1, clouds = -1;
  float renderScale = 0.0f;   // overrides the settings file when > 0
  int vsync = -1;
  std::string settingsPath = "strata_settings.txt";
  std::string savesDir = "saves";
};

class Game {
 public:
  Game();
  ~Game();
  int run(const GameOptions& options);

 private:
  enum class Screen { MainMenu, CreateWorld, Loading, Playing, Paused, Settings, Inventory, Chat, Dead };

  struct ItemEntity {
    ItemId item = 0;
    int count = 0;
    dvec3 pos;
    dvec3 vel;
    float age = 0.0f;
    float pickupDelay = 0.6f;
  };
  struct Particle {
    dvec3 pos;
    vec3 vel;
    float life = 1.0f;
    uint16_t tex = 0;
    float u0 = 0, v0 = 0;
    float size = 0.08f;
    bool resting = false;
  };
  struct Session {
    std::string name;
    std::filesystem::path dir;
    std::unique_ptr<World> world;
    LevelData level;
    Player player;
    Inventory inventory;
    int selected = 0;
    int64_t time = 0;
    ivec3 spawn;
    bool ready = false;
    bool needsUnstuck = false;  // move out of blocks once the area has loaded (after /tp)
    double accumulator = 0.0;
    int tickPhase = 0;
    RayHit target;
    ivec3 breaking;
    bool isBreaking = false;
    float breakProgress = 0.0f;
    float breakCooldown = 0.0f, useCooldown = 0.0f;
    float swing = 0.0f;
    bool swinging = false;
    float lastJump = -10.0f;
    float lastForwardTap = -10.0f;
    bool sprintTap = false;  // W was double-tapped this frame
    float selectionTimer = 0.0f;
    float cameraSky = 1.0f;
    float eyeOffset = 0.0f;  // smoothed sneak eye height change
    double autosave = 0.0;
    double startTime = 0.0;  // game time when the session started (loading time)
    std::vector<ItemEntity> items;
    std::vector<Particle> particles;
    ItemStack cursor;  // stack held by the mouse in the inventory screen
  };
  struct Message {
    std::string text;
    float age = 0.0f;
  };

  // Lifecycle.
  bool initWindow(const GameOptions& o);
  void shutdownWindow();
  // One pass of the main loop; false once the game should exit.
  bool loopIteration();
  void finish();
  void frame(double dt);
  void startSession(const std::string& name, bool create, uint64_t seed, bool creative);
  void endSession();
  void saveSession();
  void applySettings();
  void setFullscreen(bool on);
  void setScreen(Screen s);

  // Simulation.
  void updateSession(double dt);
  void updatePlaying(double dt);
  void handleInteraction(double dt);
  void breakBlock(const ivec3& p);
  void placeBlock();
  void pickBlock();
  void dropSelected();
  void processChanges();
  void spawnDrops(BlockId b, const ivec3& p);
  void spawnItem(ItemId item, int count, const dvec3& pos, const dvec3& vel);
  void spawnBreakParticles(BlockId b, const ivec3& p);
  void updateItems(double dt);
  void updateParticles(double dt);
  void respawn();
  bool spawnAreaReady() const;
  bool playerAreaReady() const;

  // Touch screens.
  void processTouches();
  TouchControls::Layout touchLayout() const;
  bool touchUi() const { return input_.usingTouch(); }
  // Direction of the ray through a point on the screen (framebuffer pixels).
  vec3 aimDirection(vec2 screen) const;
  // Asks for a chat line or command with a native text dialog.
  void promptChat();
  // Persists saves (IndexedDB in a browser).
  void persist();

  // Rendering and UI.
  void render(double dt);
  void buildFrameInput(FrameInput& fi, double alpha);
  float guiScale() const;
  float hudScale() const;  // hotbar and touch controls; larger on touch screens
  void drawHud();
  void drawDebug();
  void drawHotbar(float g);
  void drawMessages(float g, bool all);
  void drawBackground();
  void drawMainMenu();
  void drawCreateWorld();
  void drawLoading();
  void drawPause();
  void drawSettings();
  void drawInventory();
  void drawChat();
  void drawDeath();
  bool button(float x, float y, float w, float h, const std::string& label, bool enabled = true);
  bool slider(float x, float y, float w, float h, const std::string& label, float& value, float lo, float hi,
              int decimals);
  bool textField(float x, float y, float w, float h, std::string& text, int id, size_t maxLen,
                 const char* title = "Enter text");
  bool slotWidget(float x, float y, float size, ItemStack& stack, bool creativePalette);
  void message(const std::string& text);

  // Commands.
  void queueCommands(const std::string& script);
  void runCommandQueue();
  bool executeCommand(const std::string& line);
  std::string screenshotPath() const;

  Window window_;
  Input input_;
  TouchControls touch_;
  int menuFinger_ = -1;  // touch driving the virtual mouse in menus
  Renderer renderer_;
  Settings settings_;
  GameOptions options_;
  std::unique_ptr<Session> session_;
  Screen screen_ = Screen::MainMenu;
  Screen settingsReturn_ = Screen::MainMenu;
  bool quit_ = false;
  bool showDebug_ = false;
  bool hideHud_ = false;
  int fbWidth_ = 1, fbHeight_ = 1;
  double time_ = 0.0;
  double lastTime_ = 0.0, fpsTimer_ = 0.0;
  int fpsFrames_ = 0;
  float lastFov_ = 72.0f;
  bool finished_ = false;
  bool wasFocused_ = true;
  double fps_ = 0.0, frameMs_ = 0.0;
  uint64_t frameCount_ = 0;
  std::deque<Message> messages_;
  std::string chatText_;
  std::vector<std::string> chatHistory_;
  int chatHistoryIndex_ = -1;
  std::deque<std::string> commandQueue_;
  int waitFrames_ = 0;
  bool waitChunks_ = false;
  std::string pendingScreenshot_;
  // Create world screen.
  std::string newWorldName_ = "New World";
  std::string newWorldSeed_;
  bool newWorldCreative_ = false;
  int focusedField_ = -1;
  bool debugFog_ = true;
  std::vector<std::pair<std::string, std::string>> worldList_;  // folder, display name
  Pcg32 rng_;
};

}  // namespace strata
