#include <polyhook2/Detour/x64Detour.hpp>
#include "sigscan.h"
#include "arcsys.h"
#include "drawing.h"
#include "bind_watcher.h"

#include <UE4SSProgram.hpp>
#include <UnrealDef.hpp>
#include <Unreal/World.hpp>
#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#define STRIVEFRAMEDATA_API __declspec(dllexport)

/* Definitions */

// Classes
class UREDGameCommon : public Unreal::UObject {};
struct FKey {
  Unreal::FName KeyName;
  void* KeyDetailsObject;
  void* KeyDetailsRefController;
  FKey()
  : KeyName(L"")
  , KeyDetailsObject(nullptr)
  , KeyDetailsRefController(nullptr) {}
  FKey(const Unreal::FName& src)
  : KeyName(src)
  , KeyDetailsObject(nullptr)
  , KeyDetailsRefController(nullptr) {}
};
struct ActionKeyMapping {
  FName ActionName;
  uint8_t bShift : 1;
  uint8_t bCtrl : 1;
  uint8_t bAlt : 1;
  uint8_t bCmd : 1;
  FKey Key;
};

// Functions
using funcAHUDPostRender_t = void (*)(void*);
using funcMatchStart_t = void (*)(AREDGameState_Battle*);
using funcGetGameMode_t = int (*)(UREDGameCommon*);
using funcUpdateBattle_t = void (*)(AREDGameState_Battle*, float);

// Hooks
void hook_AHUDPostRender(void*);
void hook_MatchStart(AREDGameState_Battle*);
void hook_UpdateBattle(AREDGameState_Battle*, float);

// Enums
enum GAME_MODE : int32_t {
  GAME_MODE_DEBUG_BATTLE = 0x0,
  GAME_MODE_ADVERTISE = 0x1,
  GAME_MODE_MAINTENANCEVS = 0x2,
  GAME_MODE_ARCADE = 0x3,
  GAME_MODE_MOM = 0x4,
  GAME_MODE_SPARRING = 0x5,
  GAME_MODE_VERSUS = 0x6,
  GAME_MODE_VERSUS_PREINSTALL = 0x7,
  GAME_MODE_TRAINING = 0x8,
  GAME_MODE_TOURNAMENT = 0x9,
  GAME_MODE_RANNYU_VERSUS = 0xA,
  GAME_MODE_EVENT = 0xB,
  GAME_MODE_SURVIVAL = 0xC,
  GAME_MODE_STORY = 0xD,
  GAME_MODE_MAINMENU = 0xE,
  GAME_MODE_TUTORIAL = 0xF,
  GAME_MODE_LOBBYTUTORIAL = 0x10,
  GAME_MODE_CHALLENGE = 0x11,
  GAME_MODE_KENTEI = 0x12,
  GAME_MODE_MISSION = 0x13,
  GAME_MODE_GALLERY = 0x14,
  GAME_MODE_LIBRARY = 0x15,
  GAME_MODE_NETWORK = 0x16,
  GAME_MODE_REPLAY = 0x17,
  GAME_MODE_LOBBYSUB = 0x18,
  GAME_MODE_MAINMENU_QUICK_BATTLE = 0x19,
  GAME_MODE_UNDECIDED = 0x1A,
  GAME_MODE_INVALID = 0x1B,
};

