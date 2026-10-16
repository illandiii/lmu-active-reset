#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <initializer_list>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include "active_reset_hotkeys_win32.h"
extern "C" {
#include "active_reset_native.h"
#include "../restore/restore_joint_25661166.h"
}
/* Standalone owned-memory laboratory. Does not open or attach to LMU. */
static ResetNativeRuntime runtime_state;
static RestorePlan physical_plan;
static ResetMailbox mailbox{};
static ResetHotkeys hotkeys{};
struct Model {
    alignas(8) unsigned char body[0x200], score[0x27400];
    double checkpoint[3], before[3], contact[3];
    ResetIdentity identity;
    DWORD thread;
    bool allowed, held, running, fail_contact, fail_recovery, quarantined;
    int last_result;
    uint64_t contact_calls;
};
static Model model{};
static void position(double *p) {
    memcpy(p, model.body + 0x40, 24);
}
static void set_position(const double *p) {
    memcpy(model.body + 0x40, p, 24);
}
static int validate(void *, uint64_t epoch) {
    return model.allowed && model.held && GetCurrentThreadId() == model.thread &&
           epoch == model.identity.session;
}
static int enter(void *, uint64_t epoch) {
    if (!model.allowed || model.held || GetCurrentThreadId() != model.thread ||
        epoch != model.identity.session)
        return 0;
    position(model.before);
    model.held = true;
    return 1;
}
static int owner(void *, uint64_t epoch) {
    return model.held && GetCurrentThreadId() == model.thread && epoch == model.identity.session;
}
static int contacts(void *, uint64_t epoch, int original) {
    if (!owner(nullptr, epoch))
        return 0;
    ++model.contact_calls;
    double now[3];
    position(now);
    if (memcmp(now, original ? model.before : model.checkpoint, 24))
        return 0;
    if (!original && model.fail_contact) {
        model.fail_contact = false;
        model.contact[0] = 999;
        return 0;
    }
    if (original && model.fail_recovery)
        return 0;
    memcpy(model.contact, now, 24);
    return 1;
}
static void leave(void *, int resume) {
    model.held = false;
    if (!resume)
        model.running = false;
}
static void quarantine(void *, int) {
    model.quarantined = true;
    model.running = false;
}
static int capture(void *, const ResetIdentity *) {
    position(model.checkpoint);
    return 0;
}
static int prepare(void *, const ResetIdentity *identity, RestorePlan *out,
                   RestoreBindings *bindings, uintptr_t *score) {
    physical_plan.epoch = identity->session;
    physical_plan.count = 1;
    RestoreField &f = physical_plan.fields[0];
    memset(&f, 0, sizeof(f));
    f.field_offset = 0x40;
    f.size = 24;
    memcpy(f.expected, model.body + 0x40, 24);
    memcpy(f.desired, model.checkpoint, 24);
    LapGuardFlags flags{};
    memcpy(&flags.current, model.score + 0x160, 4);
    memcpy(&flags.remembered, model.score + 0x27290, 4);
    memcpy(&flags.last_computed, model.score + 0x27360, 4);
    flags.next_timed = 1;
    bindings->epoch = identity->session;
    bindings->body_count = 1;
    bindings->address[0] = (uintptr_t)model.body;
    *score = (uintptr_t)model.score;
    return restore_joint_prepare(&physical_plan, &flags, out);
}
static ResetNativeAdapter adapter = {
    nullptr, capture, prepare, validate, {nullptr, enter, owner, contacts, leave, quarantine}};
