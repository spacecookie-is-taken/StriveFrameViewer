#pragma once

#include <UnrealDef.hpp>

#include "arcsys.h"

struct GetSizeParams {
  int32_t SizeX = 0;
  int32_t SizeY = 0;
};

void initFrames(const GetSizeParams& sizedata, RC::Unreal::UFunction* drawrect, RC::Unreal::UFunction* drawtext, RC::Unreal::UObject* fontobject);
void addFrame();
void resetFrames();

void drawFrames(RC::Unreal::UObject* hud, const GetSizeParams& sizedata);


class MenuOverlay {

};