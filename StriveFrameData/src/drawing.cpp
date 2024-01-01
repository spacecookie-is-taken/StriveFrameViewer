#include "drawing.h"
#include <UE4SSProgram.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#include <format>

/* Debug Stuff */
#if 0
#define DEBUG_PRINT(...) RC::Output::send<LogLevel::Warning>(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

/* Combo Triger Settings */
constexpr int COMBO_ENDED_TIME = 20;  // time idle before combo is considered over
constexpr int COMBO_NUM_TIME = 5;     // minimum time of unchanging states to display state length
constexpr int COMBO_TRUNC_TIME = 10;  // max segments for unchanging state, after which the segments are truncated
constexpr double COLOR_DECAY = 0.9f;
constexpr double BAR_TEXT_SIZE = 0.7;

/* Unprojected coordinate Display Settings */
constexpr int FRAME_SEGMENTS = 120;
constexpr int FADE_DISTANCE = 5;
constexpr int SEG_WIDTH = 10;
constexpr int SEG_HEIGHT = 18;
constexpr int SEG_SPACING = 2;
constexpr int BAR_SPACING = 4;
constexpr int BORDER_THICKNESS = 2;

constexpr int SEG_TOTAL = SEG_SPACING + SEG_WIDTH;
constexpr int BAR_WIDTH = (FRAME_SEGMENTS * SEG_TOTAL) + SEG_SPACING;
constexpr int BAR_HEIGHT = 2 * SEG_SPACING + SEG_HEIGHT;
constexpr int BAR_RIGHT = BAR_WIDTH / 2;
constexpr int BAR_LEFT = -1 * BAR_RIGHT;

constexpr int BAR_TWO_TOP = BAR_SPACING / 2;
constexpr int BAR_TWO_CENTER = BAR_TWO_TOP + (BAR_HEIGHT / 2);
constexpr int BAR_TWO_BOTTOM = BAR_TWO_TOP + BAR_HEIGHT;

constexpr int BAR_ONE_BOTTOM = -1 * BAR_TWO_TOP;
constexpr int BAR_ONE_CENTER = -1 * BAR_TWO_CENTER;
constexpr int BAR_ONE_TOP = -1 * BAR_TWO_BOTTOM;

constexpr int SEGS_TWO_TOP = BAR_TWO_TOP + SEG_SPACING;
constexpr int SEGS_TWO_BOTTOM = BAR_TWO_BOTTOM - SEG_SPACING;

constexpr int SEGS_ONE_TOP = BAR_ONE_TOP + SEG_SPACING;
constexpr int SEGS_ONE_BOTTOM = BAR_ONE_BOTTOM - SEG_SPACING;

constexpr int INFO_ONE_LOC = BAR_ONE_TOP - BAR_HEIGHT;
constexpr int INFO_TWO_LOC = BAR_TWO_BOTTOM + 2;

/* Projected Display Settings */
constexpr double EXPECTED_DISP_RATIO = 16.0 / 9.0;
constexpr double PROJECTED_RATIO = 0.0006f;
constexpr double CENTER_X_RATIO = 0.5f;
constexpr double CENTER_Y_RATIO = 0.8f;

/* Unreal Structs */
struct FLinearColor {
  float R = 0.f, G = 0.f, B = 0.f, A = 0.f;

  FLinearColor operator*(float mod) const {
    FLinearColor result = *this;
    result.R *= mod;
    result.G *= mod;
    result.B *= mod;
    return result;
  }
};

struct DrawRectParams {
  FLinearColor RectColor;
  float ScreenX;
  float ScreenY;
  float ScreenW;
  float ScreenH;
};

struct DrawTextParams {
  RC::Unreal::FString Text;
  FLinearColor TextColor;
  float ScreenX;
  float ScreenY;
  void* Font;
  float Scale;
  bool bScalePosition;
};

/* Unreal Constants */
FLinearColor color_invisible{1.f, 1.f, 1.f, 0.f};
FLinearColor color_white{1.f, 1.f, 1.f, 1.f};
FLinearColor color_purple{0.59f, 0.19f, 0.78f, 1.f};
FLinearColor background_color{0.05f, 0.05f, 0.05f, 0.7f};
static const FLinearColor state_colors[] = {
    FLinearColor{.2f, .2f, .2f, .9f},  // Gray
    FLinearColor{.1f, .1f, .8f, .9f},  // Blue
    FLinearColor{.1f, .6f, .1f, .9f},  // Green
    FLinearColor{.7f, .7f, .1f, .9f},  // Yellow
    FLinearColor{.8f, .1f, .1f, .9f},  // Red
    FLinearColor{.8f, .4f, .1f, .9f}   // Orange
};

