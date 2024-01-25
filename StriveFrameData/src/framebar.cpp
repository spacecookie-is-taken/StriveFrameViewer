#include "framebar_p.h"

/* Debug Stuff */
#if 0
#define DEBUG_PRINT(...) RC::Output::send<LogLevel::Warning>(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif
#define ENABLE_PRJT_DEBUG false
#define ENABLE_STATE_DEBUG false

namespace
{
  /* Combo Triger Settings */
  constexpr int COMBO_ENDED_TIME = 20; // time idle before combo is considered over
  constexpr int COMBO_NUM_TIME = 5;    // minimum time of unchanging states to display state length
  constexpr int COMBO_TRUNC_TIME = 10; // max segments for unchanging state, after which the segments are truncated
  constexpr double COLOR_DECAY = 0.9f;

  /* Draw-Space Settings */
  constexpr int FADE_DISTANCE = 5;
  constexpr int SEG_WIDTH = 20;
  constexpr int SEG_HEIGHT = 36;
  constexpr int SEG_SPACING = 4;
  constexpr int BAR_SPACING = 8;
  constexpr int BORDER_THICKNESS = 3;
  constexpr double BAR_TEXT_SIZE = 1.4;

  /* Scree-Space Settings */
  constexpr double CENTER_X_RATIO = 0.5f;
  constexpr double CENTER_Y_RATIO = 0.8f;

  /* Draw-Space Calculations */
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

  /* Unreal Constants */
  FLinearColor color_invisible{1.f, 1.f, 1.f, 0.f};
  FLinearColor color_white{1.f, 1.f, 1.f, 1.f};
  FLinearColor color_black{0.f, 0.f, 0.f, 1.f};
  FLinearColor projectile_color{.8f, .1f, .1f, 1.f};
  FLinearColor background_color{0.05f, 0.05f, 0.05f, 0.7f};
  static const FLinearColor state_colors[] = {
      FLinearColor{.2f, .2f, .2f, .9f}, // Gray
      FLinearColor{.1f, .1f, .8f, .9f}, // Blue
      FLinearColor{.1f, .6f, .1f, .9f}, // Green
      FLinearColor{.7f, .7f, .1f, .9f}, // Yellow
      FLinearColor{.8f, .1f, .1f, .9f}, // Red
      FLinearColor{.8f, .4f, .1f, .9f}, // Orange
      FLinearColor{.8f, .4f, .1f, .9f}  // Orange
  };
}

// ############################################################
// Helpers

std::wstring convertToWide(const std::string_view &str)
{
  if (str.empty())
    return std::wstring();
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
  return wstrTo;
}

std::wstring makeStatsText(const MoveStats &stats, int advantage)
{
  std::wstringstream builder;
  builder << L"Startup: " << stats.startup << ", Active: ";
  for (int idx = 0; idx < stats.actives.size() - 1; ++idx)
  {
    auto &focus = stats.actives[idx];
    builder << focus.first << L"(" << focus.second << ")";
  }
  auto &last = stats.actives.back();
  builder << last.first << ", Recovery: " << last.second << L", Advantage: " << advantage;
  return builder.str();
}

template <class... ArgT>
void doBoth(std::pair<PlayerData, PlayerData> &data, void (PlayerData::*func)(ArgT...), ArgT... args)
{
  data.first.*func(args...);
  data.second.*func(args...);
}

template <class FuncT, class... ArgT>
void doBoth(std::pair<PlayerData, PlayerData> &data, const std::function<FuncT> &func, ArgT... args)
{
  func(&data.first, args...);
  func(&data.second, args...);
}

// ############################################################
// ProjectileTracker

void ProjectileTracker::markOld(asw_player &player)
{
  // assumes a move will never spawn a projectile frame 1
  for (auto &iter : ownership)
  {
    if (iter.second.root_parent == &player && iter.second.old != 0)
    {
      iter.second.old = 2;
    }
  }
}

