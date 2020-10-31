#include "pch.h"
#include <vector>

uintptr_t FindDMAAddy(uintptr_t ptr, std::vector<unsigned int> offsets)
{
    uintptr_t addr = ptr;
    for (unsigned int i = 0; i < offsets.size(); ++i)
    {
        addr = *(uintptr_t*)addr;
        addr += offsets[i];
    }
    return addr;
}

void Inject(HINSTANCE hModule) {
    uintptr_t baseAddr = (uintptr_t)GetModuleHandleW(L"Minecraft.Windows.exe");
    uintptr_t addr = FindDMAAddy(baseAddr + 0x0365A6D8, { 0x10, 0x128, 0x0, 0xE8, 0xC });

    while (true) {
        *(INT*)(addr) = 16777473;
        Sleep(5);
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
