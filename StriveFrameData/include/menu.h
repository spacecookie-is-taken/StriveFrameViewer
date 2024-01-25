#pragma once

#include "draw_utils.h"

class ModMenu {
  DrawContext tool;

  std::vector<int> settings;

  bool is_showing = false;
  int cursor_position = 0;

  void changeSetting(size_t idx, bool right=true);

public:
  ModMenu();

  void update(bool bar_toggled, bool hitbox_toggled, bool menu_toggled);
  void draw();

  bool barEnabled() const { return settings[0]; }
  bool hitboxEnabled() const { return settings[1]; }
  bool fadeEnabled() const { return settings[2]; }
  int colorScheme() const { return settings[3]; }
};