static void initialize() {
    memset(&runtime_state, 0, sizeof(runtime_state));
    memset(&model, 0, sizeof(model));
    model.thread = GetCurrentThreadId();
    model.identity = {1, 1, 1, 1};
    model.allowed = true;
    model.running = true;
    runtime_state.control.owner_thread = model.thread;
    double p[3] = {10, 0, 0};
    set_position(p);
    memcpy(model.contact, p, 24);
    for (unsigned off : {0x160u, 0x27290u, 0x27360u}) {
        int32_t flag = 2;
        memcpy(model.score + off, &flag, 4);
    }
    double record = 87.4541;
    memcpy(model.score + 0x180, &record, 8);
    InitializeSRWLock(&mailbox.lock);
    mailbox.pending = false;
    assert(reset_hotkeys_configure(&hotkeys, GetCurrentProcessId(), VK_F5, VK_F9));
}
static bool post(int action, uint64_t now) {
    ResetRequest r{++hotkeys.next_id, now, model.identity, (uint32_t)action};
    return reset_mailbox_post(&mailbox, r);
}
static int step(uint64_t now) {
    ResetRequest request{};
    bool pending = reset_mailbox_take(&mailbox, request);
    return active_reset_native_step(&runtime_state, &adapter, model.thread, now, &model.identity,
                                    model.allowed, pending ? &request : nullptr);
}
static void synthetic_key(int action, uint64_t now) {
    active_reset_key_poll(&hotkeys.keys, 1, 0, 0, 0);
    int edge =
        active_reset_key_poll(&hotkeys.keys, 1, action == RESET_SAVE, action == RESET_LOAD, 0);
    assert(edge == action);
    assert(post(edge, now));
}
static int self_test() {
    initialize();
    assert(!reset_hotkeys_configure(&hotkeys, GetCurrentProcessId(), VK_F5, VK_F5));
    synthetic_key(RESET_LOAD, 100);
    assert(step(100) == CONTROL_NO_CHECKPOINT);
    synthetic_key(RESET_SAVE, 110);
    assert(step(110) == CONTROL_OK);
    uint64_t sequence = hotkeys.next_id;
    assert(reset_hotkeys_configure(&hotkeys, GetCurrentProcessId(), VK_F6, VK_F10) &&
           hotkeys.next_id == sequence);
    assert(!reset_hotkeys_configure(&hotkeys, GetCurrentProcessId(), VK_F6, VK_F6) &&
           hotkeys.next_id == sequence && hotkeys.load_vk == VK_F10);
    double p[3] = {50, 2, 3};
    set_position(p);
    synthetic_key(RESET_LOAD, 120);
    assert(step(120) == CONTROL_OK);
    double restored[3];
    position(restored);
    assert(!memcmp(restored, model.checkpoint, 24) && !memcmp(model.contact, model.checkpoint, 24));
    double record;
    memcpy(&record, model.score + 0x180, 8);
    assert(record == 87.4541);
    int32_t flag;
    memcpy(&flag, model.score + 0x160, 4);
    assert(flag == 0);
    set_position(p);
    model.fail_contact = true;
    synthetic_key(RESET_LOAD, 130);
    assert(step(130) == CONTROL_RESTORE_RECOVERED);
    position(restored);
    assert(!memcmp(restored, p, 24) && !memcmp(model.contact, p, 24));
    synthetic_key(RESET_LOAD, 140);
    model.allowed = false;
    assert(step(140) == CONTROL_REJECTED);
    model.allowed = true;
    assert(post(RESET_SAVE, 150));
    assert(!post(RESET_LOAD, 150));
    assert(step(150) == CONTROL_OK);
    AcquireSRWLockExclusive(&mailbox.lock);
    ResetRequest busy{};
    assert(!reset_mailbox_take(&mailbox, busy));
    ReleaseSRWLockExclusive(&mailbox.lock);
    model.identity.session++;
    assert(step(160) == CONTROL_NO_REQUEST && !runtime_state.control.checkpoint_valid);
    synthetic_key(RESET_SAVE, 170);
    assert(step(170) == CONTROL_OK);
    p[0] = 70;
    set_position(p);
    model.fail_contact = model.fail_recovery = true;
    synthetic_key(RESET_LOAD, 180);
    assert(step(180) == CONTROL_QUARANTINED && model.quarantined && !model.running);
    puts("PASS offline end-to-end: configurable key edges -> bounded mailbox -> owner-thread "
         "capture/prepare -> Win32 physical/scoring commit -> contacts -> compensation/fatal "
         "lockout; record bytes preserved.");
    return 0;
}
static const wchar_t *status_text(int result) {
    switch (result) {
    case CONTROL_OK:
        return L"Operazione completata";
    case CONTROL_NO_CHECKPOINT:
        return L"Salva prima un checkpoint";
    case CONTROL_SAVE_FAILED:
        return L"Salvataggio fallito; checkpoint precedente conservato";
    case CONTROL_RESTORE_RECOVERED:
        return L"Errore contatti: stato precedente recuperato";
    case CONTROL_QUARANTINED:
        return L"Recupero fallito: simulazione bloccata, riavvia il banco";
    case CONTROL_REJECTED:
        return L"Comando scartato per contesto o scadenza";
    default:
        return L"Pronto";
    }
}
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_CREATE) {
        CreateWindowW(L"BUTTON", L"Salva", WS_VISIBLE | WS_CHILD, 25, 260, 130, 35, window,
                      (HMENU)1, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Ripristina", WS_VISIBLE | WS_CHILD, 170, 260, 130, 35, window,
                      (HMENU)2, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Pausa / Riprendi", WS_VISIBLE | WS_CHILD, 315, 260, 170, 35,
                      window, (HMENU)3, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Errore contatti al prossimo reset", WS_VISIBLE | WS_CHILD, 25,
                      310, 300, 35, window, (HMENU)4, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Nuova sessione", WS_VISIBLE | WS_CHILD, 345, 310, 180, 35,
                      window, (HMENU)5, nullptr, nullptr);
        SetTimer(window, 1, 20, nullptr);
        return 0;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(w)) {
        case 1:
            post(RESET_SAVE, GetTickCount64());
            break;
        case 2:
            post(RESET_LOAD, GetTickCount64());
            break;
        case 3:
            if (!model.quarantined)
                model.running = !model.running;
            break;
        case 4:
            model.fail_contact = true;
            break;
        case 5:
            model.identity.session++;
            break;
        }
        return 0;
    }
    if (message == WM_TIMER) {
        model.allowed = GetForegroundWindow() == window;
        if (model.running && !model.quarantined) {
            double p[3];
            position(p);
            p[0] += .06;
            set_position(p);
            memcpy(model.contact, p, 24);
        }
        reset_hotkeys_poll(&hotkeys, model.identity, &mailbox);
        int result = step(GetTickCount64());
        if (result != CONTROL_NO_REQUEST)
            model.last_result = result;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window, &ps);
        RECT r;
        GetClientRect(window, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_WINDOW + 1));
        double p[3];
        position(p);
        wchar_t text[1200];
        swprintf(text, 1200,
                 L"LMU Active Reset - Banco OFFLINE\n\nQuesto programma usa solo memoria propria. "
                 L"Non si collega a LMU.\n\nF%d: salva | F%d: ripristina (solo con questa finestra "
                 L"in primo piano)\nPosizione simulata: %.3f    Contatti: %.3f\nCheckpoint: %ls    "
                 L"Sessione: %llu\nSalvataggi: %llu    Reset riusciti: %llu\n\n%ls",
                 hotkeys.save_vk - VK_F1 + 1, hotkeys.load_vk - VK_F1 + 1, p[0], model.contact[0],
                 runtime_state.control.checkpoint_valid ? L"disponibile" : L"assente",
                 (unsigned long long)model.identity.session,
                 (unsigned long long)runtime_state.control.saves,
                 (unsigned long long)runtime_state.control.loads, status_text(model.last_result));
        r.left = 25;
        r.top = 20;
        DrawTextW(dc, text, -1, &r, DT_LEFT | DT_TOP);
        EndPaint(window, &ps);
        return 0;
    }
    if (message == WM_DESTROY) {
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
static int configured_key(const wchar_t *name, const wchar_t *fallback, const wchar_t *path) {
    wchar_t text[32];
    GetPrivateProfileStringW(L"Hotkeys", name, fallback, text, 32, path);
    wchar_t *end = nullptr;
    long n = wcstol(text + 1, &end, 10);
    return text[0] == L'F' && end && !*end && n >= 1 && n <= 24 ? VK_F1 + (int)n - 1 : 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--self-test"))
        return self_test();
    initialize();
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t *name = wcsrchr(path, L'\\');
    if (!name)
        return 2;
    wcscpy(name + 1, L"active-reset-lab.ini");
    if (!reset_hotkeys_configure(&hotkeys, GetCurrentProcessId(),
                                 configured_key(L"Save", L"F5", path),
                                 configured_key(L"Reset", L"F9", path))) {
        MessageBoxW(nullptr,
                    L"Configura due tasti distinti fra F1 e F24 nel file active-reset-lab.ini.",
                    L"Hotkey non valide", MB_OK | MB_ICONERROR);
        return 2;
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW cls{};
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.lpszClassName = L"LMUActiveResetOfflineLab";
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&cls);
    HWND window =
        CreateWindowW(cls.lpszClassName, L"LMU Active Reset - Banco offline", WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, 740, 420, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 3;
    ShowWindow(window, SW_SHOW);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
