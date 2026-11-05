// Keyboard, mouse and touch state, fed by the platform window (see
// platform/window.hpp). Key and button codes are GLFW's on every platform.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/math.hpp"

namespace strata {

class Input {
 public:
  static constexpr int kKeyCount = 512;
  static constexpr int kButtonCount = 8;
  // Key / button actions (same values as GLFW_RELEASE, GLFW_PRESS, GLFW_REPEAT).
  static constexpr int kRelease = 0, kPress = 1, kRepeat = 2;

  // Cancelled: the system took the touch over (e.g. an edge gesture); unlike
  // Ended it never counts as a tap.
  enum class TouchPhase { Began, Moved, Ended, Cancelled };
  struct TouchEvent {
    int id;
    TouchPhase phase;
    vec2 pos;  // framebuffer pixels
  };

  // Call once at the end of every frame. It forgets this frame's presses,
  // clicks, typed text, movement and touches; events that arrive before the
  // next frame (a browser delivers them between frames) count for that frame.
  void endFrame();

  bool down(int key) const { return valid(key) && keys_[static_cast<size_t>(key)]; }
  bool pressed(int key) const { return valid(key) && pressed_[static_cast<size_t>(key)]; }
  // Pressed or auto-repeated this frame (text editing).
  bool repeated(int key) const { return valid(key) && (pressed_[static_cast<size_t>(key)] || repeat_[static_cast<size_t>(key)]); }
  bool mouseDown(int b) const { return b >= 0 && b < kButtonCount && buttons_[static_cast<size_t>(b)]; }
  bool mousePressed(int b) const { return b >= 0 && b < kButtonCount && buttonPressed_[static_cast<size_t>(b)]; }
  bool mouseReleased(int b) const { return b >= 0 && b < kButtonCount && buttonReleased_[static_cast<size_t>(b)]; }
  vec2 mouseDelta() const { return delta_; }
  vec2 mousePos() const { return pos_; }
  float scroll() const { return scroll_; }
  const std::string& typed() const { return typed_; }
  bool focused() const { return focused_; }
  bool resized() const { return resized_; }
  // This frame's touch screen events, in order.
  const std::vector<TouchEvent>& touches() const { return touches_; }
  // True when the last pointer input came from a touch screen (shows the
  // on-screen controls); a real mouse or keyboard switches back.
  bool usingTouch() const { return usingTouch_; }
  void setUsingTouch(bool touch) { usingTouch_ = touch; }

  void setCursorCaptured(bool captured);
  bool cursorCaptured() const { return captured_; }
  // The platform released a captured cursor on its own (e.g. Esc in a
  // browser); reported once.
  bool takeCaptureLost();
  // Discard pending mouse movement (e.g. after capturing the cursor).
  void resetMouse();
  // Ignore all keys until they are released (used when leaving text input).
  void consumeAll();
  // Forget this frame's presses, clicks and typed text (keeps held state), so a
  // key that switches screens is not seen again by the new screen.
  void consumePresses();

  // Events from the platform window. `emulated` marks mouse events generated
  // from touches (menus are driven by a virtual mouse on touch screens).
  void onKey(int key, int action);
  void onChar(unsigned codepoint);
  void onButton(int button, int action, bool emulated = false);
  // Cursor position in window units; `scale` converts them to framebuffer
  // pixels (mousePos() is in framebuffer pixels, mouseDelta() in window units).
  void onCursor(double x, double y, float scaleX = 1.0f, float scaleY = 1.0f, bool emulated = false);
  void onCursorDelta(double dx, double dy);  // relative motion of a captured pointer
  void onScroll(double dy);
  void onFocus(bool focused);
  void onResize() { resized_ = true; }
  void onTouch(int id, TouchPhase phase, float x, float y);
  void onCaptureLost();

 private:
  static bool valid(int key) { return key >= 0 && key < kKeyCount; }
  std::array<bool, kKeyCount> keys_{}, pressed_{}, repeat_{};
  std::array<bool, kButtonCount> buttons_{}, buttonPressed_{}, buttonReleased_{};
  vec2 pos_, last_, delta_;
  bool havePos_ = false;
  float scroll_ = 0.0f;
  std::string typed_;
  std::vector<TouchEvent> touches_;
  bool focused_ = true;
  bool captured_ = false;
  bool captureLost_ = false;
  bool resized_ = false;
  bool usingTouch_ = false;
};

}  // namespace strata