void ProjectileTracker::processFrame()
{
  // assumes if an entity at &p was destroyed and a new entity is created at &p, any new entities that point to &p as their parent refer to the new one
  const auto engine = asw_engine::get();
  if (!engine)
    return;

  asw_player *player_one = engine->players[0].entity;
  asw_player *player_two = engine->players[1].entity;
  if (!player_one || !player_two)
    return;

  // mark all old pointers as dead until seen in the current frame
  for (auto &iter : ownership)
  {
    iter.second.alive = false;
    if (iter.second.old == 0)
      iter.second.old = 1;
  }

  // check for in-frame reassignment
  for (int idx = 0; idx < engine->entity_count; ++idx)
  {
    auto *focus = engine->entities[idx], *parent = focus->parent_obj;
    if (auto iter = ownership.find(focus); iter != ownership.end())
    {
      if (iter->second.direct_parent != parent)
      {
        RC::Output::send<LogLevel::Warning>(STR("PSFP re-assign: {}, Old Parent: {}, New Parent: {}\n"), (void *)focus, (void *)iter->second.direct_parent, (void *)parent);
        ownership.erase(focus);
      }
      else
      {
        iter->second.alive = true;
      }
    }
  }

  // create new infos and link parents
  for (int idx = 0; idx < engine->entity_count; ++idx)
  {
    auto *focus = engine->entities[idx], *parent = focus->parent_obj;
    if (focus == player_one || focus == player_two)
      continue;
    bool old = false;
    if (ownership.find(focus) == ownership.end())
    {
      if (auto parent_iter = ownership.find(parent); parent_iter != ownership.end())
      {
        old = parent_iter->second.old;
        ownership.emplace(focus, ProjectileInfo(focus, *parent_iter));
      }
      else
      {
        ownership.emplace(focus, ProjectileInfo(focus));
      }
    }
    for (auto &iter : ownership)
    {
      if (iter.second.root_parent == focus)
      {
        iter.second.root_parent = parent;
        iter.second.old = old;
      }
    }
  }

  // remove any dead elements
  for (auto first = ownership.begin(), last = ownership.end(); first != last;)
  {
    if (!first->second.alive)
      first = ownership.erase(first);
    else
      ++first;
  }
}
void ProjectileTracker::debugDump()
{
  constexpr const wchar_t *age_vals[] = {L"NEW", L"EXIST", L"OLD"};

  const auto engine = asw_engine::get();
  if (!engine)
    return;

  asw_player *player_one = engine->players[0].entity;
  asw_player *player_two = engine->players[1].entity;

  auto format = STR("Prjt script:{}, sprite:{}, ptr:{}, par:{}, pt:{}, old:{}, dmg:{}, hit:{}, act:{}\n");
  for (auto &iter : ownership)
  {
    auto *focus = iter.first, *top_parent = iter.second.root_parent;
    auto bb_script = convertToWide(focus->get_BB_state());
    auto sprite = convertToWide(focus->get_sprite_name());
    void *direct_parent = (void *)iter.second.direct_parent;
    const wchar_t *parent_type = (top_parent == player_one) ? L"ONE" : ((top_parent == player_two) ? L"TWO" : L"FREE");
    const wchar_t *age = age_vals[iter.second.old];
    int dmg = focus->atk_param_hit.damage;
    int hitbox_count = focus->hitbox_count;
    const wchar_t *active = focus->is_active() ? L"Y" : L"N";

    RC::Output::send<LogLevel::Warning>(format, bb_script, sprite, (void *)focus, direct_parent, parent_type, age, dmg, hitbox_count, active);
  }
}

ProjectileTracker ptracker;

