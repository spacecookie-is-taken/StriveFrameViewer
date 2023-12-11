#pragma once

#include <UnrealDef.hpp>

#include "arcsys.h"

struct GetSizeParams {
	int32_t SizeX = 0;
	int32_t SizeY = 0;
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

struct PlayerState {
	bool canact;
  bool block_stunned;
  bool hit_stunned;
	bool active;
	int time;
	bool recovery;
	bool hitstop;
	int state_time;
	int inactive_time;

	PlayerState();
	PlayerState(asw_player& player, const PlayerState& last, bool proj_active);
	PlayerStateType getType() const;
};

void initFrames(const GetSizeParams& sizedata, RC::Unreal::UFunction* drawrect, RC::Unreal::UFunction* drawtext);
void addFrame(asw_player& player_one, asw_player& player_two, bool player_one_proj, bool player_two_proj);
void resetFrames();

void drawFrames(RC::Unreal::UObject* hud);