// Strings
// clang-format off
static const wchar_t*  BomEventNames[] = {
	L"BOM_EVENT_ENTRY",
	L"BOM_EVENT_ONLYDRAMA_BATTLE_SETUP",
	L"BOM_EVENT_ARCADE_EVENT_SETUP",
	L"BOM_EVENT_ARCADE_EVENT_FINISH",
	L"BOM_EVENT_ENTRY_EVENT_SETUP",
	L"BOM_EVENT_ENTRY_EVENT_FINISH",
	L"BOM_EVENT_ENTRY_BG",
	L"BOM_EVENT_ENTRY_CHARA",
	L"BOM_EVENT_RESULT_CHARA",
	L"BOM_EVENT_RESET",
	L"BOM_EVENT_SET_KEY_FLAG",
	L"BOM_EVENT_BATTLE_START",
	L"BOM_EVENT_RESULT_SCREEN_SETUP_FOR_STORY",
	L"BOM_EVENT_WIN_ACTION",
	L"BOM_EVENT_MATCH_WIN_ACTION",
	L"BOM_EVENT_MATCH_RESULT_WAIT",
	L"BOM_EVENT_DECISION",
	L"BOM_EVENT_BATTLE_START_CAMERA",
	L"BOM_EVENT_REQUEST_FINISH_STOP",
	L"BOM_EVENT_REQUEST_FINISH_CAMERA",
	L"BOM_EVENT_ENTRY_SCREEN_CONTROL",
	L"BOM_EVENT_WIN_SCREEN_CONTROL",
	L"BOM_EVENT_RESET_REMATCH",
	L"BOM_EVENT_MAIN_MEMBER_CHANGE",
	L"BOM_EVENT_RESET_ON_MEMBER_CHANGE",
	L"BOM_EVENT_MATCH_START_EFFECT",
	L"BOM_EVENT_FINISH_SLOW",
	L"BOM_EVENT_DESTRUCTION_FINISH_START",
	L"BOM_EVENT_DRAMATIC_FINISH_START",
	L"BOM_EVENT_SHENLONG_SYSTEM",
	L"BOM_EVENT_RESULT_VOICE_COMMON",
	L"BOM_EVENT_RESULT_VOICE_SPECIAL",
	L"BOM_EVENT_STOP_RESULT_VOICE",
	L"BOM_EVENT_DRAMATIC_FINISH_UI_AND_STOP",
	L"BOM_EVENT_HUD_TUTORIAL_START",
	L"BOM_EVENT_ENTRY_START",
	L"BOM_EVENT_ADV_DISP_BATTLE",
	L"BOM_EVENT_RANNYU_SAVER",
	L"BOM_EVENT_BBS_EVENT_SETUP",
	L"BOM_EVENT_BBS_EVENT_FINISH",
	L"BOM_EVENT_ROUND_RESET_FOR_BG",
	L"BOM_EVENT_INVALID",
	L"BOM_EVENT_MAX",
};
// clang-format on

/* Variables */

// Debug: Scan utility if asw_player offsets change
#if 0
#define ENABLE_UPDATE_SCANNING
#define SCAN_START 0x6000
constexpr int SCAN_SIZE = 4*16*4; // to 0x6400
uint32_t SCAN_RANGE[SCAN_SIZE];
#endif

// Functions
funcGetGameMode_t orig_GetGameMode;
funcAHUDPostRender_t orig_AHUDPostRender;
funcMatchStart_t orig_MatchStart;
funcUpdateBattle_t orig_UpdateBattle;

// Game Data
GetSizeParams SizeData;

// Unreal Objects
Unreal::AActor* input_actor = nullptr;
Unreal::UObject* hud_actor = nullptr;
Unreal::UObject* player_actor = nullptr;
Unreal::UObject* font_object = nullptr;
Unreal::UObject* worldsets_actor = nullptr;

Unreal::UFunction* drawrect_func = nullptr;
Unreal::UFunction* getsize_func = nullptr;

Unreal::FProperty* paused_prop = nullptr;

// State Data
UREDGameCommon* GameCommon;
UE4SSProgram* Program;
bool in_allowed_mode = false;
bool MatchStartFlag = false;
bool renderingHooked = false;

std::vector<int> allowed_modes = {GAME_MODE_TRAINING, GAME_MODE_REPLAY};
int last_mode = GAME_MODE_DEBUG_BATTLE;
bool was_paused = false;

// Configuration Data
bool cfg_overlayEnabled = true;
bool cfg_truncEnabled = true;
bool cfg_dustloopEnabled = false;
int cfg_resetButton = 7;

/* Functions */

// Utilities
class AsyncInputChecker {
  int pauseButton = VK_F2;
  int advanceButton = VK_F3;
  bool isPaused = false;
  bool shouldAdvance = false;

  void checkBinds(bool await = false) {
    auto inputs = BindWatcherI::getInputs(await);
    for (const auto& input : inputs) {
      if(input == pauseButton){
        isPaused = !isPaused;
      }
      else if(input == advanceButton){
        shouldAdvance = true;
      }
    }
  }
public:
  void pause(){
    checkBinds();
    while (isPaused && !shouldAdvance) {
      checkBinds(true);
    }
  }
  void finishAdvance(){
    shouldAdvance = false;
  }
  void reset(){
    isPaused = false;
    shouldAdvance = false;
  }
} input_checker;

