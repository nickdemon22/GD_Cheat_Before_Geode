// GD Menu - внутренний мод для Geometry Dash 2.209 (Windows, x64)
// Загружается как прокси XInput9_1_0.dll (см. proxy.cpp).
// Меню: INSERT. Функции: Noclip. Журнал: GDMenu.log рядом с GeometryDash.exe

#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------------
// Журнал
// ---------------------------------------------------------------------------
void dbg(const char* fmt, ...) {
    static wchar_t path[MAX_PATH] = {};
    if (!path[0]) {
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        wchar_t* s = wcsrchr(path, L'\\');
        if (s) *(s + 1) = 0;
        wcscat_s(path, L"GDMenu.log");
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") != 0 || !f) return;
    va_list a;
    va_start(a, fmt);
    vfprintf(f, fmt, a);
    va_end(a);
    fputc('\n', f);
    fclose(f);
}

// ---------------------------------------------------------------------------
// Смещения (RVA от базы GeometryDash.exe), версия 2.209
// ---------------------------------------------------------------------------
namespace offsets {
    constexpr uintptr_t PlayLayer_destroyPlayer = 0x512390;
}

// Первые байты функции (из Ghidra). Защита от хука не того места.
static const uint8_t k_destroyPrologue[] = {
    0x48, 0x8B, 0xC4,          // mov rax, rsp
    0x48, 0x89, 0x58, 0x18,    // mov [rax+18h], rbx
    0x55, 0x56, 0x57           // push rbp / rsi / rdi
};

// ---------------------------------------------------------------------------
// Настройки
// ---------------------------------------------------------------------------
struct Settings {
    bool menuOpen = true;
    bool noclip = false;
    bool keepNoObjectDeaths = false; // не блокировать смерть без объекта столкновения
} g;

// ---------------------------------------------------------------------------
// Хук PlayLayer::destroyPlayer(PlayerObject* player, GameObject* obj)
// ---------------------------------------------------------------------------
using destroyPlayer_t = void(*)(void* self, void* player, void* obj);
static destroyPlayer_t o_destroyPlayer = nullptr;

static void h_destroyPlayer(void* self, void* player, void* obj) {
    if (g.noclip) {
        if (obj != nullptr || !g.keepNoObjectDeaths) return;
    }
    o_destroyPlayer(self, player, obj);
}

// ---------------------------------------------------------------------------
// ImGui / окно
// ---------------------------------------------------------------------------
static HWND    g_hwnd = nullptr;
static WNDPROC o_wndProc = nullptr;
static bool    g_inited = false;
static bool    g_inSwap = false;
static int     g_frames = 0;

static LRESULT CALLBACK h_wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_INSERT && !(lp & (1 << 30))) {
        g.menuOpen = !g.menuOpen;
        dbg("INSERT pressed, menuOpen=%d", (int)g.menuOpen);
        if (g_inited) ImGui::GetIO().MouseDrawCursor = g.menuOpen;
        return 0;
    }
    if (g_inited && g.menuOpen) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return 0;
        if (io.WantCaptureKeyboard &&
            (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR ||
             msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)) return 0;
    }
    return CallWindowProcW(o_wndProc, hwnd, msg, wp, lp);
}

static void initImGui(HDC hdc) {
    g_hwnd = WindowFromDC(hdc);
    dbg("initImGui: hdc=%p hwnd=%p", (void*)hdc, (void*)g_hwnd);
    if (!g_hwnd) return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.MouseDrawCursor = g.menuOpen;
    ImGui::StyleColorsDark();

    bool okWin = ImGui_ImplWin32_Init(g_hwnd);
    bool okGl = ImGui_ImplOpenGL3_Init();
    dbg("ImGui init: win32=%d opengl3=%d, GL_VERSION=%s", (int)okWin, (int)okGl,
        (const char*)glGetString(GL_VERSION));

    o_wndProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)h_wndProc);
    g_inited = okWin && okGl;
}

// Шрифт ImGui по умолчанию без кириллицы, поэтому подписи на английском.
static void drawMenu() {
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 190), ImGuiCond_FirstUseEver);
    ImGui::Begin("GD Menu", &g.menuOpen);

    ImGui::Checkbox("Noclip", &g.noclip);
    if (g.noclip) {
        ImGui::Indent();
        ImGui::Checkbox("Keep deaths without object", &g.keepNoObjectDeaths);
        ImGui::Unindent();
    }

    // Новые функции добавляй сюда:
    // ImGui::SliderFloat("Speed", &g.speed, 0.1f, 2.0f);

    ImGui::Separator();
    ImGui::TextDisabled("INSERT - show/hide");
    ImGui::End();
}

