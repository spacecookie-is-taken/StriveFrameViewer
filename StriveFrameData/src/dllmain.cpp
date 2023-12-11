#include <polyhook2/Detour/x64Detour.hpp>
#include "sigscan.h"
#include "arcsys.h"
#include "netcalls.h"
#include "drawing.h"

#include <UE4SSProgram.hpp>
#include <UnrealDef.hpp>
#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#define STRIVEFRAMEDATA_API __declspec(dllexport)

class UREDGameCommon : public Unreal::UObject {};
UREDGameCommon* GameCommon;

typedef int(*GetGameMode_Func)(UREDGameCommon*);
GetGameMode_Func GetGameMode;

enum GAME_MODE : int32_t
{
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

int last_hurt = 0;
int last_hit = 0;
int last_ec = 0;
int last_flag = 0;

#define SCAN_START 0x6000
constexpr int SCAN_SIZE = 4*16*4; // to 0x6400
uint32_t SCAN_RANGE[SCAN_SIZE];

bool ShouldUpdateBattle = true;
bool ShouldAdvanceBattle = false;

struct UpdateAdvantage
{
    UpdateAdvantage()
    {
        Text = Unreal::FString(STR(""));
    }

    Unreal::FString Text;
};


/*
	Imitates FKey and bool return value
		FName
		TSharedPtr<FKeyDetails,...>
			FKeyDetails*
			FSharedReferencer<...>
				FReferenceControllerBase*
	Flattened:
		FName
		FKeyDetails*
		FReferenceControllerBase*
		bool
	Size matches 0x18 from ghidra, allignment matches as well
*/
struct InputParamData {
	Unreal::FName KeyName;
	void* KeyDetailsObject;
	void* KeyDetailsRefController;
	bool was_pressed;

	InputParamData(const Unreal::FName& src)
		: KeyName(src)
		, KeyDetailsObject(nullptr)
		, KeyDetailsRefController(nullptr)
		, was_pressed(false)
	{}
};

static const wchar_t* RawButtonNames[] = {
	L"Gamepad_LeftX",
	L"Gamepad_LeftY",
	L"Gamepad_RightX",
	L"Gamepad_RightY",
	L"Gamepad_LeftTriggerAxis",
	L"Gamepad_RightTriggerAxis",
	L"Gamepad_LeftThumbstick",
	L"Gamepad_RightThumbstick",
	L"Gamepad_Special_Left",
	L"Gamepad_Special_Right",
	L"Gamepad_FaceButton_Bottom",
	L"Gamepad_FaceButton_Right",
	L"Gamepad_FaceButton_Left",
	L"Gamepad_FaceButton_Top",
	L"Gamepad_LeftShoulder",
	L"Gamepad_RightShoulder",
	L"Gamepad_LeftTrigger",
	L"Gamepad_RightTrigger",
	L"Gamepad_DPad_Up",
	L"Gamepad_DPad_Down",
	L"Gamepad_DPad_Right",
	L"Gamepad_DPad_Left",
	L"Gamepad_LeftStick_Up,"
	L"Gamepad_LeftStick_Down",
	L"Gamepad_LeftStick_Right",
	L"Gamepad_LeftStick_Left",
	L"Gamepad_RightStick_Up",
	L"Gamepad_RightStick_Down",
	L"Gamepad_RightStick_Right",
	L"Gamepad_RightStick_Left"
};
constexpr int ButtonCount = sizeof(RawButtonNames) / sizeof(wchar_t*);
InputParamData* ButtonData[ButtonCount];
std::vector<bool> ButtonStates;

GetSizeParams SizeData;

Unreal::UObject* input_actor = nullptr;
Unreal::UObject* hud_actor = nullptr;
Unreal::UObject* player_actor = nullptr;

Unreal::UFunction* press_function = nullptr;
Unreal::UFunction* drawrect_func = nullptr;
Unreal::UFunction* drawtext_func = nullptr;
Unreal::UFunction* getplayer_func = nullptr;
Unreal::UFunction* getsize_func = nullptr;

bool MatchStartFlag = false;

void(*MatchStart_Orig)(AREDGameState_Battle*);
void MatchStart_New(AREDGameState_Battle* GameState)
{
    MatchStartFlag = true;
    ShouldAdvanceBattle = false;
    ShouldUpdateBattle = true;

    // reset unreal pointers
    input_actor = nullptr;
    hud_actor = nullptr;
    player_actor = nullptr;

    press_function = nullptr;
    drawrect_func = nullptr;
    drawtext_func = nullptr;
    getplayer_func = nullptr;
    getsize_func = nullptr;

    MatchStart_Orig(GameState);
}

const void* vtable_hook(const void** vtable, const int index, const void* hook)
{
	DWORD old_protect;
	VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &old_protect);
	const auto* orig = vtable[index];
	vtable[index] = hook;
	VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect);
	return orig;
}

