#pragma once

#include "draw_utils.h"

struct Palette {
  FLinearColor projectile_color;
  FLinearColor background_color;
  FLinearColor state_colors[8];
};

struct CurrentOptions {
  const Palette& palette;
  bool show_fade;
  bool show_delim;
  bool show_cancels;
};

class ModMenu {
  DrawContext tool;

//  std::vector<int> settings;

  bool is_showing = false;
  int cursor_position = 0;

  void changeSetting(size_t idx, bool right=true);

  ModMenu();
public:
  ~ModMenu();
  static ModMenu& instance();

  void update(bool framebar_pressed, bool hitbox_pressed, bool menu_pressed);
  void draw();

  bool barEnabled() const;
  bool hitboxEnabled() const;
  bool fadeEnabled() const;
  bool delimEnabled() const;
  bool cancelEnabled() const;
  bool dashEnabled() const;
  int pauseType() const;

  CurrentOptions getScheme() const;
};
