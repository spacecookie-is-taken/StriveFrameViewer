#include "menu.h"

class RedInputChecker {
  unsigned short novel_inputs;
public:
  enum InputFlag {
    IF_Up = 0x1,
    IF_Down = 0x2,
    IF_Left = 0x4,
    IF_Right = 0x8,
    IF_P = 0x10,
    IF_K = 0x20,
    IF_S = 0x40,
    IF_H = 0x80,
    IF_Dust = 0x100,
    IF_ANY = 0x1FF
  };
  RedInputChecker(asw_inputs& inputs){
    novel_inputs = inputs.m_CurRecFlg & (~inputs.m_PreRecFlg);
  }
  bool checkInput(InputFlag flag) const { return novel_inputs & flag; }

  /*const IconLoc& getDirLoc() const {
    int offset = 0;
    if(checkInput(IF_Up)) offset += 3;
    else if(checkInput(IF_Down)) offset += 6;

    if(checkInput(IF_Left)) offset += 1;
    else if(checkInput(IF_Right)) offset += 2;

    return ICON_DIR_LOCS[offset];
  }
  const IconLoc& getButtonLoc() const {
    if(checkInput(IF_P)) return ICON_BUTTON_P_LOC;
    if(checkInput(IF_K)) return ICON_BUTTON_K_LOC;
    if(checkInput(IF_S)) return ICON_BUTTON_S_LOC;
    if(checkInput(IF_H)) return ICON_BUTTON_H_LOC;
    if(checkInput(IF_Dust)) return ICON_BUTTON_D_LOC;
    return ICON_NULL_LOC;
  }*/
  void debug() const {
    RC::Output::send<LogLevel::Warning>(STR("Input: {}\n"), novel_inputs);
  }
};


struct OptionData {
  const wchar_t* title;
  size_t count;
  std::array<const wchar_t*, 5> values;
};

namespace {
  constexpr const std::array<OptionData, 4> options = {
    OptionData{L"Frame Bar:",     2, {L"< Disabled   >",   L"< Enabled    >"}},
    OptionData{L"Hitboxes:",      2, {L"< Disabled   >",   L"< Enabled    >"}},
    OptionData{L"Fade Effect:",   2, {L"< Disabled   >",   L"< Enabled    >"}},
    OptionData{L"Color Scheme:",  4, {L"< SF6        >", L"< Classic    >", L"< Dustloop   >", L"< Colorblind >"}},
  };

  constexpr size_t OPTION_COUNT = options.size();

  constexpr int buildTitleWidth() {
    int max_width = 0;
    for(const auto& op : options){
      max_width = std::wstring_view(op.title).size();
    }
    return max_width;
  }

  constexpr int buildValueWidth() {
    int max_width = 0;
    for(const auto& op : options){
      for(size_t idx = 0; idx < op.count; idx++){
        max_width = std::wstring_view(op.values[idx]).size();
      }
    }
    return max_width;
  }

  constexpr double CENTER_X_RATIO = 0.5f;
  constexpr double CENTER_Y_RATIO = 0.5f;
  constexpr double MENU_PADDING = 30;
  constexpr double OPTION_HEIGHT = 60;
  constexpr double OPTION_SPACING = 20;
  constexpr double COL_SPACING = 20;

  constexpr double TITLE_WIDTH = buildTitleWidth() * 24;
  constexpr double VALUE_WIDTH = buildValueWidth() * 24;
  constexpr double MENU_WIDTH = TITLE_WIDTH + COL_SPACING + VALUE_WIDTH + (2*MENU_PADDING);
  
  constexpr double MENU_HEIGHT = 2* MENU_PADDING + OPTION_COUNT * (OPTION_HEIGHT + OPTION_SPACING) - OPTION_SPACING;
  constexpr double MENU_TOP = -MENU_HEIGHT/2;
  constexpr double MENU_LEFT = -MENU_WIDTH/2;
  constexpr double OPTION_LEFT = MENU_LEFT + MENU_PADDING;
  constexpr double VALUE_LEFT = OPTION_LEFT + TITLE_WIDTH + COL_SPACING;

  FLinearColor background_color{0.2f, 0.2f, 0.2f, 0.8f};
  FLinearColor cursor_color{0.5f, 0.0f, 0.0f, 1.0f};