bool checkForReset() {
  auto* events = asw_events::get();
  auto count = events->event_count;
  if (count > 10) count = 10;
  for (unsigned int idx = 0; idx < count; ++idx) {
    if (events->events[idx].type == BOM_EVENT_BATTLE_START) return true;
  }
  return false;
}
const void* vtable_hook(const void** vtable, const int index, const void* hook) {
  DWORD old_protect;
  VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &old_protect);
  const auto* orig = vtable[index];
  vtable[index] = hook;
  VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect);
  return orig;
}
void initRenderHooks() {
  /* Get current HUD actor */
  RC::Output::send<LogLevel::Warning>(STR("Finding HUD actor\n"));
  static auto hud_class_name = Unreal::FName(STR("REDHUD_Battle"), Unreal::FNAME_Add);
  hud_actor = UObjectGlobals::FindFirstOf(hud_class_name);

  static auto ufont_class_name = Unreal::FName(STR("Font"), Unreal::FNAME_Add);
  std::vector<RC::Unreal::UObject*> all_fonts;
  UObjectGlobals::FindAllOf(ufont_class_name, all_fonts);
  for (auto* font : all_fonts) {
    RC::Output::send<LogLevel::Warning>(STR("- {}\n"), font->GetName());
    if (font->GetName() == L"RobotoDistanceField") {
      font_object = font;
    }
  }

  /* Input State bp hooks */
  static auto input_class_name = Unreal::FName(STR("REDPlayerController_Battle"), Unreal::FNAME_Add);
  static auto input_func_name = Unreal::FName(STR("WasInputKeyJustPressed"), Unreal::FNAME_Add);
  static auto getworldsets_func_name = Unreal::FName(STR("K2_GetWorldSettings"), Unreal::FNAME_Add);
  Unreal::UFunction* getworldsets_func = nullptr;
  Unreal::UWorld* world_actor = nullptr;

  input_actor = static_cast<Unreal::AActor*>(UObjectGlobals::FindFirstOf(input_class_name));
  if (input_actor) {
    RC::Output::send<LogLevel::Warning>(STR("Found Input Object\n"));
    world_actor = input_actor->GetWorld();
  }
  if (world_actor) {
    RC::Output::send<LogLevel::Warning>(STR("Found World Object\n"));
    getworldsets_func = world_actor->GetFunctionByNameInChain(getworldsets_func_name);
  }
  if (getworldsets_func) {
    RC::Output::send<LogLevel::Warning>(STR("Found World Settings Function\n"));
    world_actor->ProcessEvent(getworldsets_func, &worldsets_actor);
  }
  if (worldsets_actor) {
    RC::Output::send<LogLevel::Warning>(STR("Found World Settings Object\n"));
    paused_prop = worldsets_actor->GetPropertyByName(STR("PauserPlayerState"));
    if (paused_prop) {
      RC::Output::send<LogLevel::Warning>(STR("Found Paused Property\n"));
    }
  }

  /* HUD Rendering bp hooks */
  static auto hud_drawrect_func_name = Unreal::FName(STR("DrawRect"), Unreal::FNAME_Add);
  static auto hud_drawtext_func_name = Unreal::FName(STR("DrawText"), Unreal::FNAME_Add);
  static auto hud_getplayer_func_name = Unreal::FName(STR("GetOwningPlayerController"), Unreal::FNAME_Add);
  static auto player_getsize_func_name = Unreal::FName(STR("GetViewportSize"), Unreal::FNAME_Add);

  Unreal::UFunction* getplayer_func = nullptr;
  Unreal::UFunction* drawtext_func = nullptr;

  if (hud_actor) {
    RC::Output::send<LogLevel::Warning>(STR("Found HUD Object\n"));

    drawrect_func = hud_actor->GetFunctionByNameInChain(hud_drawrect_func_name);
    drawtext_func = hud_actor->GetFunctionByNameInChain(hud_drawtext_func_name);
    getplayer_func = hud_actor->GetFunctionByNameInChain(hud_getplayer_func_name);
  }
  if (drawrect_func) {
    RC::Output::send<LogLevel::Warning>(STR("Found HUD drawRect Function\n"));
  }
  if (drawtext_func) {
    RC::Output::send<LogLevel::Warning>(STR("Found HUD drawText Function\n"));
  }
  if (getplayer_func) {
    RC::Output::send<LogLevel::Warning>(STR("Found HUD getPlayer Function\n"));
    hud_actor->ProcessEvent(getplayer_func, &player_actor);
  }

  if (player_actor) {
    RC::Output::send<LogLevel::Warning>(STR("Found Player Object\n"));
    getsize_func = player_actor->GetFunctionByNameInChain(player_getsize_func_name);
  }
  if (getsize_func) {
    RC::Output::send<LogLevel::Warning>(STR("Found Player getSize Function\n"));
    player_actor->ProcessEvent(getsize_func, &SizeData);

    RC::Output::send<LogLevel::Warning>(STR("VIEW SIZE - x:{}, y:{}\n"), SizeData.SizeX, SizeData.SizeY);
  }

  initFrames(SizeData, drawrect_func, drawtext_func, font_object);

  if (renderingHooked) return;
  renderingHooked = true;

  /* HUD Rendering vtable hook*/
  const auto** AHUD_vtable = (const void**)get_rip_relative(sigscan::get().scan("\x48\x8D\x05\x00\x00\x00\x00\xC6\x83\x18\x03", "xxx????xxxx") + 3);
  orig_AHUDPostRender = (funcAHUDPostRender_t)vtable_hook(AHUD_vtable, 214, hook_AHUDPostRender);
}
// Hooks
void hook_MatchStart(AREDGameState_Battle* GameState) {
  MatchStartFlag = true;
  input_checker.reset();

  // reset unreal pointers
  input_actor = nullptr;
  hud_actor = nullptr;
  player_actor = nullptr;
  worldsets_actor = nullptr;

  drawrect_func = nullptr;
  getsize_func = nullptr;
  font_object = nullptr;

  orig_MatchStart(GameState);
}
void hook_AHUDPostRender(void* hud) {
  orig_AHUDPostRender(hud);
  if (drawrect_func && hud_actor && cfg_overlayEnabled) {
    if (hud_actor == hud) {
      drawFrames(hud_actor);
    } else {
      RC::Output::send<LogLevel::Warning>(STR("Expiring HUD actor\n"));
      hud_actor = nullptr;
    }
  }
  input_checker.pause();
}
void hook_UpdateBattle(AREDGameState_Battle* GameState, float DeltaTime) {
  if (!GameCommon) {
    GameCommon = reinterpret_cast<UREDGameCommon*>(UObjectGlobals::FindFirstOf(FName(STR("REDGameCommon"))));
    return;
  }
  if (int current_mode = orig_GetGameMode(GameCommon); current_mode != last_mode) {
    RC::Output::send<LogLevel::Warning>(STR("Mode Change: {}\n"), current_mode);
    last_mode = current_mode;

    in_allowed_mode = (std::find(allowed_modes.begin(), allowed_modes.end(), current_mode) != allowed_modes.end());
  }

  if (!in_allowed_mode) {
    orig_UpdateBattle(GameState, DeltaTime);
    return;
  }

  if (checkForReset()) {
    resetFrames();
  }
  orig_UpdateBattle(GameState, DeltaTime);
  input_checker.finishAdvance();

  const auto engine = asw_engine::get();
  if (!engine) return;

  asw_player* player_one = engine->players[0].entity;
  asw_player* player_two = engine->players[1].entity;

  if (MatchStartFlag) {
    MatchStartFlag = false;
    initRenderHooks();
  }

  /* Get pause state */
#if 1
  bool paused = false;
  if (paused_prop) {
    Unreal::AActor** val = static_cast<Unreal::AActor**>(paused_prop->ContainerPtrToValuePtr<void>(worldsets_actor));
    if (val) {
      paused = (*val != nullptr);
      // RC::Output::send<LogLevel::Warning>(STR("Paused: {}\n"), (void*)(*val));
    }
  }
  if (was_paused != paused) {
    RC::Output::send<LogLevel::Warning>(STR("Paused: {}\n"), paused);
    was_paused = paused;
  }
#endif

#ifdef ENABLE_UPDATE_SCANNING  // utility to force scan for new offsets of asw_player after an update
  const uint32_t* scan_root = (uint32_t*)((char*)player_one + SCAN_START);
  for (int i = 0; i < SCAN_SIZE; ++i) {
    if (i == 62 || i == 63) continue;
    auto& last_val = SCAN_RANGE[i];
    auto& next_val = *(scan_root + i);
    if (last_val != next_val) {
      RC::Output::send<LogLevel::Warning>(STR("CHANGE: {}, {} -> {}\n"), i, last_val, next_val);
      last_val = next_val;
    }
  }
#endif

  // scan for player owned entities that are active projectiles
  bool player_one_proj = false;
  bool player_two_proj = false;
  for (int idx = 0; idx < engine->entity_count; ++idx) {
    const auto* focus = engine->entities[idx];
    if (focus == player_one || focus == player_two) continue;

    if (focus->parent_obj == player_one) {
      if (focus->is_active()) {
        player_one_proj = true;
      }
    } else if (focus->parent_obj == player_two) {
      if (focus->is_active()) {
        player_two_proj = true;
      }
    }
  }

  /* Update Frame Data */
  // sometimes the reset doesn't fully take effect for a few frames, this prevents combo data from "bleeding" over
  if (!paused) {
    addFrame(*player_one, *player_two, player_one_proj, player_two_proj);
  }

  /* Update Screen Size */
  if (getsize_func) {
    player_actor->ProcessEvent(getsize_func, &SizeData);
    // RC::Output::send<LogLevel::Warning>(STR("Screen Size: {} {}\n"), SizeData.SizeX, SizeData.SizeY);
    updateSize(SizeData);
  }
}