using AHUD_PostRender_t = void(*)(void*);
AHUD_PostRender_t orig_AHUD_PostRender;
void hook_AHUD_PostRender(void* hud) {
	orig_AHUD_PostRender(hud);
    if (drawrect_func && hud_actor) {
        if(hud_actor == hud){
            drawFrames(hud_actor);
        }
        else {
            RC::Output::send<LogLevel::Warning>(STR("Expiring HUD actor\n"));
            hud_actor = nullptr;
        }
    }
}

bool renderingHooked = false;
void initRenderHooks() {

    /* Get current HUD actor */
    RC::Output::send<LogLevel::Warning>(STR("Finding HUD actor\n"));
    static auto hud_class_name = Unreal::FName(STR("REDHUD_Battle"), Unreal::FNAME_Add);
    hud_actor = UObjectGlobals::FindFirstOf(hud_class_name);

    /* Input State bp hooks */
    static auto input_class_name = Unreal::FName(STR("REDPlayerController_Battle"), Unreal::FNAME_Add);
    static auto input_func_name = Unreal::FName(STR("WasInputKeyJustPressed"), Unreal::FNAME_Add);

    input_actor = UObjectGlobals::FindFirstOf(input_class_name);
    if (input_actor) {
        RC::Output::send<LogLevel::Warning>(STR("Found Input Object\n"));
        press_function = input_actor->GetFunctionByNameInChain(input_func_name);
    }
    if (press_function) {
        RC::Output::send<LogLevel::Warning>(STR("Found Input Function\n"));
        ButtonStates.resize(ButtonCount, false);
        for (int idx = 0; idx < ButtonCount; ++idx) {
            ButtonData[idx] = new InputParamData(Unreal::FName(RawButtonNames[idx], Unreal::FNAME_Add));
        }
    }

    /* HUD Rendering bp hooks */
    static auto hud_drawrect_func_name = Unreal::FName(STR("DrawRect"), Unreal::FNAME_Add);
    static auto hud_drawtext_func_name = Unreal::FName(STR("DrawText"), Unreal::FNAME_Add);
    static auto hud_getplayer_func_name = Unreal::FName(STR("GetOwningPlayerController"), Unreal::FNAME_Add);
    static auto player_getsize_func_name = Unreal::FName(STR("GetViewportSize"), Unreal::FNAME_Add);

    if (hud_actor) {
        RC::Output::send<LogLevel::Warning>(STR("Found HUD Object\n")); 
        drawrect_func = hud_actor->GetFunctionByNameInChain(hud_drawrect_func_name);
        drawtext_func = hud_actor->GetFunctionByNameInChain(hud_drawtext_func_name);
        getplayer_func = hud_actor->GetFunctionByNameInChain(hud_getplayer_func_name);
    }
    if (drawrect_func) { RC::Output::send<LogLevel::Warning>(STR("Found HUD drawRect Function\n")); }
    if (drawtext_func) { RC::Output::send<LogLevel::Warning>(STR("Found HUD drawText Function\n")); }
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

    initFrames(SizeData, drawrect_func, drawtext_func);

    if(renderingHooked) return;
    renderingHooked = true;

    /* HUD Rendering vtable hook*/
    const auto** AHUD_vtable = (const void**)get_rip_relative(sigscan::get().scan("\x48\x8D\x05\x00\x00\x00\x00\xC6\x83\x18\x03", "xxx????xxxx") + 3);
	orig_AHUD_PostRender = (AHUD_PostRender_t)vtable_hook(AHUD_vtable, 214, hook_AHUD_PostRender);
}

UE4SSProgram* Program;

bool f2_pressed = false;
bool f3_pressed = false;

