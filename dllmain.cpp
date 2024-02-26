#include <windows.h>
#include <Shlobj.h>
#include <Psapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <MinHook.h>

#pragma comment(lib, "MinHook.x64.lib")
#pragma warning(disable : 4996)

std::wstring acFolderPath;
std::wofstream modLog;

#define INRANGE(x, a, b) (x >= a && x <= b)
#define GET_BYTE(x) (GET_BITS(x[0]) << 4 | GET_BITS(x[1]))
#define GET_BITS(x)                                                            \
  (INRANGE((x & (~0x20)), 'A', 'F') ? ((x & (~0x20)) - 'A' + 0xa)              \
                                    : (INRANGE(x, '0', '9') ? x - '0' : 0))

auto findSig(const char* szSignature) -> uintptr_t {
    const char* pattern = szSignature;
    uintptr_t firstMatch = 0;

    static const auto rangeStart = (uintptr_t) GetModuleHandleA("Minecraft.Windows.exe");
    static MODULEINFO miModInfo;
    static bool init = false;

    if (!init) {
        init = true;
        GetModuleInformation(GetCurrentProcess(), (HMODULE) rangeStart, &miModInfo, sizeof(MODULEINFO));
    }

    static const uintptr_t rangeEnd = rangeStart + miModInfo.SizeOfImage;

    BYTE patByte = GET_BYTE(pattern);
    const char* oldPat = pattern;

    for (uintptr_t pCur = rangeStart; pCur < rangeEnd; pCur++) {
        if (!*pattern)
            return firstMatch;

        while (*(PBYTE) pattern == ' ')
            pattern++;

        if (!*pattern)
            return firstMatch;

        if (oldPat != pattern) {
            oldPat = pattern;
            if (*(PBYTE) pattern != '\?')
                patByte = GET_BYTE(pattern);
        }

        if (*(PBYTE) pattern == '\?' || *(BYTE *) pCur == patByte) {
            if (!firstMatch)
                firstMatch = pCur;

            if (!pattern[2] || !pattern[1])
                return firstMatch;

            pattern += 2;
        } else {
            pattern = szSignature;
            firstMatch = 0;
        }
    }

    return 0;
}

[[maybe_unused]] auto getVtable(void* obj) -> void** {
    return *((void***) obj);
}

class Player {
public:
    auto setSprinting(bool value) -> void {
        using setSprinting = void(*)(void*, bool);
        static uintptr_t setSprintingAddr = NULL;

        if (setSprintingAddr == NULL) {
            setSprintingAddr = (uintptr_t)getVtable(this)[165];

            return;
        }

        ((setSprinting) setSprintingAddr)(this, value);
    }
};

class GameMode {
public:
    void** vtable{};
    Player* player{};
};

void (*oGameMode_tick)(GameMode*);

auto hGameMode_tick(GameMode* _this) -> void {
    if (_this->player != nullptr) {
        _this->player->setSprinting(true);
    }

    oGameMode_tick(_this);
}

auto Inject(HINSTANCE hModule) -> void {
    MH_Initialize();

    PWSTR pAppDataPath;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pAppDataPath);
    acFolderPath = pAppDataPath;
    CoTaskMemFree(pAppDataPath);

    modLog.open(acFolderPath + L"\\AutoSprint.txt");

    uintptr_t sigAddr = findSig("48 8D 05 ? ? ? ? 48 89 01 48 89 51 ? 48 C7 41");

    if (!sigAddr)
        return;

    int offset = *(int*)(sigAddr + 3);
    auto **vtable = (uintptr_t**)(sigAddr + offset + 7);

    if (MH_CreateHook((void*) vtable[8], (void*) &hGameMode_tick,(void**) &oGameMode_tick) == MH_OK) {
        MH_EnableHook((void*) vtable[8]);
    }
}

auto APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) -> BOOL {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE) Inject, hModule, NULL,nullptr);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    return TRUE;
}