  static const Pallete color_palletes[] = {
    Pallete{ // SF6
      FLinearColor{(201.f/255.f), (128.f/255.f), (0.f/255.f), 1.f}, 
      {
        FLinearColor{(26.f/255.f), (26.f/255.f), (26.f/255.f), 1.f}, // IDLE
        FLinearColor{(253.f/255.f), (245.f/255.f), (46.f/255.f), 1.f}, // Block
        FLinearColor{(253.f/255.f), (245.f/255.f), (46.f/255.f), 1.f}, // Hit
        FLinearColor{(1.f/255.f), (182.f/255.f), (149.f/255.f), 1.f}, // Busy
        FLinearColor{(205.f/255.f), (43.f/255.f), (103.f/255.f), 1.f}, // Attacking
        FLinearColor{(1.f/255.f), (111.f/255.f), (188.f/255.f), 1.f}, // Projectile
        FLinearColor{(1.f/255.f), (111.f/255.f), (188.f/255.f), 1.f}  // Recovering
      }
    },
    Pallete{ // CLASSIC
      FLinearColor{.8f, .1f, .1f, 1.f}, 
      {
        FLinearColor{.2f, .2f, .2f, .9f}, // IDLE
        FLinearColor{.1f, .1f, .8f, .9f}, // Block
        FLinearColor{.1f, .6f, .1f, .9f}, // Hit
        FLinearColor{.7f, .7f, .1f, .9f}, // Busy
        FLinearColor{.8f, .1f, .1f, .9f}, // Attacking
        FLinearColor{.8f, .4f, .1f, .9f}, // Projectile
        FLinearColor{.8f, .4f, .1f, .9f}  // Recovering
      }
    },
    Pallete{ // DUSTLOOP
      FLinearColor{(255.f/255.f), (0.f/255.f), (0.f/255.f), 1.f}, 
      {
        FLinearColor{(128.f/255.f), (128.f/255.f), (128.f/255.f), 1.f}, // IDLE
        FLinearColor{(233.f/255.f), (215.f/255.f), (4.f/255.f), 1.f}, // Block
        FLinearColor{(233.f/255.f), (215.f/255.f), (4.f/255.f), 1.f}, // Hit
        FLinearColor{(54.f/255.f), (179.f/255.f), (126.f/255.f), 1.f}, // Busy
        FLinearColor{(255.f/255.f), (93.f/255.f), (93.f/255.f), 1.f}, // Attacking
        FLinearColor{(0.f/255.f), (105.f/255.f), (182.f/255.f), 1.f}, // Projectile
        FLinearColor{(0.f/255.f), (105.f/255.f), (182.f/255.f), 1.f}  // Recovering
      }
    },
    Pallete{ // COLORBLIND
      FLinearColor{.8f, .1f, .1f, 1.f}, 
      {
        FLinearColor{.2f, .2f, .2f, .9f}, // IDLE
        FLinearColor{.1f, .1f, .8f, .9f}, // Block
        FLinearColor{.1f, .6f, .1f, .9f}, // Hit
        FLinearColor{.7f, .7f, .1f, .9f}, // Busy
        FLinearColor{.8f, .1f, .1f, .9f}, // Attacking
        FLinearColor{.8f, .4f, .1f, .9f}, // Projectile
        FLinearColor{.8f, .4f, .1f, .9f}  // Recovering
      }
    },
  };
}

size_t rotateVal(size_t val, bool positive, size_t max){
  if(positive){
    return (val + 1) % max;
  }
  else if(val == 0){
    return max - 1;
  }
  else {
    return val - 1;
  }
}

void ModMenu::changeSetting(size_t idx, bool right) {
  auto& relevant = settings[idx];
  relevant = rotateVal(relevant, right, options[idx].count);
}

ModMenu& ModMenu::instance(){
  static ModMenu me;
  return me;
}

ModMenu::~ModMenu() = default;
ModMenu::ModMenu() 
: tool(CENTER_X_RATIO, CENTER_Y_RATIO) {
  settings.resize(OPTION_COUNT, 1);
}

void ModMenu::update(bool bar_toggled, bool hitbox_toggled, bool menu_toggled) {
  if(bar_toggled) changeSetting(0);
  if(hitbox_toggled) changeSetting(1);
  if(menu_toggled) is_showing = !is_showing;

  if(!is_showing) return;

  // get button inputs and update cursor
  auto checker = RedInputChecker(asw_engine::get()->inputs[0]);
  if(checker.checkInput(RedInputChecker::IF_Up)) cursor_position = rotateVal(cursor_position, false, OPTION_COUNT);
  if(checker.checkInput(RedInputChecker::IF_Down)) cursor_position = rotateVal(cursor_position, true, OPTION_COUNT);
  if(checker.checkInput(RedInputChecker::IF_Left)) changeSetting(cursor_position, false);
  if(checker.checkInput(RedInputChecker::IF_Right)) changeSetting(cursor_position, true);
}

void ModMenu::draw() {
  if(!is_showing) return;

  tool.update();

  tool.drawRect(MENU_LEFT, MENU_TOP, MENU_WIDTH, MENU_HEIGHT, background_color);

  // draw cursor
  double cursor_top = MENU_TOP + MENU_PADDING + cursor_position*(OPTION_HEIGHT+OPTION_SPACING);
  tool.drawRect(OPTION_LEFT, cursor_top, MENU_WIDTH - (2*MENU_PADDING), OPTION_HEIGHT, cursor_color);

  for(size_t idx=0;idx<OPTION_COUNT;idx++){
    auto& relevant = options[idx];
    double top = MENU_TOP + MENU_PADDING + idx*(OPTION_HEIGHT+OPTION_SPACING);
    tool.drawOutlinedText(OPTION_LEFT, top, FString(relevant.title), 2.0);
    tool.drawOutlinedText(VALUE_LEFT, top, FString(relevant.values[settings[idx]]), 2.0);
  }
}

const Pallete& ModMenu::getScheme() const {
  return color_palletes[settings[3]];
}