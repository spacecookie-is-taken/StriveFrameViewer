#pragma once

#include "arcsys.h"

#include <vector>

void SendData(const char* data);

void SendFrameData(int player_id, asw_player* player);

void SendInputData(const std::vector<bool>& data);