static void renderFrame(HDC hdc) {
    if (!g_inited) {
        static bool tried = false;
        if (tried) return;   // не пытаемся инициализировать бесконечно
        tried = true;
        initImGui(hdc);
        if (!g_inited) return;
    }
    if (!g.menuOpen) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    drawMenu();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (g_frames < 3) {
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        dbg("rendered frame %d, display=%.0fx%.0f, cmdlists=%d",
            g_frames, ds.x, ds.y, ImGui::GetDrawData()->CmdListsCount);
    }
    g_frames++;
}

// ---------------------------------------------------------------------------
// Хук opengl32!wglSwapBuffers. Рендер обёрнут в SEH: при исключении
// пишем причину в журнал и отключаем меню, а игра продолжает работать.
// ---------------------------------------------------------------------------
using swap_t = BOOL(WINAPI*)(HDC);
static swap_t o_wglSwap = nullptr;
static bool   g_disabled = false;

static LONG logException(EXCEPTION_POINTERS* ep) {
    void* addr = ep->ExceptionRecord->ExceptionAddress;
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m);
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    dbg("EXCEPTION code=0x%08lX addr=%p module=%s +0x%llx",
        ep->ExceptionRecord->ExceptionCode, addr, name,
        (unsigned long long)((uintptr_t)addr - (uintptr_t)m));
    return EXCEPTION_EXECUTE_HANDLER;
}

static void safeRender(HDC hdc) {
    if (g_disabled) return;
    __try {
        renderFrame(hdc);
    } __except (logException(GetExceptionInformation())) {
        g_disabled = true;
        dbg("menu rendering disabled after exception");
    }
}

static BOOL WINAPI h_wglSwap(HDC hdc) {
    safeRender(hdc);
    return o_wglSwap(hdc);
}

// ---------------------------------------------------------------------------
// Инициализация (отдельный поток, не в DllMain)
// ---------------------------------------------------------------------------
static void setupHooks() {
    dbg("---- mod started ----");

    HMODULE gl = nullptr;
    while (!(gl = GetModuleHandleA("opengl32.dll"))) Sleep(100);
    dbg("opengl32.dll loaded");

    // даём игре спокойно дойти до создания окна
    Sleep(3000);
    dbg("installing hooks");

    MH_STATUS st = MH_Initialize();
    dbg("MH_Initialize: %s", MH_StatusToString(st));
    if (st != MH_OK) return;

    if (void* p = (void*)GetProcAddress(gl, "wglSwapBuffers")) {
        st = MH_CreateHook(p, (void*)h_wglSwap, (void**)&o_wglSwap);
        dbg("hook wglSwapBuffers (%p): %s", p, MH_StatusToString(st));
    } else {
        dbg("wglSwapBuffers not found");
    }

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    void* target = (void*)(base + offsets::PlayLayer_destroyPlayer);
    if (memcmp(target, k_destroyPrologue, sizeof(k_destroyPrologue)) == 0) {
        st = MH_CreateHook(target, (void*)h_destroyPlayer, (void**)&o_destroyPlayer);
        dbg("hook destroyPlayer (%p): %s", target, MH_StatusToString(st));
    } else {
        dbg("destroyPlayer prologue MISMATCH at base+0x%llx", (unsigned long long)offsets::PlayLayer_destroyPlayer);
        MessageBoxA(nullptr,
            "destroyPlayer was not found at the expected offset (different game version?).\n"
            "Noclip is disabled, the menu still works.",
            "GD Menu", MB_ICONWARNING);
    }

    st = MH_EnableHook(MH_ALL_HOOKS);
    dbg("MH_EnableHook(ALL): %s", MH_StatusToString(st));
}

static DWORD WINAPI mainThread(LPVOID) {
    __try {
        setupHooks();
    } __except (logException(GetExceptionInformation())) {
        dbg("setupHooks aborted by exception");
    }
    return 0;
}

// Вызывается из DllMain в proxy.cpp
void StartMod() {
    CreateThread(nullptr, 0, mainThread, nullptr, 0, nullptr);
}