/* Known Issues:
  Several Asuka spells will mark inactive and set sprite to null on frame-0 hit:
    "Howling Metron" / "AttackMagic_01"
    "Howling Metron MS Processing" / "AttackMagic_03"
    "Bit Shift Metron" / "AttackMagic_10"
    "RMS Boost Metron" / "AttackMagic_11"
*/
bool shouldBeActive(std::pair<asw_entity *const, ProjectileTracker::ProjectileInfo> &info_pair)
{
  const auto bbscript = std::string_view(info_pair.first->get_BB_state());

  // see above, these 4 break the assumptions that cover every other projectile, and I'm fucking tired
  if (bbscript.size() == 14 && bbscript.substr(0, 12) == "AttackMagic_")
  {
    auto AM_num = bbscript.substr(12, 2);
    if (AM_num == "01" || AM_num == "03" || AM_num == "10" || AM_num == "11")
      return true;
  }
  /* null check for two reasons:
      strive leaves active but nulled sprites around FOREVER (metron 808 lasts 50 frames)
      we don't want to apply the frame-0 hack to null sprites that haven't come out yet
  */
  const auto sprite = std::string_view(info_pair.first->get_sprite_name());
  if (sprite == "null")
    return false;
  if (info_pair.first->is_active() && info_pair.first->hitbox_count > 0)
    return true;

  /* another hack... some projectiles spawn deactived and activate later on
    Baikan's Tatami Mat
    either of Ram's Bajoneto
  */
  // tatami mat is spawned WAY before its active, don't apply frame-0 hack to it
  if (bbscript == "TatamiLandObj" || bbscript == "404wepL_summon_short" || bbscript == "404wepR_summon_short")
    return false;

  // to compensate for projectiles that "should" be active but are self deactivating frame-0
  // we artificially extend their lifetime just long enough for our purposes.
  int damage = info_pair.first->atk_param_hit.damage;
  if (damage > 0 && info_pair.second.hit_delay)
  {
    --info_pair.second.hit_delay;
    return true;
  }
  return false;
}

// ############################################################
// MoveStats

void MoveStats::update(const PlayerState &current)
{
  if (current.type == PST_Busy)
  {
    startup = current.state_time + 1;
  }
  else if (current.type == PST_Attacking || current.type == PST_ProjectileAttacking)
  {
    // this is a new attack
    if (actives.back().second > 0)
      actives.push_back({0, 0});
    actives.back().first = current.state_time;
  }
  else if (current.type == PST_Recovering)
  {
    actives.back().second = current.state_time;
  }
}

void PlayerData::updateSegment(int prev_idx, int curr_idx)
{
  // previous section has ended
  auto &previous = segments[prev_idx];
  auto &active = segments[curr_idx];
  previous.trunc = 0;

  if (current_state.state_time == 1)
  {
    DEBUG_PRINT(STR("New Section for One\n"));
    active.color = state_colors[current_state.type];
    // ... and was long enough that we want to print length
    if (previous_state.state_time > COMBO_NUM_TIME)
    {
      DEBUG_PRINT(STR("Last section requires truncating\n"));
      previous.trunc = previous_state.state_time;
    }
  }
  else
  {
    DEBUG_PRINT(STR("Same Section for One\n"));
    // we are drawing this section, fade its color slightly
    // active_segment.color_one = state_colors[type_one] * COLOR_DECAY;
    active.color = previous.color * COLOR_DECAY;
  }
  active.border = current_state.anyProjectiles() ? projectile_color : color_invisible;
}
// FrameBar::Data::updateActiveSection(const PlayerState& state, int previous_time, FrameInfo& active, FrameInfo& previous)
// updateActiveSection(current_state.first, previous_state.first.state_time, active_segment.first, prev_segment.first);

// ############################################################
// PlayerState

