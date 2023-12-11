#include "drawing.h"

/* Combo Triger Settings */
constexpr int COMBO_ENDED_TIME = 20;  // time idle before combo is considered over
constexpr int COMBO_NUM_TIME = 5;     // minimum time of unchanging states to display state length
constexpr int COMBO_TRUNC_TIME = 10;  // max segments for unchanging state, after which the segments are truncated
constexpr double COLOR_DECAY = 0.9f;

/* Unprojected coordinate Display Settings */
constexpr int FRAME_SEGMENTS = 120;
constexpr int FADE_DISTANCE = 5;
constexpr int SEG_WIDTH = 10;
constexpr int SEG_HEIGHT = 18;
constexpr int SEG_SPACING = 2;
constexpr int BAR_SPACING = 4;

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

/* Projected Display Settings */
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

FLinearColor color_white{1.f, 1.f, 1.f, 1.f};
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

struct DrawTool {
  double center_x = 0.0;
  double center_y = 0.0;
  double units = 0.0;
  RC::Unreal::UObject* hud_actor = nullptr;
  RC::Unreal::UFunction* drawrect_func = nullptr;
  RC::Unreal::UFunction* drawtext_func = nullptr;

  DrawTool() = default;
  DrawTool(const GetSizeParams& SizeData, RC::Unreal::UFunction* drawrect, RC::Unreal::UFunction* drawtext) {
    hud_actor = nullptr;
    drawrect_func = drawrect;
    drawtext_func = drawtext;
    updateSize(SizeData);
  }

  void updateSize(const GetSizeParams& SizeData) {
    center_x = SizeData.SizeX * CENTER_X_RATIO;
    center_y = SizeData.SizeY * CENTER_Y_RATIO;
    units = SizeData.SizeX * PROJECTED_RATIO;
  }

  void drawRect(int x, int y, int width, int height, const FLinearColor& color) const {
    double prj_x = center_x + (units * x);
    double prj_y = center_y + (units * y);
    double prj_width = units * width;
    double prj_height = units * height;

    DrawRectParams rect_params{color, prj_x, prj_y, prj_width, prj_height};
    hud_actor->ProcessEvent(drawrect_func, &rect_params);
  }

  void drawText(int x, int y, const std::wstring& text) const {
    RC::Unreal::FString Text(text.c_str());

    x -= (text.size() - 1) * 5 - 2;

    double prj_x = center_x + (units * x);
    double prj_y = center_y + (units * y);

    // TODO: scale text off screen resolution
    DrawTextParams params{Text, color_white, prj_x, prj_y, nullptr, units * 1.2, false};

    hud_actor->ProcessEvent(drawtext_func, &params);
  }
};

PlayerState::PlayerState()
: canact(false)
, block_stunned(false)
, hit_stunned(false)
, active(false)
, time(-1)
, recovery(false)
, hitstop(false)
, state_time(0)
, inactive_time(0) {
}

PlayerState::PlayerState(asw_player& player, const PlayerState& last, bool proj_active)
: canact(player.can_act())
, block_stunned(player.is_in_blockstun() || player.is_stagger() || player.is_guard_crush())
, hit_stunned(player.is_in_hitstun() || player.is_knockdown() || player.is_roll())
, active(player.is_active() || proj_active) {
  time = player.action_time;
  recovery = ((last.active && !active) || last.recovery) && time >= last.time;
  hitstop = active && (time == last.time);
  if (last.getType() == getType()) {  //  && (getType() != PST_Busy || time > 1)
    state_time = last.state_time + 1;
    if (state_time > 1000) {
      state_time = 1000;
    }
  } else {
    state_time = 0;
  }
}

PlayerStateType PlayerState::getType() const {
  if (canact) return PST_Idle;
  else if (block_stunned) return PST_BlockStunned;
  else if (hit_stunned) return PST_HitStunned;
  else if (active) return PST_Attacking;
  else if (recovery) return PST_Recovering;
  else return PST_Busy;
}

struct FrameState {
  struct FrameInfo {
    FLinearColor color_one;
    FLinearColor color_two;
    int trunc_one = 0;
    int trunc_two = 0;
  };
  FrameInfo segments[FRAME_SEGMENTS];

  PlayerState previous_state_one;
  PlayerState previous_state_two;
  PlayerState current_state_one;
  PlayerState current_state_two;

  bool active = false;
  int current_segment_idx = 0;

  FrameState() { resetFrames(); }

  void resetFrames() {
    current_segment_idx = 0;
    current_state_one.state_time = 1;
    current_state_two.state_time = 1;
    previous_state_one.state_time = 0;
    previous_state_two.state_time = 0;
    for (int idx = 0; idx < FRAME_SEGMENTS; ++idx) {
      segments[idx] = FrameInfo();
    }
  }

