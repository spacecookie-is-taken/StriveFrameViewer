#pragma once

#include "draw_utils.h"

struct Palette {
  FLinearColor projectile_color;
  FLinearColor background_color;
  FLinearColor state_colors[7];
};

struct CurrentOptions {
  const Palette& palette;
  bool show_fade;
  bool show_delim;
  bool show_cancels;
};

class ModMenu {
  DrawContext tool;

  std::vector<int> settings;

  bool is_showing = false;
  int cursor_position = 0;

  void changeSetting(size_t idx, bool right=true);

  ModMenu();
public:
  ~ModMenu();
  static ModMenu& instance();

  void update(bool bar_toggled, bool hitbox_toggled, bool menu_toggled);
  void draw();

  bool barEnabled() const;
  bool hitboxEnabled() const;
  bool fadeEnabled() const;
  bool delimEnabled() const;
  bool cancelEnabled() const;

  CurrentOptions getScheme() const;
};