/* Mod Definition */
class StriveFrameData : public CppUserModBase {
 public:
  PLH::x64Detour* UpdateBattle_Detour;
  PLH::x64Detour* MatchStart_Detour;

  StriveFrameData()
  : CppUserModBase() {
    ModName = STR("Strive Frame Data");
    ModVersion = STR("1.03");
    ModDescription = STR("A tool to display frame advantage.");
    ModAuthors = STR("pbozai");
    UpdateBattle_Detour = nullptr;
    MatchStart_Detour = nullptr;
    // Do not change this unless you want to target a UE4SS version
    // other than the one you're currently building with somehow.
    // ModIntendedSDKVersion = STR("2.6");

    register_tab(STR("Strive Frame Data"), [](CppUserModBase* instance) {
      UE4SS_ENABLE_IMGUI();
      // RC::Output::send<LogLevel::Warning>(STR("IMGUI Pointer: {}\n"), (void*)ImGui::GetCurrentContext());

      ImGui::Text("Options:");

      ImGui::Checkbox("Enable Overlay", &cfg_overlayEnabled);
      // ImGui::Checkbox("Enable Truncation", &cfg_truncEnabled);
      // ImGui::Checkbox("Show Dustloop Style Timings", &cfg_dustloopEnabled);
      // ImGui::Checkbox("Enable Fade Effect", &cfg_fadeEnabled);
      // Input Selector for reset button
      // Input Selector for pause gameplay
      // Input Selector for advance gameplay by a frame
      // Add "End Time" for how long the bar should wait before thinking a combo is dropped

      ImGui::Text("Help:");
      ImGui::Text("TODO");
    });
  }

