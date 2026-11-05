#include "game/input.hpp"

namespace strata {

void Input::endFrame() {
  pressed_.fill(false);
  repeat_.fill(false);
  buttonPressed_.fill(false);
  buttonReleased_.fill(false);
  delta_ = vec2(0.0f, 0.0f);
  scroll_ = 0.0f;
  typed_.clear();
  touches_.clear();
  resized_ = false;
}

void Input::setCursorCaptured(bool captured) {
  if (captured == captured_) return;
  captured_ = captured;
  captureLost_ = false;
  resetMouse();
}

bool Input::takeCaptureLost() {
  const bool lost = captureLost_;
  captureLost_ = false;
  return lost;
}

void Input::resetMouse() {
  havePos_ = false;
  delta_ = vec2(0.0f, 0.0f);
}

void Input::consumeAll() {
  keys_.fill(false);
  consumePresses();
}

void Input::consumePresses() {
  pressed_.fill(false);
  repeat_.fill(false);
  buttonPressed_.fill(false);
  typed_.clear();
  scroll_ = 0.0f;
}

void Input::onKey(int key, int action) {
  if (!valid(key)) return;
  const size_t k = static_cast<size_t>(key);
  if (action == kPress) {
    keys_[k] = true;
    pressed_[k] = true;
    usingTouch_ = false;
  } else if (action == kRelease) {
    keys_[k] = false;
  } else if (action == kRepeat) {
    repeat_[k] = true;
  }
}

void Input::onChar(unsigned cp) {
  if (cp >= 32 && cp < 127) typed_.push_back(static_cast<char>(cp));
}

void Input::onButton(int button, int action, bool emulated) {
  if (button < 0 || button >= kButtonCount) return;
  if (!emulated) usingTouch_ = false;
  const size_t b = static_cast<size_t>(button);
  if (action == kPress) {
    buttons_[b] = true;
    buttonPressed_[b] = true;
  } else if (action == kRelease) {
    buttons_[b] = false;
    buttonReleased_[b] = true;
  }
}

void Input::onCursor(double x, double y, float scaleX, float scaleY, bool emulated) {
  pos_ = vec2(static_cast<float>(x) * scaleX, static_cast<float>(y) * scaleY);
  if (emulated) {
    // A virtual cursor jumps to each new touch; that is not mouse movement.
    havePos_ = false;
    return;
  }
  // Movement is measured in the unscaled units (screen coordinates), which
  // keeps the look sensitivity independent of the display density.
  const vec2 p(static_cast<float>(x), static_cast<float>(y));
  if (havePos_) {
    delta_ += p - last_;
    if (length(p - last_) > 0.0f) usingTouch_ = false;
  }
  last_ = p;
  havePos_ = true;
}

void Input::onCursorDelta(double dx, double dy) {
  delta_ += vec2(static_cast<float>(dx), static_cast<float>(dy));
  if (dx != 0.0 || dy != 0.0) usingTouch_ = false;
}

void Input::onScroll(double dy) { scroll_ += static_cast<float>(dy); }

void Input::onFocus(bool f) {
  focused_ = f;
  if (!f) {
    keys_.fill(false);
    buttons_.fill(false);
  }
}

void Input::onTouch(int id, TouchPhase phase, float x, float y) {
  usingTouch_ = true;
  touches_.push_back({id, phase, vec2(x, y)});
}

void Input::onCaptureLost() {
  if (captured_) captureLost_ = true;
}

}  // namespace strata