/* Classes */

class DrawTool {
  double center_x = 0.0;
  double center_y = 0.0;
  double units = 0.0;
  RC::Unreal::UObject* hud_actor = nullptr;
  RC::Unreal::UObject* font = nullptr;
  RC::Unreal::UFunction* drawrect_func = nullptr;
  RC::Unreal::UFunction* drawtext_func = nullptr;

 public:
  DrawTool() = default;
  DrawTool(const GetSizeParams& SizeData, RC::Unreal::UFunction* drawrect, RC::Unreal::UFunction* drawtext, RC::Unreal::UObject* fontobject) {
    hud_actor = nullptr;
    font = fontobject;
    drawrect_func = drawrect;
    drawtext_func = drawtext;
    update(SizeData);
  }

  void update(const GetSizeParams& SizeData, RC::Unreal::UObject* hud = nullptr) {
    hud_actor = hud;
    auto x = SizeData.SizeX;
    auto y = SizeData.SizeY;

    // Strive always renders to a 16:9 region, we need to fix this here since our size data is the "true" screen space window size
    const double actual_ratio = x / y;
    if (actual_ratio > EXPECTED_DISP_RATIO) {  // 21:9 Ultrawide monitor probably
      x = y * EXPECTED_DISP_RATIO;
    } else if (actual_ratio < EXPECTED_DISP_RATIO) {  // 4:3 or 16:10 monitor probably
      y = x / EXPECTED_DISP_RATIO;
    }

    center_x = x * CENTER_X_RATIO;
    center_y = y * CENTER_Y_RATIO;
    units = x * PROJECTED_RATIO;
  }

  void drawRect(int x, int y, int width, int height, const FLinearColor& color) const {
    double prj_x = center_x + (units * x);
    double prj_y = center_y + (units * y);
    double prj_width = units * width;
    double prj_height = units * height;

    DrawRectParams rect_params{color, prj_x, prj_y, prj_width, prj_height};
    hud_actor->ProcessEvent(drawrect_func, &rect_params);
  }

  void drawText(int x, int y, const std::wstring& text, double scale) const {
    RC::Unreal::FString Text(text.c_str());

    double prj_x = center_x + (units * x);
    double prj_y = center_y + (units * y);
    double front_scale = units * scale;

    // TODO: scale text off screen resolution
    DrawTextParams params{Text, color_white, prj_x, prj_y, font, front_scale, false};

    hud_actor->ProcessEvent(drawtext_func, &params);
  }
};

enum PlayerStateType {
  PST_Idle = 0,
  PST_BlockStunned,
  PST_HitStunned,
  PST_Busy,
  PST_Attacking,
  PST_Recovering,
  PST_None,
  PST_End
};

class PlayerState {
  int time;

 public:
  bool projectile_active;
  bool hitstop;
  PlayerStateType type;
  int state_time;

  PlayerState()
  : time(-1)
  , projectile_active(false)
  , hitstop(false)
  , type(PST_Idle)
  , state_time(0) {}

  PlayerState(asw_player& player, const PlayerState& last, bool proj_active) {
    bool canact = player.can_act();
    bool block_stunned = player.is_in_blockstun() || player.is_stagger() || player.is_guard_crush();
    bool hit_stunned = player.is_in_hitstun() || player.is_knockdown();
    bool player_active = player.is_active();

    time = player.action_time;
    projectile_active = !player_active && proj_active;

    bool recovery = time >= last.time && (last.type == PST_Recovering || (last.type == PST_Attacking && !player_active) || (!last.projectile_active && projectile_active));
    hitstop = (time == last.time) && (player_active || projectile_active);

    if (canact) type = PST_Idle;
    else if (block_stunned) type = PST_BlockStunned;
    else if (hit_stunned) type = PST_HitStunned;
    else if (player_active) type = PST_Attacking;
    else if (recovery) type = PST_Recovering;
    else type = PST_Busy;

    if (last.type == type && last.projectile_active == projectile_active) {
      state_time = (last.state_time < 500) ? last.state_time + 1 : last.state_time;
    } else {
      state_time = 1;
    }
  }
};

void DebugPrintState(const PlayerState& f) {
  DEBUG_PRINT(STR("can:{}, bs:{}, hs:{}, act:{}, t:{}, r:{}, hit:{}, st:{}\n"), f.canact, f.block_stunned, f.hit_stunned, f.active, f.time, f.recovery, f.hitstop, f.state_time);
}

