#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include "core/log.hpp"
#include "game/game.hpp"

namespace {
void usage() {
  std::printf(
      "Strata - a voxel sandbox\n\n"
      "Usage: strata [options]\n"
      "  --world NAME          open (or create) the world saves/NAME directly\n"
      "  --seed N              seed for a newly created world\n"
      "  --creative            creative mode (flying, unlimited blocks)\n"
      "  --survival            survival mode\n"
      "  --size WxH            window size (default 1280x720)\n"
      "  --fullscreen          start in fullscreen\n"
      "  --render-distance N   view distance in chunks\n"
      "  --shadows N           0 off, 1 low, 2 medium, 3 high\n"
      "  --clouds N            0 off, 1 fast, 2 fancy\n"
      "  --render-scale X      3D resolution relative to the window, 0.5 .. 1\n"
      "  --no-vsync            disable vertical sync\n"
      "  --exec \"CMD; CMD\"     run console commands (e.g. \"waitchunks; screenshot a.png; quit\")\n"
      "  --frames N            quit after N frames\n"
      "  --settings FILE       settings file (default strata_settings.txt)\n"
      "  --saves DIR           directory with saved worlds (default saves)\n"
      "  --log FILE            also write the log to FILE\n");
}
}  // namespace

static int run(int argc, char** argv) {
  strata::GameOptions o;
#if defined(__EMSCRIPTEN__)
  // In a browser everything lives in /strata, which web/pre.js mounts on
  // IndexedDB so worlds and settings survive reloads. Options come from the
  // page's query string (e.g. index.html?world=test&creative).
  o.settingsPath = "/strata/strata_settings.txt";
  o.savesDir = "/strata/saves";
#endif
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", a.c_str());
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--world") {
      o.world = next();
    } else if (a == "--seed") {
      o.seed = std::strtoull(next().c_str(), nullptr, 10);
      o.hasSeed = true;
    } else if (a == "--creative") {
      o.creative = 1;
    } else if (a == "--survival") {
      o.creative = 0;
    } else if (a == "--size") {
      const std::string v = next();
      const size_t x = v.find('x');
      if (x == std::string::npos) {
        std::fprintf(stderr, "--size expects WIDTHxHEIGHT\n");
        return 2;
      }
      o.width = std::max(320, std::atoi(v.substr(0, x).c_str()));
      o.height = std::max(240, std::atoi(v.substr(x + 1).c_str()));
    } else if (a == "--fullscreen") {
      o.fullscreen = true;
    } else if (a == "--render-distance") {
      o.renderDistance = std::atoi(next().c_str());
    } else if (a == "--shadows") {
      o.shadows = std::atoi(next().c_str());
    } else if (a == "--clouds") {
      o.clouds = std::atoi(next().c_str());
    } else if (a == "--render-scale") {
      o.renderScale = static_cast<float>(std::atof(next().c_str()));
    } else if (a == "--no-vsync") {
      o.vsync = 0;
    } else if (a == "--exec") {
      o.exec = next();
    } else if (a == "--frames") {
      o.maxFrames = std::atoi(next().c_str());
    } else if (a == "--settings") {
      o.settingsPath = next();
    } else if (a == "--saves") {
      o.savesDir = next();
    } else if (a == "--log") {
      strata::log::setFile(next());
    } else if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else {
      std::fprintf(stderr, "unknown option %s (try --help)\n", a.c_str());
      return 2;
    }
  }
#if defined(__EMSCRIPTEN__)
  // The browser keeps calling the game after main() returns, so it must
  // outlive this function; the page owns it until it is closed.
  static strata::Game* game = new strata::Game();
  const int code = game->run(o);
  if (code != 0) LOG_ERROR("the game could not start (code %d)", code);
  return code;
#else
  try {
    strata::Game game;
    return game.run(o);
  } catch (const std::exception& e) {
    LOG_ERROR("fatal error: %s", e.what());
    return 1;
  }
#endif
}

#if defined(__EMSCRIPTEN__)
// Command line arguments come from the page (index.html turns its query
// string into Module.arguments). They are fetched here because this
// toolchain drops a main(argc, argv) entry point.
EM_JS(int, js_arg_count, (), {
  const a = typeof arguments_ !== 'undefined' ? arguments_ : (Module['arguments'] || []);
  return a.length;
});
EM_JS(void, js_arg, (int i, char* out, int size), {
  const a = typeof arguments_ !== 'undefined' ? arguments_ : (Module['arguments'] || []);
  stringToUTF8(String(a[i]), out, size);
});

EM_JS(void, js_start_failed, (int code), {
  if (Module.strataFail)
    Module.strataFail('The game did not start (code ' + code + '). Check the options in the page address; ' +
                      'the browser console has details.');
});

int main() {
  std::vector<std::string> args{"strata"};
  const int n = js_arg_count();
  for (int i = 0; i < n; ++i) {
    char buf[1024];
    js_arg(i, buf, static_cast<int>(sizeof(buf)));
    args.emplace_back(buf);
  }
  std::vector<char*> argv;
  for (std::string& a : args) argv.push_back(&a[0]);
  argv.push_back(nullptr);
  const int code = run(static_cast<int>(args.size()), argv.data());
  if (code != 0) {
    // Leave the page with an explanation rather than a spinner.
    js_start_failed(code);
  }
  return code;
}
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