PlayerState::PlayerState(asw_player &player, const PlayerState &last)
{
  time = player.action_time;

  // assumes a sprite won't come out on this frame, this is false
  const bool same_script = time > 1;
  if (!same_script)
  {
    ptracker.markOld(player);
  }

  const bool normal_canact = player.can_act();
  const bool stance_canact = player.is_stance_idle();
  const bool block_stunned = player.is_in_blockstun() || player.is_stagger() || player.is_guard_crush();
  const bool hit_stunned = player.is_in_hitstun();
  const bool knockdown = player.is_knockdown();
  const bool player_active = player.is_active() && (player.hitbox_count > 0 || player.throw_range >= 0);

  bool projectile_active = false;
  for (auto &iter : ptracker.ownership)
  {
    if (iter.second.root_parent != &player || !shouldBeActive(iter))
      continue;
    any_prjt |= !player_active || (iter.second.old == 2);
    projectile_active |= !(iter.second.old == 2);
  }

  // TODO: account for 0-lifetime projectiles that spawn and immediately self destruct, such as Asuka's 808

  if (normal_canact || stance_canact)
    type = PST_Idle;
  else if (block_stunned)
    type = PST_BlockStunned;
  else if (hit_stunned || knockdown)
    type = PST_HitStunned;
  else if (player_active && last.active_stall)
    type = PST_Attacking;
  else if (projectile_active && last.active_stall)
    type = PST_ProjectileAttacking;
  else if (same_script && last.type != PST_Busy)
    type = PST_Recovering;
  else
    type = PST_Busy;

  // active stall prevents the first active frame (before the hit is registered) from appearing active
  // this helps match Dustloop and looks more intuitive
  if (player_active || projectile_active)
  {
    active_stall = true;
  }

  // state_time is used for determining how long was spent in each PST state for a single BB state script
  // type != PST_Busy to prevent interuptible post move animations (that are idle equivalent) or chained stuns from breaking segments
  if ((same_script || type != PST_Busy) && last.type == type)
  {
    state_time = (last.state_time < 1000) ? last.state_time + 1 : last.state_time;
  }
  else
  {
    state_time = 1;
  }

  if constexpr (ENABLE_STATE_DEBUG)
  {
    auto format = STR("script:{}, time:{}, sprite:{}, can:{}, stance:{} bstun:{}, hstun:{}, plact:{}, pjact:{}, any:{}, st:{}, cin:{}, hbc:{}, trw:{}\n");
    std::wstring local_script = convertToWide(player.get_BB_state());
    std::wstring local_sprite = convertToWide(player.get_sprite_name());
    auto nca = normal_canact ? L"Y" : L"N";
    auto sca = stance_canact ? L"Y" : L"N";
    auto bs = block_stunned ? L"Y" : L"N";
    auto hs = hit_stunned ? L"Y" : L"N";
    auto pla = player_active ? L"Y" : L"N";
    auto pja = projectile_active ? L"Y" : L"N";
    auto aja = any_prjt ? L"Y" : L"N";
    auto cin = player.cinematic_counter ? L"Y" : L"N";
    RC::Output::send<LogLevel::Warning>(format, local_script, time, local_sprite, nca, sca, bs, hs, pla, pja, aja, state_time, cin, player.hitbox_count, player.throw_range);
  }
}

// ############################################################
// FrameBar::Data

FrameBar::Data::Data()
    : tool(CENTER_X_RATIO, CENTER_Y_RATIO)
{
  resetFrames();
}

void FrameBar::Data::drawFrame(const FrameInfo &info, int top, int left)
{
  if (info.color.A != 0.f)
  {
    if (info.border.A != 0.f)
    {
      tool.drawRect(left - BORDER_THICKNESS, top - BORDER_THICKNESS, SEG_WIDTH + 2 * BORDER_THICKNESS, SEG_HEIGHT + 2 * BORDER_THICKNESS, info.border);
    }
    tool.drawRect(left, top, SEG_WIDTH, SEG_HEIGHT, info.color);
    if (info.trunc > 0)
    {
      auto text = std::to_wstring(info.trunc);
      int text_left = left - (text.size() - 1) * 16 + 2;
      tool.drawOutlinedText(text_left, top, Unreal::FString(text.c_str()), BAR_TEXT_SIZE);
    }
  }
}

void FrameBar::Data::resetFrames()
{
  current_segment_idx = 0;
  current_state.first.state_time = 1;
  current_state.second.state_time = 1;
  previous_state.first.state_time = 1;
  previous_state.second.state_time = 1;
  working_stats = std::make_pair(MoveStats(), MoveStats());
  for (int idx = 0; idx < FRAME_SEGMENTS; ++idx)
  {
    segments[idx] = std::make_pair(FrameInfo(), FrameInfo());
    // data.first.segments[idx] = FrameInfo();
    // data.second.segments[idx] = FrameInfo();
  }
}