struct FrameState {
  struct FrameInfo {
    FLinearColor color;
    FLinearColor border;
    int trunc = 0;
  };
  struct MoveStats {
    int startup = 0;
    int active = 0;
    int recovery = 0;
  };
  std::pair<FrameInfo, FrameInfo> segments[FRAME_SEGMENTS];
  std::pair<MoveStats, MoveStats> stats;
  std::pair<PlayerState, PlayerState> previous_state;
  std::pair<PlayerState, PlayerState> current_state;

  // last move stats
  int advantage = 0;

  bool active = false;
  int current_segment_idx = 0;

  FrameState() { resetFrames(); }

  void resetFrames() {
    advantage = 0;
    current_segment_idx = 0;
    current_state.first.state_time = 1;
    current_state.second.state_time = 1;
    previous_state.first.state_time = 1;
    previous_state.second.state_time = 1;
    stats = std::make_pair(MoveStats(), MoveStats());
    for (int idx = 0; idx < FRAME_SEGMENTS; ++idx) {
      segments[idx] = std::make_pair(FrameInfo(), FrameInfo());
    }
  }

  void updateActiveSection(const PlayerState& state, int previous_time, FrameInfo& active, FrameInfo& previous) {
    // previous section has ended
    previous.trunc = 0;

    if (state.state_time == 1) {
      DEBUG_PRINT(STR("New Section for One\n"));
      active.color = state_colors[state.type];
      // ... and was long enough that we want to print length
      if (previous_time > COMBO_NUM_TIME) {
        DEBUG_PRINT(STR("Last section requires truncating\n"));
        previous.trunc = previous_time;
      }
    } else {
      DEBUG_PRINT(STR("Same Section for One\n"));
      // we are drawing this section, fade its color slightly
      // active_segment.color_one = state_colors[type_one] * COLOR_DECAY;
      active.color = previous.color * COLOR_DECAY;
    }
    active.border = state.projectile_active ? color_purple : color_invisible;
  }
  void updateStats(const PlayerState& state, MoveStats& stats) {
    if (state.type == PST_Attacking) {
      stats.active = state.state_time;
    } else if (state.type == PST_Recovering) {
      stats.recovery = state.state_time;
    } else if (state.type == PST_Busy) {
      stats.startup = state.state_time;
    }
  }

  void addFrame(asw_player& player_one, asw_player& player_two, bool player_one_proj, bool player_two_proj) {
    // update states
    if (!current_state.first.hitstop && !current_state.second.hitstop) {
      previous_state.first = current_state.first;
      previous_state.second = current_state.second;
    }
    current_state.first = PlayerState(player_one, previous_state.first, player_one_proj);
    current_state.second = PlayerState(player_two, previous_state.second, player_two_proj);

    // end combo if we've been idle for a long time
    if (current_state.first.type == PST_Idle && current_state.second.type == PST_Idle) {
      if (!active) {
        return;
      }
      if (current_state.first.state_time > COMBO_ENDED_TIME && current_state.second.state_time > COMBO_ENDED_TIME) {
        DEBUG_PRINT(STR("Combo ended\n"));
        active = false;
        return;
      }
    }
    // reset if this is a new combo
    else if (!active) {
      DEBUG_PRINT(STR("Combo reset\n"));
      resetFrames();
    }

    DEBUG_PRINT(STR("Frame {}\n"), current_segment_idx);
    DebugPrintState(current_state.first);
    DebugPrintState(current_state.second);

    // ignore hitstop time
    if (current_state.first.hitstop || current_state.second.hitstop) {
      DEBUG_PRINT(STR("Skipping Hitstop\n"));
      return;
    }

    // update move info
    updateStats(current_state.first, stats.first);
    updateStats(current_state.second, stats.second);

    // update advantage
    if (current_state.first.type == PST_Idle) {
      if (!current_state.second.type == PST_Idle) {
        ++advantage;
      }
    } else {
      if (current_state.second.type == PST_Idle) {
        --advantage;
      } else {
        advantage = 0;
      }
    }

    // update frame display

    auto& active_segment = segments[current_segment_idx];

    // only trigger truncation logic if mid-combo
    if (active) {
      DEBUG_PRINT(STR("Is Active\n"));
      auto& prev_segment = segments[(current_segment_idx + FRAME_SEGMENTS - 1) % FRAME_SEGMENTS];

      // we are truncating, update truncated
      if (current_state.first.state_time > COMBO_TRUNC_TIME && current_state.second.state_time > COMBO_TRUNC_TIME) {
        prev_segment.first.trunc = current_state.first.state_time;
        prev_segment.second.trunc = current_state.second.state_time;
        DEBUG_PRINT(STR("Truncating 1:{}, 2:{}\n"), prev_segment.trunc_one, prev_segment.trunc_two);
        return;
      }

      updateActiveSection(current_state.first, previous_state.first.state_time, active_segment.first, prev_segment.first);
      updateActiveSection(current_state.second, previous_state.second.state_time, active_segment.second, prev_segment.second);
    } else {
      DEBUG_PRINT(STR("Was not Active\n"));
      active_segment.first.color = state_colors[current_state.first.type];
      active_segment.first.border = current_state.first.projectile_active ? color_purple : color_invisible;
      active_segment.second.color = state_colors[current_state.second.type];
      active_segment.second.border = current_state.second.projectile_active ? color_purple : color_invisible;
    }

    // fade effect, clear segments near tail
    auto& fade_segment = segments[(current_segment_idx + FADE_DISTANCE) % FRAME_SEGMENTS];
    fade_segment.first.color.A = 0.f;
    fade_segment.second.color.A = 0.f;


    // advance to next segment
    if (++current_segment_idx >= FRAME_SEGMENTS) {
      DEBUG_PRINT(STR("Cycling segment index\n"));
      current_segment_idx = 0;
    }

    active = true;
  }
};

