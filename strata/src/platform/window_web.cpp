// Browser window (phones, tablets and desktop browsers): an HTML canvas with
// a WebGL 2 context, DOM input events and the browser's pointer lock and
// fullscreen APIs. web/index.html provides the page and web/pre.js the
// IndexedDB storage and the gesture-gated requests (fullscreen).
#include "platform/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>  // key codes only; the game uses GLFW's numbering everywhere
#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "core/log.hpp"
#include "game/input.hpp"
#include "gl/gl.hpp"

namespace strata {

namespace {
constexpr const char* kCanvas = "#canvas";
// Rendering more pixels than this per CSS pixel costs a lot of fill rate on
// phones (3x screens) for little visible gain; the 3D view has its own scale.
constexpr double kMaxPixelRatio = 2.0;

// clang-format off
EM_JS(double, js_pixel_ratio, (), { return window.devicePixelRatio || 1; });
EM_JS(int, js_coarse_pointer, (), {
  try { return window.matchMedia('(pointer: coarse)').matches ? 1 : 0; } catch (e) { return 0; }
});
EM_JS(void, js_fullscreen, (int on), { if (Module.strataFullscreen) Module.strataFullscreen(!!on); });
EM_JS(int, js_is_fullscreen, (), { return (document.fullscreenElement || document.webkitFullscreenElement) ? 1 : 0; });
EM_JS(void, js_sync, (), { if (Module.strataSync) Module.strataSync(); });
// Writes the answer (truncated to fit) into `out`; returns -1 when cancelled.
EM_JS(int, js_prompt, (const char* title, const char* text, char* out, int outSize), {
  const r = window.prompt(UTF8ToString(title), UTF8ToString(text));
  if (r === null) return -1;
  stringToUTF8(r, out, outSize);
  return 0;
});
EM_JS(void, js_ready, (), { if (Module.strataReady) Module.strataReady(); });
EM_JS(void, js_fail, (const char* text), { if (Module.strataFail) Module.strataFail(UTF8ToString(text)); });
// clang-format on

// KeyboardEvent.code (the physical key, independent of the layout) to GLFW.
int mapKey(const char* code) {
  if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z' && code[4] == '\0')
    return GLFW_KEY_A + (code[3] - 'A');
  if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9' && code[6] == '\0')
    return GLFW_KEY_0 + (code[5] - '0');
  if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9') {
    const int n = std::atoi(code + 1);
    if (n >= 1 && n <= 12) return GLFW_KEY_F1 + n - 1;
  }
  struct Named {
    const char* code;
    int key;
  };
  static const Named kNamed[] = {
      {"Space", GLFW_KEY_SPACE},         {"Enter", GLFW_KEY_ENTER},           {"NumpadEnter", GLFW_KEY_KP_ENTER},
      {"Escape", GLFW_KEY_ESCAPE},       {"Backspace", GLFW_KEY_BACKSPACE},   {"Tab", GLFW_KEY_TAB},
      {"Delete", GLFW_KEY_DELETE},       {"ArrowRight", GLFW_KEY_RIGHT},      {"ArrowLeft", GLFW_KEY_LEFT},
      {"ArrowDown", GLFW_KEY_DOWN},      {"ArrowUp", GLFW_KEY_UP},            {"PageUp", GLFW_KEY_PAGE_UP},
      {"PageDown", GLFW_KEY_PAGE_DOWN},  {"Home", GLFW_KEY_HOME},             {"End", GLFW_KEY_END},
      {"ShiftLeft", GLFW_KEY_LEFT_SHIFT}, {"ShiftRight", GLFW_KEY_RIGHT_SHIFT}, {"ControlLeft", GLFW_KEY_LEFT_CONTROL},
      {"ControlRight", GLFW_KEY_RIGHT_CONTROL}, {"AltLeft", GLFW_KEY_LEFT_ALT}, {"AltRight", GLFW_KEY_RIGHT_ALT},
      {"Slash", GLFW_KEY_SLASH},         {"Minus", GLFW_KEY_MINUS},           {"Equal", GLFW_KEY_EQUAL},
      {"Period", GLFW_KEY_PERIOD},       {"Comma", GLFW_KEY_COMMA},           {"Semicolon", GLFW_KEY_SEMICOLON},
      {"Quote", GLFW_KEY_APOSTROPHE},    {"BracketLeft", GLFW_KEY_LEFT_BRACKET}, {"BracketRight", GLFW_KEY_RIGHT_BRACKET},
      {"Backslash", GLFW_KEY_BACKSLASH}, {"Backquote", GLFW_KEY_GRAVE_ACCENT},
  };
  for (const Named& n : kNamed)
    if (std::strcmp(code, n.code) == 0) return n.key;
  return -1;
}

// The single code point of a printable key value ("a", "A", "?"), or 0 for
// named keys ("Enter", "Shift", ...).
unsigned printable(const char* key) {
  const unsigned char* s = reinterpret_cast<const unsigned char*>(key);
  unsigned cp = 0;
  int len = 0;
  if (s[0] < 0x80) cp = s[0], len = 1;
  else if ((s[0] & 0xE0) == 0xC0) cp = s[0] & 0x1Fu, len = 2;
  else if ((s[0] & 0xF0) == 0xE0) cp = s[0] & 0x0Fu, len = 3;
  else if ((s[0] & 0xF8) == 0xF0) cp = s[0] & 0x07u, len = 4;
  else return 0;
  for (int i = 1; i < len; ++i) {
    if ((s[i] & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (s[i] & 0x3Fu);
  }
  return s[len] == '\0' ? cp : 0;
}

// DOM buttons are 0 left, 1 middle, 2 right; GLFW's are 0 left, 1 right, 2 middle.
int mapButton(unsigned short b) { return b == 1 ? GLFW_MOUSE_BUTTON_MIDDLE : b == 2 ? GLFW_MOUSE_BUTTON_RIGHT : b; }
}  // namespace

struct Window::Impl {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
  Input* input = nullptr;
  int width = 1, height = 1;  // canvas drawing buffer
  double cssW = 0.0, cssH = 0.0, ratio = 0.0;
  bool locked = false;
  bool wantLock = false;

  // Canvas pixels per CSS pixel for converting event coordinates.
  float scaleX() const { return cssW > 0.0 ? static_cast<float>(width / cssW) : 1.0f; }
  float scaleY() const { return cssH > 0.0 ? static_cast<float>(height / cssH) : 1.0f; }

  // Matches the drawing buffer to the canvas' CSS size (window resizes,
  // rotation, browser zoom and moving between screens).
  void updateSize() {
    double w = 0.0, h = 0.0;
    emscripten_get_element_css_size(kCanvas, &w, &h);
    const double r = std::min(js_pixel_ratio(), kMaxPixelRatio);
    if (w == cssW && h == cssH && r == ratio) return;
    cssW = w;
    cssH = h;
    ratio = r;
    width = std::max(1, static_cast<int>(std::lround(w * r)));
    height = std::max(1, static_cast<int>(std::lround(h * r)));
    emscripten_set_canvas_element_size(kCanvas, width, height);
    if (input) input->onResize();
  }
};

namespace {
Window::Impl* implOf(void* user) { return static_cast<Window::Impl*>(user); }

EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* e, void* user) {
  Window::Impl& w = *implOf(user);
  const int key = mapKey(e->code);
  // Releases always count, or a key let go while Ctrl is down would stick.
  if (type == EMSCRIPTEN_EVENT_KEYUP) {
    if (key >= 0) w.input->onKey(key, Input::kRelease);
    return EM_FALSE;
  }
  if (type != EMSCRIPTEN_EVENT_KEYDOWN) return EM_FALSE;
  // Leave browser shortcuts (Ctrl/Cmd combinations, reload, fullscreen,
  // developer tools) to the browser.
  if (e->ctrlKey || e->metaKey) return EM_FALSE;
  if (std::strcmp(e->code, "F5") == 0 || std::strcmp(e->code, "F11") == 0 || std::strcmp(e->code, "F12") == 0)
    return EM_FALSE;
  if (key >= 0) w.input->onKey(key, e->repeat ? Input::kRepeat : Input::kPress);
  const unsigned cp = printable(e->key);
  if (cp) w.input->onChar(cp);
  return key >= 0 ? EM_TRUE : EM_FALSE;  // stop the page scrolling on Space, arrows, ...
}

EM_BOOL onMouse(int type, const EmscriptenMouseEvent* e, void* user) {
  Window::Impl& w = *implOf(user);
  switch (type) {
    case EMSCRIPTEN_EVENT_MOUSEDOWN: w.input->onButton(mapButton(e->button), Input::kPress); break;
    case EMSCRIPTEN_EVENT_MOUSEUP: w.input->onButton(mapButton(e->button), Input::kRelease); break;
    case EMSCRIPTEN_EVENT_MOUSEMOVE:
      if (w.locked) {
        w.input->onCursorDelta(static_cast<double>(e->movementX), static_cast<double>(e->movementY));
      } else {
        w.input->onCursor(static_cast<double>(e->targetX), static_cast<double>(e->targetY), w.scaleX(), w.scaleY());
      }
      break;
    default: break;
  }
  return EM_TRUE;
}

EM_BOOL onWheel(int, const EmscriptenWheelEvent* e, void* user) {
  // One notch is ~100 pixels or 3 lines; GLFW reports +1 per notch up.
  const double unit = e->deltaMode == DOM_DELTA_PIXEL ? 100.0 : e->deltaMode == DOM_DELTA_LINE ? 3.0 : 1.0;
  if (e->deltaY != 0.0) implOf(user)->input->onScroll(-e->deltaY / unit);
  return EM_TRUE;
}

EM_BOOL onTouch(int type, const EmscriptenTouchEvent* e, void* user) {
  Window::Impl& w = *implOf(user);
  const Input::TouchPhase phase = type == EMSCRIPTEN_EVENT_TOUCHSTART  ? Input::TouchPhase::Began
                                  : type == EMSCRIPTEN_EVENT_TOUCHMOVE ? Input::TouchPhase::Moved
                                  : type == EMSCRIPTEN_EVENT_TOUCHEND  ? Input::TouchPhase::Ended
                                                                       : Input::TouchPhase::Cancelled;
  for (int i = 0; i < e->numTouches; ++i) {
    const EmscriptenTouchPoint& t = e->touches[i];
    if (!t.isChanged) continue;
    w.input->onTouch(static_cast<int>(t.identifier), phase, static_cast<float>(t.targetX) * w.scaleX(),
                     static_cast<float>(t.targetY) * w.scaleY());
  }
  return EM_TRUE;  // no emulated mouse events, scrolling or zooming
}

EM_BOOL onPointerLock(int, const EmscriptenPointerlockChangeEvent* e, void* user) {
  Window::Impl& w = *implOf(user);
  const bool was = w.locked;
  w.locked = e->isActive;
  // Esc in a browser ends the lock without a key event reaching the page.
  if (was && !w.locked) {
    w.input->resetMouse();
    if (w.wantLock) w.input->onCaptureLost();
    w.wantLock = false;
  }
  if (w.locked) w.input->resetMouse();
  return EM_TRUE;
}

EM_BOOL onFocus(int type, const EmscriptenFocusEvent*, void* user) {
  implOf(user)->input->onFocus(type == EMSCRIPTEN_EVENT_FOCUS);
  return EM_FALSE;
}

EM_BOOL onVisibility(int, const EmscriptenVisibilityChangeEvent* e, void* user) {
  implOf(user)->input->onFocus(!e->hidden);
  return EM_FALSE;
}
}  // namespace

Window::Window() = default;
Window::~Window() { destroy(); }

bool Window::create(int, int, bool, Input& input) {
  destroy();
  auto m = std::make_unique<Impl>();
  m->input = &input;
  EmscriptenWebGLContextAttributes attr;
  emscripten_webgl_init_context_attributes(&attr);
  attr.majorVersion = 2;
  attr.minorVersion = 0;
  attr.alpha = EM_FALSE;
  attr.depth = EM_FALSE;  // every 3D pass renders into its own targets
  attr.stencil = EM_FALSE;
  attr.antialias = EM_FALSE;
  attr.premultipliedAlpha = EM_FALSE;
  attr.preserveDrawingBuffer = EM_FALSE;
  attr.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
  attr.enableExtensionsByDefault = EM_TRUE;
  m->context = emscripten_webgl_create_context(kCanvas, &attr);
  if (m->context <= 0) {
    LOG_ERROR("this browser cannot create a WebGL 2 context");
    js_fail("This browser could not create a WebGL 2 context. Make sure hardware acceleration is enabled.");
    return false;
  }
  emscripten_webgl_make_context_current(m->context);
  // Floating point render targets carry the HDR image; without them nothing
  // can be drawn.
  const bool floatTargets = emscripten_webgl_enable_extension(m->context, "EXT_color_buffer_float");
  const bool halfTargets = emscripten_webgl_enable_extension(m->context, "EXT_color_buffer_half_float");
  emscripten_webgl_enable_extension(m->context, "EXT_texture_filter_anisotropic");
  emscripten_webgl_enable_extension(m->context, "OES_texture_float_linear");
  if (!floatTargets && !halfTargets) {
    LOG_ERROR("this browser's WebGL 2 cannot render to floating point textures");
    js_fail("This device cannot render to floating point textures, which Strata needs.");
    return false;
  }
  m->updateSize();

  Impl* u = m.get();
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, u, EM_TRUE, onKey);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, u, EM_TRUE, onKey);
  emscripten_set_mousedown_callback(kCanvas, u, EM_TRUE, onMouse);
  emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, u, EM_TRUE, onMouse);
  emscripten_set_mousemove_callback(kCanvas, u, EM_TRUE, onMouse);
  emscripten_set_wheel_callback(kCanvas, u, EM_TRUE, onWheel);
  emscripten_set_touchstart_callback(kCanvas, u, EM_TRUE, onTouch);
  emscripten_set_touchmove_callback(kCanvas, u, EM_TRUE, onTouch);
  emscripten_set_touchend_callback(kCanvas, u, EM_TRUE, onTouch);
  emscripten_set_touchcancel_callback(kCanvas, u, EM_TRUE, onTouch);
  emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, u, EM_TRUE, onPointerLock);
  emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, u, EM_TRUE, onFocus);
  emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, u, EM_TRUE, onFocus);
  emscripten_set_visibilitychange_callback(u, EM_TRUE, onVisibility);
  impl_ = std::move(m);
  const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  LOG_INFO("WebGL 2 context %dx%d (%s), float targets: %s", impl_->width, impl_->height,
           renderer ? renderer : "?", floatTargets ? "32 and 16 bit" : "16 bit only");
  js_ready();
  return true;
}