void FrameBar::Data::updateActiveSection(const PlayerState &state, int previous_time, FrameInfo &active, FrameInfo &previous)
{
  // previous section has ended
  previous.trunc = 0;

  if (state.state_time == 1)
  {
    DEBUG_PRINT(STR("New Section for One\n"));
    active.color = state_colors[state.type];
    // ... and was long enough that we want to print length
    if (previous_time > COMBO_NUM_TIME)
    {
      DEBUG_PRINT(STR("Last section requires truncating\n"));
      previous.trunc = previous_time;
    }
  }
  else
  {
    DEBUG_PRINT(STR("Same Section for One\n"));
    // we are drawing this section, fade its color slightly
    // active_segment.color_one = state_colors[type_one] * COLOR_DECAY;
    active.color = previous.color * COLOR_DECAY;
  }
  active.border = state.anyProjectiles() ? projectile_color : color_invisible;
}

void FrameBar::Data::addFrame()
{
  const auto engine = asw_engine::get();
  if (!engine)
    return;

  auto &p_one = *engine->players[0].entity;
  auto &p_two = *engine->players[1].entity;

  // update projectiles, even during hitstun
  ptracker.processFrame();
  if constexpr (ENABLE_PRJT_DEBUG)
  {
    RC::Output::send<LogLevel::Warning>(STR("PTracker Pre:\n"));
    ptracker.debugDump();
  }

  // crate updated states
  std::pair<PlayerState, PlayerState> next = {
      PlayerState(p_one, current_state.first),
      PlayerState(p_two, current_state.second)};

  if (p_one.cinematic_counter || p_two.cinematic_counter)
  {
    if constexpr (ENABLE_STATE_DEBUG)
    {
      RC::Output::send<LogLevel::Warning>(STR("CINEMATIC SKIP {} {} {} {}\n"), p_one.hitstop, p_one.atk_param_hit.hitstop, p_two.hitstop, p_two.hitstop);
    }
    return;
  }
  // covers COUNTER HIT, this might cover all cases
  else if ((next.first.time == current_state.first.time && next.first.time > 1 && (next.second.type == PST_HitStunned || next.second.type == PST_BlockStunned)) || (next.second.time == current_state.second.time && next.second.time > 1 && (next.first.type == PST_HitStunned || next.first.type == PST_BlockStunned)))
  {
    if constexpr (ENABLE_STATE_DEBUG)
    {
      RC::Output::send<LogLevel::Warning>(STR("STUN SKIP {} {} {} {}\n"), p_one.hitstop, p_one.atk_param_hit.hitstop, p_two.hitstop, p_two.hitstop);
    }
    return;
  }
  else if constexpr (ENABLE_STATE_DEBUG)
  {
    RC::Output::send<LogLevel::Warning>(STR("KEEP {} {} {} {}\n"), p_one.hitstop, p_one.atk_param_hit.hitstop, p_two.hitstop, p_two.hitstop);
  }

  // shift states
  previous_state.first = current_state.first;
  previous_state.second = current_state.second;
  current_state = next;

  if constexpr (ENABLE_PRJT_DEBUG)
  {
    RC::Output::send<LogLevel::Warning>(STR("PTracker Post:\n"));
    ptracker.debugDump();
  }

  // end combo if we've been idle for a long time
  if (current_state.first.type == PST_Idle && current_state.second.type == PST_Idle)
  {
    if (!combo_active)
    {
      return;
    }
    if (current_state.first.state_time > COMBO_ENDED_TIME && current_state.second.state_time > COMBO_ENDED_TIME)
    {
      DEBUG_PRINT(STR("Combo ended\n"));
      combo_active = false;
      return;
    }
  }
  // reset if this is a new combo
  else if (!combo_active)
  {
    DEBUG_PRINT(STR("Combo reset\n"));
    resetFrames();
  }

  DEBUG_PRINT(STR("Frame {}\n"), current_segment_idx);

  // update move info
  if (current_state.first.time > 1)
  {
    working_stats.first.update(current_state.first);
    if (working_stats.first.actives.front().first > 0)
    {
      displayed_stats.first = working_stats.first;
    }
  }
  else
  {
    working_stats.first = MoveStats();
  }

  if (current_state.second.time > 1)
  {
    working_stats.second.update(current_state.second);
    if (working_stats.second.actives.front().first > 0)
    {
      displayed_stats.second = working_stats.second;
    }
  }
  else
  {
    working_stats.second = MoveStats();
  }

  // update advantage
  if (!tracking_advantage)
  {
    if (current_state.first.isStunned() || current_state.second.isStunned())
    {
      advantage = 0;
      tracking_advantage = true;
    }
  }
  else
  {
    if (current_state.first.type == PST_Idle)
    {
      if (current_state.second.type == PST_Idle)
      {
        tracking_advantage = false;
      }
      else
      {
        ++advantage;
      }
    }
    else
    {
      if (current_state.second.type == PST_Idle)
      {
        --advantage;
      }
      else
      {
        advantage = 0;
      }
    }
  }

  // update frame display

  auto &active_segment = segments[current_segment_idx];

  // only trigger truncation logic if mid-combo
  if (combo_active)
  {
    DEBUG_PRINT(STR("Is Active\n"));
    int prev_idx = (current_segment_idx + FRAME_SEGMENTS - 1) % FRAME_SEGMENTS;
    auto &prev_segment = segments[prev_idx];

    // we are truncating, update truncated
    if (current_state.first.state_time > COMBO_TRUNC_TIME && current_state.second.state_time > COMBO_TRUNC_TIME)
    {
      prev_segment.first.trunc = current_state.first.state_time;
      prev_segment.second.trunc = current_state.second.state_time;
      DEBUG_PRINT(STR("Truncating 1:{}, 2:{}\n"), prev_segment.trunc_one, prev_segment.trunc_two);
      return;
    }

    // doBoth(data, &PlayerData::updateSegment, prev_idx, current_segment_idx);

    updateActiveSection(current_state.first, previous_state.first.state_time, active_segment.first, prev_segment.first);
    updateActiveSection(current_state.second, previous_state.second.state_time, active_segment.second, prev_segment.second);
  }
  else
  {
    DEBUG_PRINT(STR("Was not Active\n"));
    active_segment.first.color = state_colors[current_state.first.type];
    active_segment.first.border = current_state.first.anyProjectiles() ? projectile_color : color_invisible;
    active_segment.second.color = state_colors[current_state.second.type];
    active_segment.second.border = current_state.second.anyProjectiles() ? projectile_color : color_invisible;
  }

  // fade effect, clear segments near tail
  auto &fade_segment = segments[(current_segment_idx + FADE_DISTANCE) % FRAME_SEGMENTS];
  fade_segment.first.color.A = 0.f;
  fade_segment.second.color.A = 0.f;

  // advance to next segment
  if (++current_segment_idx >= FRAME_SEGMENTS)
  {
    DEBUG_PRINT(STR("Cycling segment index\n"));
    current_segment_idx = 0;
  }

  combo_active = true;
}
void FrameBar::Data::reset()
{
  combo_active = false;
  current_state.first = PlayerState();
  current_state.second = PlayerState();
  ptracker.reset();
}
void FrameBar::Data::draw()
{
  tool.update();

  auto player_one_info = makeStatsText(displayed_stats.first, advantage);
  auto player_two_info = makeStatsText(displayed_stats.second, -advantage);

  tool.drawOutlinedText(BAR_LEFT, INFO_ONE_LOC, Unreal::FString(player_one_info.c_str()), BAR_TEXT_SIZE);
  tool.drawOutlinedText(BAR_LEFT, INFO_TWO_LOC, Unreal::FString(player_two_info.c_str()), BAR_TEXT_SIZE);

  tool.drawRect(BAR_LEFT, BAR_ONE_TOP, BAR_WIDTH, BAR_HEIGHT, background_color);
  tool.drawRect(BAR_LEFT, BAR_TWO_TOP, BAR_WIDTH, BAR_HEIGHT, background_color);

  for (int idx = 0; idx < FRAME_SEGMENTS; ++idx)
  {
    int left = BAR_LEFT + (SEG_TOTAL * idx) + SEG_SPACING;
    const auto &segment = segments[idx];

    drawFrame(segment.first, SEGS_ONE_TOP, left);
    drawFrame(segment.second, SEGS_TWO_TOP, left);
  }
}

FrameBar::~FrameBar() = default;
FrameBar::FrameBar() : data(new Data()) {}
void FrameBar::addFrame() { d().addFrame(); }
void FrameBar::reset() { d().reset(); }
void FrameBar::draw() { d().draw(); }