  void addFrame(asw_player& player_one, asw_player& player_two, bool player_one_proj, bool player_two_proj) {
    // update states
    previous_state_one = current_state_one;
    current_state_one = PlayerState(player_one, previous_state_one, player_one_proj);

    previous_state_two = current_state_two;
    current_state_two = PlayerState(player_two, previous_state_two, player_two_proj);

    auto type_one = current_state_one.getType();
    auto type_two = current_state_two.getType();
    auto time_one = current_state_one.state_time;
    auto time_two = current_state_two.state_time;

    // end combo if we've been idle for a long time
    if (type_one == PST_Idle && type_two == PST_Idle) {
      if (!active) return;
      if (time_one >= COMBO_ENDED_TIME && time_two >= COMBO_ENDED_TIME) {
        active = false;
        return;
      }
    }
    // reset if this is a new combo
    else if (!active) {
      resetFrames();
    }

    // ignore hitstop time
    if (current_state_one.hitstop || current_state_two.hitstop) {
      --current_state_one.state_time;
      --current_state_two.state_time;
      return;
    }

    auto& active_segment = segments[current_segment_idx];

    // only trigger truncation logic if mid-combo
    if (active) {
      auto& prev_segment = segments[(current_segment_idx + FRAME_SEGMENTS - 1) % FRAME_SEGMENTS];

      // we are truncating, update truncated
      if (time_one >= COMBO_TRUNC_TIME && time_two >= COMBO_TRUNC_TIME) {
        prev_segment.trunc_one = time_one + 1;
        prev_segment.trunc_two = time_two + 1;
        return;
      }

      // if we were truncating, we aren't anymore
      prev_segment.trunc_one = 0;
      prev_segment.trunc_two = 0;

      // previous section has ended
      if (time_one == 0) {
        active_segment.color_one = state_colors[type_one];
        // ... and was long enough that we want to print length
        if (previous_state_one.state_time >= COMBO_NUM_TIME) {
          prev_segment.trunc_one = previous_state_one.state_time + 1;
        }
      } else {
        // we are drawing this section, fade its color slightly
        // active_segment.color_one = state_colors[type_one] * COLOR_DECAY;
        active_segment.color_one = prev_segment.color_one * COLOR_DECAY;
      }

      if (time_two == 0) {
        active_segment.color_two = state_colors[type_two];
        // ... and was long enough that we want to print length
        if (previous_state_two.state_time >= COMBO_NUM_TIME) {
          prev_segment.trunc_two = previous_state_two.state_time + 1;
        }
      } else {
        // we are drawing this section, fade its color slightly
        // active_segment.color_two = state_colors[type_two] * COLOR_DECAY;
        active_segment.color_two = prev_segment.color_two * COLOR_DECAY;
      }
    } else {
      active_segment.color_one = state_colors[type_one];
      active_segment.color_two = state_colors[type_two];
    }

    // fade effect, clear segments near tail
    auto& fade_segment = segments[(current_segment_idx + FADE_DISTANCE) % FRAME_SEGMENTS];
    fade_segment.color_one.A = 0.f;
    fade_segment.color_two.A = 0.f;

    // advance to next segment
    if (++current_segment_idx >= FRAME_SEGMENTS) {
      current_segment_idx = 0;
    }

    active = true;
  }
};

/* Statics */

FrameState state_data;
DrawTool tool;

/* ABI */

void initFrames(const GetSizeParams& sizedata, RC::Unreal::UFunction* drawrect, RC::Unreal::UFunction* drawtext) {
  tool = DrawTool(sizedata, drawrect, drawtext);
}

void updateSize(const GetSizeParams& sizedata) {
  tool.updateSize(sizedata);
}

void addFrame(asw_player& player_one, asw_player& player_two, bool player_one_proj, bool player_two_proj) {
  state_data.addFrame(player_one, player_two, player_one_proj, player_two_proj);
}

void resetFrames() {
  state_data.active = false;
  state_data.current_state_one = PlayerState();
  state_data.current_state_two = PlayerState();
}

void drawFrames(RC::Unreal::UObject* hud) {
  tool.hud_actor = hud;
  if (!tool.drawrect_func) {
    throw std::runtime_error("DrawTool was unitialized");
  }

  tool.drawRect(BAR_LEFT, BAR_ONE_TOP, BAR_WIDTH, BAR_HEIGHT, background_color);
  tool.drawRect(BAR_LEFT, BAR_TWO_TOP, BAR_WIDTH, BAR_HEIGHT, background_color);

  for (int idx = 0; idx < FRAME_SEGMENTS; ++idx) {
    int left = BAR_LEFT + (SEG_TOTAL * idx) + SEG_SPACING;
    const auto& info = state_data.segments[idx];
    if (info.color_one.A != 0.f) {
      tool.drawRect(left, SEGS_ONE_TOP, SEG_WIDTH, SEG_HEIGHT, info.color_one);
      if (info.trunc_one > 0) {
        tool.drawText(left, SEGS_ONE_TOP, std::to_wstring(info.trunc_one));
      }
    }
    if (info.color_two.A != 0.f) {
      tool.drawRect(left, SEGS_TWO_TOP, SEG_WIDTH, SEG_HEIGHT, info.color_two);
      if (info.trunc_two > 0) {
        tool.drawText(left, SEGS_TWO_TOP, std::to_wstring(info.trunc_two));
      }
    }
  }
}