void Window::destroy() {
  if (!impl_) return;
  emscripten_html5_remove_all_event_listeners();
  if (impl_->context > 0) emscripten_webgl_destroy_context(impl_->context);
  impl_.reset();
}

void Window::pollEvents() {
  // DOM events arrive between frames on their own; only the size is polled.
  if (impl_) impl_->updateSize();
}

void Window::swapBuffers() {}  // the browser presents when the frame callback returns

bool Window::closeRequested() const { return !impl_; }

void Window::framebufferSize(int& w, int& h) const {
  w = impl_ ? impl_->width : 1;
  h = impl_ ? impl_->height : 1;
}

float Window::pixelRatio() const { return impl_ && impl_->ratio > 0.0 ? static_cast<float>(impl_->ratio) : 1.0f; }

void Window::setCursorCaptured(bool captured) {
  if (!impl_) return;
  impl_->wantLock = captured;
  if (captured) {
    // Browsers only grant the lock during a user gesture; Emscripten retries
    // in the next input event handler (the release of the click that got here).
    if (!impl_->locked) emscripten_request_pointerlock(kCanvas, EM_TRUE);
  } else if (impl_->locked) {
    emscripten_exit_pointerlock();
  }
}

bool Window::cursorCaptured() const { return impl_ && impl_->locked; }

void Window::setFullscreen(bool on) { js_fullscreen(on ? 1 : 0); }

bool Window::fullscreen() const { return js_is_fullscreen() != 0; }

void Window::setVsync(bool) {}  // the browser always paces frames to the display

bool Window::canQuit() { return false; }

bool Window::hasTouchScreen() { return js_coarse_pointer() != 0; }

double Window::now() { return emscripten_get_now() / 1000.0; }

bool Window::promptText(const std::string& title, std::string& text) {
  char buf[512];
  if (js_prompt(title.c_str(), text.c_str(), buf, static_cast<int>(sizeof(buf))) < 0) return false;
  text = buf;
  return true;
}

void Window::persistStorage() { js_sync(); }

}  // namespace strata