void(*UpdateBattle_Orig)(AREDGameState_Battle*, float);
void UpdateBattle_New(AREDGameState_Battle* GameState, float DeltaTime) {
	if (!GameCommon)
	{
		GameCommon = reinterpret_cast<UREDGameCommon*>(UObjectGlobals::FindFirstOf(FName(STR("REDGameCommon"))));
		return;
	}
    if (GetGameMode(GameCommon) != static_cast<int>(GAME_MODE_TRAINING))
    {
        UpdateBattle_Orig(GameState, DeltaTime);
        return;
    }
	
	if (GetAsyncKeyState(VK_F2) & 0x8000)
	{
		if (!f2_pressed)
		{
			ShouldUpdateBattle = !ShouldUpdateBattle;
			f2_pressed = true;
		}
	}
	else
	{
		f2_pressed = false;
	}
	if (GetAsyncKeyState(VK_F3) & 0x8000)
	{
		if (!f3_pressed)
		{
			ShouldAdvanceBattle = true;
			f3_pressed = true;
		}
	}
	else
	{
		f3_pressed = false;
	}

    if (ShouldUpdateBattle || ShouldAdvanceBattle) {
		UpdateBattle_Orig(GameState, DeltaTime);
		ShouldAdvanceBattle = false;

	    const auto engine = asw_engine::get();
	    if (!engine) return;

		asw_player* player_one = engine->players[0].entity;
		asw_player* player_two = engine->players[1].entity;

        if (MatchStartFlag) {
			MatchStartFlag = false;
            initRenderHooks();
        }

		/* Get input state */
		if (input_actor && press_function) {
			for (int idx = 0; idx < ButtonCount; ++idx) {
				InputParamData& queried_key = *ButtonData[idx];
				input_actor->ProcessEvent(press_function, &queried_key);
				ButtonStates[idx] = queried_key.was_pressed;
#if 0
				if (queried_key.was_pressed) {
					RC::Output::send<LogLevel::Warning>(STR("{} Key Was Pressed\n"), RawButtonNames[idx]);
				}
#endif
			}
		}

#if 0
        auto next_hurt = player_one->hurtbox_count;
        auto next_hit = player_one->hitbox_count;
        if (last_hurt != next_hurt || last_hit != next_hit) {
            RC::Output::send<LogLevel::Warning>(STR("Hurt: {}, Hit: {}\n"), next_hurt, next_hit);
        }
        last_hurt = next_hurt;
        last_hit = next_hit;

        if (player_one->action_time == 1) {
            RC::Output::send<LogLevel::Warning>(STR("ACT Reset\n"));
        }
#endif
 
#if 0 // utility to force scan for new offsets of asw_player after an update
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

        int next_ec = engine->entity_count;
        bool player_one_proj = false;
        bool player_two_proj = false;
        for(int idx=0;idx<next_ec;++idx){
            const auto* focus = engine->entities[idx];
            if(focus == player_one || focus == player_two) continue;

            if(focus->parent_obj == player_one) {
                if( focus->is_active() ){
                    player_one_proj = true;
                }
            }
            else if(focus->parent_obj == player_two) {
                if( focus->is_active() ){
                    player_two_proj = true;
                }
            }
        }
        last_ec = next_ec;

		/* Send arcsys state and input state to remote server */
#if 0
		SendFrameData(1, player_one);
		SendFrameData(2, player_two);
		SendInputData(ButtonStates);
#endif

        /* Update Frame Data */
		addFrame(*player_one, *player_two, player_one_proj, player_two_proj);
		if (ButtonStates.at(7)) {
			resetFrames();
		}
	}
}

class StriveFrameData : public CppUserModBase
{
public:
	PLH::x64Detour* UpdateBattle_Detour;
	PLH::x64Detour* MatchStart_Detour;

    StriveFrameData() : CppUserModBase()
    {
    	ModName = STR("Strive Frame Advantage");
    	ModVersion = STR("1.0");
    	ModDescription = STR("A tool to display frame advantage.");
    	ModAuthors = STR("WistfulHopes");
    	UpdateBattle_Detour = nullptr;
    	MatchStart_Detour = nullptr;
    	// Do not change this unless you want to target a UE4SS version
    	// other than the one you're currently building with somehow.
    	//ModIntendedSDKVersion = STR("2.6");
    }

    ~StriveFrameData() override
    {
    }

    auto on_update() -> void override
    {
    }

    auto on_unreal_init() -> void override
    {
    	GameCommon = reinterpret_cast<UREDGameCommon*>(UObjectGlobals::FindFirstOf(FName(STR("REDGameCommon"))));

    	Program = &UE4SSProgram::get_program();

	    const uint64_t UpdateBattle_Addr = sigscan::get().scan("\x40\x53\x57\x41\x54\x41\x55\x48\x81\xEC\x88\x00\x00\x00", "xxxxxxxxxxxxxx");
    	UpdateBattle_Detour = new PLH::x64Detour(
			UpdateBattle_Addr, reinterpret_cast<uint64_t>(&UpdateBattle_New),
			reinterpret_cast<uint64_t*>(&UpdateBattle_Orig));
    	UpdateBattle_Detour->hook();

	    const uint64_t MatchStart_Addr = sigscan::get().scan("\x44\x39\x68\x08\x74\x00\x48\x8B\x18\xEB\x00\x48\x8B\xDF", "xxxxx?xxxx?xxx") - 0x8D;
    	MatchStart_Detour = new PLH::x64Detour(
			MatchStart_Addr, reinterpret_cast<uint64_t>(&MatchStart_New),
			reinterpret_cast<uint64_t*>(&MatchStart_Orig));
    	MatchStart_Detour->hook();
		
	    const uintptr_t GetGameMode_Addr = sigscan::get().scan("\x0F\xB6\x81\xF0\x02\x00\x00\xC3", "xxxxxxxx");
    	GetGameMode = reinterpret_cast<GetGameMode_Func>(GetGameMode_Addr);
    	
    	ASWInitFunctions();
    	bbscript::BBSInitializeFunctions();
    }
};

extern "C"
{
    STRIVEFRAMEDATA_API CppUserModBase* start_mod()
    {
        return new StriveFrameData();
    }

    STRIVEFRAMEDATA_API void uninstall_mod(CppUserModBase* mod)
    {
        delete mod;
    }
}