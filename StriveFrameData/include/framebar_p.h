#pragma once
#include "framebar.h"
#include "draw_utils.h"
#include "arcsys.h"

#include <utility>

constexpr int FRAME_SEGMENTS = 120;

enum PlayerStateType
{
  PST_Idle = 0,
  PST_BlockStunned,
  PST_HitStunned,
  PST_Busy,
  PST_Attacking,
  PST_ProjectileAttacking,
  PST_Recovering,
  PST_None,
  PST_End
};

struct ProjectileTracker
{
  struct ProjectileInfo
  {
    asw_entity *direct_parent;
    asw_entity *root_parent;
    bool alive;
    int old;           // 0: brand new (protected), 1: not new, 2: marked old
    int hit_delay = 2; // this entity has hit something already and should not be considered for further damage

    ProjectileInfo(asw_entity *source)
    {
      direct_parent = source->parent_obj;
      root_parent = direct_parent;
      alive = true;
      old = 0;
    }
    ProjectileInfo(asw_entity *source, const std::pair<asw_entity *, ProjectileInfo> &parent_info)
    {
      direct_parent = parent_info.first;
      root_parent = parent_info.second.root_parent;
      alive = true;
      old = parent_info.second.old;
    }
  };

  std::unordered_map<asw_entity *, ProjectileInfo> ownership;

  void markOld(asw_player &player);
  void processFrame();
  void debugDump();
  void reset() { ownership.clear(); }
};

struct PlayerState
{
  bool any_prjt = false;

public:
  int time = -1;
  PlayerStateType type = PST_Idle;
  int state_time = 0;
  bool active_stall = true;

  PlayerState() = default;
  PlayerState(asw_player &player, const PlayerState &last);

  bool isStunned() const { return type == PST_HitStunned || type == PST_BlockStunned; }
  bool anyProjectiles() const { return (type == PST_ProjectileAttacking) || any_prjt; }
};

struct FrameInfo
{
  FLinearColor color;
  FLinearColor border;
  int trunc = 0;
};

struct MoveStats
{
  int startup = 0;
  std::vector<std::pair<int, int>> actives{{0, 0}};
  void update(const PlayerState &current);
};

struct PlayerData
{
  FrameInfo segments[FRAME_SEGMENTS];
  MoveStats working_stats;
  MoveStats displayed_stats;
  PlayerState previous_state;
  PlayerState current_state;

  void updateSegment(int prev_idx, int curr_idx);
};

struct FrameBar::Data
{
  DrawContext tool;
  bool combo_active = false;
  int current_segment_idx = 0;
  bool tracking_advantage = false;
  int advantage = 0;
  std::pair<FrameInfo, FrameInfo> segments[FRAME_SEGMENTS];
  std::pair<MoveStats, MoveStats> working_stats;
  std::pair<MoveStats, MoveStats> displayed_stats;
  std::pair<PlayerState, PlayerState> previous_state;
  std::pair<PlayerState, PlayerState> current_state;
  // std::pair<PlayerData, PlayerData> data;

  void drawFrame(const FrameInfo &info, int top, int left);
  void updateActiveSection(const PlayerState &state, int previous_time, FrameInfo &active, FrameInfo &previous);
  void resetFrames();

  Data();
  void addFrame();
  void reset();
  void draw();
};
