// Прокси для XInput1_4.dll.
// libcocos2d.dll импортирует XINPUT1_4.dll. Игра загружает эту DLL из своей
// папки вместо системной, мы запускаем мод и пересылаем все вызовы
// настоящему XInput1_4.dll из System32.

#include <windows.h>

void StartMod();
void dbg(const char* fmt, ...);

static HMODULE g_real = nullptr;

static HMODULE realMod() {
    if (!g_real) {
        wchar_t path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        wcscat_s(path, L"\\XInput1_4.dll");
        g_real = LoadLibraryW(path);
        dbg("proxy: real XInput1_4 loaded=%p err=%lu", (void*)g_real, g_real ? 0UL : GetLastError());
    }
    return g_real;
}
static FARPROC realFn(const char* name) {
    HMODULE m = realMod();
    return m ? GetProcAddress(m, name) : nullptr;
}
static FARPROC realOrd(WORD ord) {
    HMODULE m = realMod();
    return m ? GetProcAddress(m, MAKEINTRESOURCEA(ord)) : nullptr;
}

// Экспорт с пересылкой по имени
#define FORWARD(name, sig, call)                                              \
    extern "C" DWORD WINAPI name sig {                  \
        using F = DWORD(WINAPI*) sig;                                         \
        static F f = nullptr;                                                 \
        if (!f) f = (F)realFn(#name);                                         \
        return f ? f call : ERROR_DEVICE_NOT_CONNECTED;                       \
    }

FORWARD(XInputGetState,           (DWORD a, void* b),                         (a, b))
FORWARD(XInputSetState,           (DWORD a, void* b),                         (a, b))
FORWARD(XInputGetCapabilities,    (DWORD a, DWORD b, void* c),                (a, b, c))
FORWARD(XInputGetBatteryInformation, (DWORD a, BYTE b, void* c),              (a, b, c))
FORWARD(XInputGetKeystroke,       (DWORD a, DWORD b, void* c),                (a, b, c))
FORWARD(XInputGetAudioDeviceIds,  (DWORD a, LPWSTR b, UINT* c, LPWSTR d, UINT* e), (a, b, c, d, e))

extern "C" void WINAPI XInputEnable(BOOL enable) {
    using F = void(WINAPI*)(BOOL);
    static F f = nullptr;
    if (!f) f = (F)realFn("XInputEnable");
    if (f) f(enable);
}

// Экспорты только по номерам (в XInput1_4 это 100-104)
#define FORWARD_ORD(impl, ord, sig, call)                                     \
    extern "C" DWORD WINAPI impl sig {                                        \
        using F = DWORD(WINAPI*) sig;                                         \
        static F f = nullptr;                                                 \
        if (!f) f = (F)realOrd(ord);                                          \
        return f ? f call : ERROR_DEVICE_NOT_CONNECTED;                       \
    }

FORWARD_ORD(XInputGetStateEx_impl,            100, (DWORD a, void* b),        (a, b))
FORWARD_ORD(XInputWaitForGuideButton_impl,    101, (DWORD a, DWORD b, void* c), (a, b, c))
FORWARD_ORD(XInputCancelGuideButtonWait_impl, 102, (DWORD a),                 (a))
FORWARD_ORD(XInputPowerOffController_impl,    103, (DWORD a),                 (a))
FORWARD_ORD(XInputGetBaseBusInformation_impl, 104, (DWORD a, void* b),        (a, b))
FORWARD_ORD(XInputOrdinal108_impl,            108, (ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d), (a, b, c, d))

// Номера экспортов ДОЛЖНЫ совпадать с системным XInput1_4.dll:
// libcocos2d.dll импортирует XInputGetState по номеру (ordinal 2).
#pragma comment(linker, "/EXPORT:XInputGetState,@2")
#pragma comment(linker, "/EXPORT:XInputSetState,@3")
#pragma comment(linker, "/EXPORT:XInputGetCapabilities,@4")
#pragma comment(linker, "/EXPORT:XInputEnable,@5")
#pragma comment(linker, "/EXPORT:XInputGetBatteryInformation,@7")
#pragma comment(linker, "/EXPORT:XInputGetKeystroke,@8")
#pragma comment(linker, "/EXPORT:XInputGetAudioDeviceIds,@10")
#pragma comment(linker, "/EXPORT:XInputGetStateEx=XInputGetStateEx_impl,@100,NONAME")
#pragma comment(linker, "/EXPORT:XInputWaitForGuideButton=XInputWaitForGuideButton_impl,@101,NONAME")
#pragma comment(linker, "/EXPORT:XInputCancelGuideButtonWait=XInputCancelGuideButtonWait_impl,@102,NONAME")
#pragma comment(linker, "/EXPORT:XInputPowerOffController=XInputPowerOffController_impl,@103,NONAME")
#pragma comment(linker, "/EXPORT:XInputGetBaseBusInformation=XInputGetBaseBusInformation_impl,@104,NONAME")
#pragma comment(linker, "/EXPORT:XInputOrdinal108=XInputOrdinal108_impl,@108,NONAME")

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        dbg("proxy: DLL_PROCESS_ATTACH");
        StartMod();
    }
    return TRUE;
}
