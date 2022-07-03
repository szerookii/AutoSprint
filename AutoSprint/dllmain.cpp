#include "pch.h"

#include <Windows.h>
#include <Psapi.h>
#include <cassert>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#pragma warning(disable : 4996)

#include "lib/MinHook.h"

#define INRANGE(x,a,b)   (x >= a && x <= b)
#define GET_BYTE( x )    (GET_BITS(x[0]) << 4 | GET_BITS(x[1]))
#define GET_BITS( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))

uintptr_t findSig(const char* szSignature) {
	const char* pattern = szSignature;
	uintptr_t firstMatch = 0;
	static const uintptr_t rangeStart = (uintptr_t)GetModuleHandleA("Minecraft.Windows.exe");
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
}

struct Vec3 {
	union {
		struct {
			float x, y, z;
		};
		float arr[3];
	};

	Vec3() { x = y = z = 0; }
	Vec3(float x, float y, float z) {
		this->x = x, this->y = y, this->z = z;
	}

	float magnitudexz() {
		return sqrtf(x * x + z * z);
	}
};

class Player {
public:
	Vec3 velocity() {
		return *(Vec3*)((uintptr_t)(this) + 0x4f0);
	}

	void setSprinting(bool value) {
		using setSprinting = void(*)(void*, bool);
		static uintptr_t setSprintingAddr = NULL;

		if (setSprintingAddr == NULL)
			setSprintingAddr = findSig("48 89 5C 24 ? 57 48 83 EC ? 48 8B 81 ? ? ? ? 0F B6 DA");

		((setSprinting)setSprintingAddr)(this, value);
	}
};

class GameMode {
public:
	Player* player;

private:
	virtual ~GameMode();
};

void** getVtable(void* obj) {
	return *((void***)obj);
}

void(*oGameMode_tick)(GameMode*);
void hGameMode_tick(GameMode* gm) {
	if (gm->player != nullptr) {
		if (gm->player->velocity().magnitudexz() > 0.05f) {
			gm->player->setSprinting(true);
		}
	}

    oGameMode_tick(gm);
}

void Inject(HINSTANCE hModule) {
    MH_Initialize();

    uintptr_t sigAddr = findSig("48 8D 05 ? ? ? ? 48 8B D9 48 89 01 8B FA 48 8B 89 ? ? ? ? 48 85 C9 74 ? 48 8B 01 BA ? ? ? ? FF 10 48 8B 8B");
    
    if (!sigAddr)
        return;

	int offset = *(int*)(sigAddr + 3);
	uintptr_t** vtable = (uintptr_t**)(sigAddr + offset + 7);

    if (MH_CreateHook((void*)vtable[8], &hGameMode_tick, (LPVOID*)&oGameMode_tick) == MH_OK) {
        MH_EnableHook((void*)vtable[8]);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Inject, hModule, NULL, NULL);

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
