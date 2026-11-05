// On-screen controls for touch screens (phones and tablets).
//
//  * Left side: a floating movement stick that appears under the thumb.
//    Pushing it well past its rim while moving forward sprints.
//  * Anywhere else: drag to look around. A quick tap uses or places a block
//    at the tapped point; resting a finger breaks the block under it (the held
//    finger can still drag the view).
//  * Buttons: jump (double tap toggles flying in creative mode), sneak (a
//    toggle; held, it descends while flying), a flying toggle in creative
//    mode, and inventory, chat and pause along the top.
//  * Hotbar: tap a slot to select it.
#pragma once

#include <vector>

#include "core/math.hpp"
#include "game/input.hpp"

namespace strata {

class UiRenderer;

class TouchControls {
 public:
  // Screen geometry for this frame, in framebuffer pixels.
  struct Layout {
    float width = 1.0f, height = 1.0f;
    float unit = 1.0f;  // pixels per CSS pixel; sizes the controls
    float hotbarX = 0.0f, hotbarY = 0.0f, hotbarSlot = 1.0f;  // nine slots from hotbarX
    bool creative = false;
    bool flying = false;
  };

  struct Output {
    float forward = 0.0f, strafe = 0.0f;  // -1 .. 1
    bool sprint = false;
    bool jump = false;         // held
    bool jumpPressed = false;  // went down this frame
    bool sneak = false;
    vec2 look;                 // camera drag this frame, in CSS pixels
    bool aiming = false;       // a finger rests on the world ...
    vec2 aim;                  // ... here (framebuffer pixels)
    bool use = false;          // tapped: use or place at `aim`
    bool breaking = false;     // held: break at `aim`
    int hotbarSlot = -1;       // tapped slot
    bool pause = false, inventory = false, chat = false, toggleFly = false;
  };

  // Seconds a finger must rest before it starts breaking, and how far
  // (CSS pixels) it may move and still count as resting or tapping.
  static constexpr double kHoldTime = 0.3;
  static constexpr float kTapSlop = 12.0f;
  static constexpr float kStickRadius = 52.0f;  // CSS pixels

  // Processes this frame's touch events (call every frame while playing).
  void update(const std::vector<Input::TouchEvent>& events, const Layout& layout, double now);
  const Output& output() const { return out_; }
  // Forgets every finger, e.g. when a menu opens on top of the game.
  void releaseAll();
  // Owns this touch id (it began on the controls).
  bool owns(int id) const;
  void draw(UiRenderer& ui, const Layout& layout) const;

 private:
  enum class Role { Stick, Look, Button, Hotbar };
  enum ButtonId { kJump, kSneak, kFly, kInventory, kChat, kPause, kButtonCount };
  struct Finger {
    int id = -1;
    Role role = Role::Look;
    int button = -1;
    vec2 start, pos;
    double startTime = 0.0;
    float travel = 0.0f;   // distance moved so far (CSS pixels)
    bool holding = false;  // a look finger that rested long enough to break
  };
  struct Circle {
    vec2 c;
    float r = 0.0f;
    bool visible = false;
  };

  Circle button(int b, const Layout& l) const;
  int buttonAt(vec2 p, const Layout& l) const;
  int hotbarSlotAt(vec2 p, const Layout& l) const;
  vec2 stickRest(const Layout& l) const;  // where the stick is drawn while unused
  Finger* find(int id);
  void press(int b, const Layout& l);

  std::vector<Finger> fingers_;
  Output out_;
  bool sneakToggle_ = false;
};

}  // namespace strata