  ~StriveFrameData() override {}

  auto on_update() -> void override {}

  auto on_unreal_init() -> void override {
    GameCommon = reinterpret_cast<UREDGameCommon*>(UObjectGlobals::FindFirstOf(FName(STR("REDGameCommon"))));

    Program = &UE4SSProgram::get_program();

    const uint64_t UpdateBattle_Addr = sigscan::get().scan("\x40\x53\x57\x41\x54\x41\x55\x48\x81\xEC\x88\x00\x00\x00", "xxxxxxxxxxxxxx");
    UpdateBattle_Detour = new PLH::x64Detour(UpdateBattle_Addr, reinterpret_cast<uint64_t>(&hook_UpdateBattle), reinterpret_cast<uint64_t*>(&orig_UpdateBattle));
    UpdateBattle_Detour->hook();

    const uint64_t MatchStart_Addr = sigscan::get().scan("\x44\x39\x68\x08\x74\x00\x48\x8B\x18\xEB\x00\x48\x8B\xDF", "xxxxx?xxxx?xxx") - 0x8D;
    MatchStart_Detour = new PLH::x64Detour(MatchStart_Addr, reinterpret_cast<uint64_t>(&hook_MatchStart), reinterpret_cast<uint64_t*>(&orig_MatchStart));
    MatchStart_Detour->hook();

    const uintptr_t GetGameMode_Addr = sigscan::get().scan("\x0F\xB6\x81\xF0\x02\x00\x00\xC3", "xxxxxxxx");
    orig_GetGameMode = reinterpret_cast<funcGetGameMode_t>(GetGameMode_Addr);

    ASWInitFunctions();
    bbscript::BBSInitializeFunctions();

    BindWatcherI::addToFilter(VK_F2);
    BindWatcherI::addToFilter(VK_F3);
  }
};

extern "C" {
STRIVEFRAMEDATA_API CppUserModBase* start_mod() {
  return new StriveFrameData();
}

STRIVEFRAMEDATA_API void uninstall_mod(CppUserModBase* mod) {
  delete mod;
}
}