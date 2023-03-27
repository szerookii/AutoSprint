#include <windows.h>
#include <Psapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <Shlobj.h>

#include <MinHook.h>

#pragma comment(lib, "MinHook.x64.lib")
#pragma warning(disable : 4996)

std::wstring acFolderPath;
std::wofstream modLog;

#define INRANGE(x,a,b)   (x >= a && x <= b)
#define GET_BYTE( x )    (GET_BITS(x[0]) << 4 | GET_BITS(x[1]))
#define GET_BITS( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))

auto findSig(const char* szSignature) -> uintptr_t {
	const char* pattern = szSignature;
	uintptr_t firstMatch = 0;

	static const auto rangeStart = (uintptr_t)GetModuleHandleA("Minecraft.Windows.exe");
	static MODULEINFO miModInfo;
	static bool init = false;

	if (!init) {
		init = true;
		GetModuleInformation(GetCurrentProcess(), (HMODULE)rangeStart, &miModInfo, sizeof(MODULEINFO));
	}

	static const uintptr_t rangeEnd = rangeStart + miModInfo.SizeOfImage;

	BYTE patByte = GET_BYTE(pattern);
	const char* oldPat = pattern;

	for (uintptr_t pCur = rangeStart; pCur < rangeEnd; pCur++) {
		if (!*pattern)
			return firstMatch;

		while (*(PBYTE)pattern == ' ')
			pattern++;

		if (!*pattern)
			return firstMatch;

		if (oldPat != pattern) {
			oldPat = pattern;
			if (*(PBYTE)pattern != '\?')
				patByte = GET_BYTE(pattern);
		}

		if (*(PBYTE)pattern == '\?' || *(BYTE*)pCur == patByte) {
			if (!firstMatch)
				firstMatch = pCur;

			if (!pattern[2] || !pattern[1])
				return firstMatch;

			pattern += 2;
		}
		else {
			pattern = szSignature;
			firstMatch = 0;
		}
	}

    return 0;
}

struct Vec3 {
	union {
		struct {
			float x, y, z;
		};
		float arr[3]{};
	};

	Vec3() { x = y = z = 0; }

    [[maybe_unused]] Vec3(float x, float y, float z) {
		this->x = x, this->y = y, this->z = z;
	}

	[[nodiscard]] auto magnitude() const -> float {
		return sqrtf(x * x + z * z);
	}
};

class Player {
public:
	auto position() -> Vec3 {
		return *(Vec3*)((uintptr_t)(this) + 0x7BC);
	}

	void setSprinting(bool value) {
		using setSprinting = void(*)(void*, bool);
		static uintptr_t setSprintingAddr = NULL;

		if (setSprintingAddr == NULL) {
			setSprintingAddr = findSig("48 89 74 24 20 57 48 83 EC 30 48 8B 01 0F B6 F2 BA 03 00 00 00");
			return;
		}
			
		((setSprinting)setSprintingAddr)(this, value);
	}
};

class GameMode {
public:
	Player* player{};

public:
	virtual ~GameMode() = delete;
};

auto getVtable(void* obj) -> void** {
	return *((void***)obj);
}

void(*oGameMode_tick)(GameMode*);
auto hGameMode_tick(GameMode* gm) -> void {
	if (gm->player != nullptr) {
		if (gm->player->position().magnitude() > 0.05f) {
			gm->player->setSprinting(true);
		}
	}

    oGameMode_tick(gm);
}

auto Inject(HINSTANCE hModule) -> void {
    MH_Initialize();

	PWSTR pAppDataPath;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pAppDataPath);
	acFolderPath = pAppDataPath;
	CoTaskMemFree(pAppDataPath);

	modLog.open(acFolderPath + L"\\AutoSprint.txt");

    uintptr_t sigAddr = findSig("48 8D ? ? ? ? ? 48 8B D9 48 89 01 48 8B 89 B0 00 00 00 48 85 C9 74 11 48 8B 01 BA 01 00 00 00 48 8B 00 FF 15 ? ? ? ? 48 8B 8B A8 00 00 00 48 85 C9 74 17 48 8B 01 BA 01 00 00 00 48 8B 00 48 83 C4 20 5B 48 FF 25 ? ? ? ? 48 83 C4 20 5B C3 CC CC CC CC CC CC CC 48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 56 41 57 48 8D 6C 24 A0 48 81 EC 60 01 00 00 48 8B ? ? ? ? ? 48 33 C4 48 89 45 50");
    
    if (!sigAddr)
        return;

	int offset = *(int*)(sigAddr + 3);
	auto** vtable = (uintptr_t**)(sigAddr + offset + 7);

    if (MH_CreateHook((void*)vtable[8], (LPVOID**)&hGameMode_tick, (LPVOID*)&oGameMode_tick) == MH_OK) {
        MH_EnableHook((void*)vtable[8]);
    }
}

auto APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) -> BOOL {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)Inject, hModule, NULL, nullptr);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    return TRUE;
}
