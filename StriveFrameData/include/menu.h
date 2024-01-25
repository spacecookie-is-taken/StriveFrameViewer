#pragma once

#include "draw_utils.h"

struct Pallete {
  FLinearColor projectile_color;
  FLinearColor state_colors[7];
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

  bool barEnabled() const { return settings[0]; }
  bool hitboxEnabled() const { return settings[1]; }
  bool fadeEnabled() const { return settings[2]; }
  int colorScheme() const { return settings[3]; }

  const Pallete& getScheme() const;
};