/* Statics */

FrameState state_data;
DrawTool tool;

/* ABI */

void initFrames(const GetSizeParams& sizedata, RC::Unreal::UFunction* drawrect, RC::Unreal::UFunction* drawtext, RC::Unreal::UObject* fontobject) {
  tool = DrawTool(sizedata, drawrect, drawtext, fontobject);
}

void addFrame() {
  const auto engine = asw_engine::get();
  if (!engine) return;

  asw_player* player_one = engine->players[0].entity;
  asw_player* player_two = engine->players[1].entity;
  if (!player_one || !player_two) return;

  bool player_one_proj = false, player_two_proj = false;
  for (int idx = 0; idx < engine->entity_count; ++idx) {
    const auto *focus = engine->entities[idx], *parent = focus->parent_obj;

    player_one_proj |= (parent == player_one && focus->is_active());
    player_two_proj |= (parent == player_two && focus->is_active());
  }

  state_data.addFrame(*player_one, *player_two, player_one_proj, player_two_proj);
}

void resetFrames() {
  state_data.active = false;
  state_data.current_state.first = PlayerState();
  state_data.current_state.second = PlayerState();
}

void drawFrame(const FrameState::FrameInfo& info, int top, int left) {
  if (info.color.A != 0.f) {
    if (info.border.A != 0.f) {
      tool.drawRect(left-BORDER_THICKNESS, top-BORDER_THICKNESS, SEG_WIDTH + 2*BORDER_THICKNESS, SEG_HEIGHT + 2*BORDER_THICKNESS, info.border);
    }
    tool.drawRect(left, top, SEG_WIDTH, SEG_HEIGHT, info.color);
    if (info.trunc > 0) {
      auto text = std::to_wstring(info.trunc);
      int text_left = left - (text.size() - 1) * 8 + 1;
      tool.drawText(text_left, top, text, BAR_TEXT_SIZE);
    }
  }
}

std::wstring MakeStatsText(const FrameState::MoveStats& stats, int advantage) {
  return std::format(L"Startup: {}, Active: {}, Recovery: {}, Advantage: {}", stats.startup, stats.active, stats.recovery, advantage);
}

void drawFrames(RC::Unreal::UObject* hud, const GetSizeParams& sizedata) {
  tool.update(sizedata, hud);

  auto player_one_info = MakeStatsText(state_data.stats.first, state_data.advantage);
  auto player_two_info = MakeStatsText(state_data.stats.second, state_data.advantage);

  tool.drawText(BAR_LEFT, INFO_ONE_LOC, player_one_info, BAR_TEXT_SIZE);
  tool.drawText(BAR_LEFT, INFO_TWO_LOC, player_two_info, BAR_TEXT_SIZE);

  tool.drawRect(BAR_LEFT, BAR_ONE_TOP, BAR_WIDTH, BAR_HEIGHT, background_color);
  tool.drawRect(BAR_LEFT, BAR_TWO_TOP, BAR_WIDTH, BAR_HEIGHT, background_color);

  for (int idx = 0; idx < FRAME_SEGMENTS; ++idx) {
    int left = BAR_LEFT + (SEG_TOTAL * idx) + SEG_SPACING;
    const auto& segment = state_data.segments[idx];

    drawFrame(segment.first, SEGS_ONE_TOP, left);
    drawFrame(segment.second, SEGS_TWO_TOP, left);
  }
}

void drawConfigure() {
  tool.drawText(-60, SEGS_ONE_TOP - 40, L"Configuring reset input", 1.2);
}