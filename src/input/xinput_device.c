/**
 * Xbox Input Compatibility Layer
 *
 * Translates the Xbox controller API to a host gamepad backend.
 * Handles the structural differences between the Xbox gamepad
 * (analog buttons as bytes, separate trigger channels) and the host
 * (digital face buttons, trigger axes).
 *
 *   _WIN32 -> Windows XInput
 *   POSIX  -> SDL2 GameController
 */

#include "xinput_xbox.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ======================================================================== */
#if defined(_WIN32)
/* ====================  XInput backend  ================================== */
/* ======================================================================== */

#include <xinput.h>
#pragma comment(lib, "xinput.lib")

static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS] = { FALSE };
static DWORD g_last_packet[XBOX_MAX_CONTROLLERS] = { 0 };

/* Optional keyboard controller, following the NFSU2 reference port's mapping.
 * The host window feeds its own key messages here, so typing elsewhere does
 * not operate the game. Keep this state in input rather than making the input
 * library depend on a particular window/presentation implementation. */
static SRWLOCK s_keyboard_lock = SRWLOCK_INIT;
static BYTE s_keys[256];
static DWORD s_key_packet;
/* Opt-in automation aid for low-FPS acceptance runs. Normal input never
 * stretches a key press. Focus loss still cancels every deferred release. */
static ULONGLONG s_key_release_after[256];
static BYTE s_key_release_pending[256];
static unsigned s_pending_releases;
static volatile uint32_t s_test_key_hold_ms=UINT32_MAX;

void xbox_InputSetKeyState(unsigned key, int down)
{
    int changed = 0;
    if (key >= 256)
        return;
    AcquireSRWLockExclusive(&s_keyboard_lock);
    if (down && (!s_keys[key] || s_key_release_pending[key])) {
        uint32_t hold=s_test_key_hold_ms;
        if(hold==UINT32_MAX) {
            const char *e=getenv("RECOMP_TEST_KEY_HOLD_MS");char *end=NULL;
            unsigned long value=e && *e?strtoul(e,&end,10):0;
            hold=(end && *end) || value>10000?0:(uint32_t)value;
            s_test_key_hold_ms=hold;
        }
        s_key_release_after[key]=hold?GetTickCount64()+hold:0;
        if(s_key_release_pending[key]) {
            s_key_release_pending[key]=0;s_pending_releases--;
        }
    }
    if (!down && s_keys[key] && s_key_release_after[key]
        && GetTickCount64()<s_key_release_after[key]) {
        if(!s_key_release_pending[key]) {
            s_key_release_pending[key]=1;s_pending_releases++;changed=1;
        }
    } else if (s_keys[key] != !!down) {
        s_keys[key] = (BYTE)!!down;
        if(!down && s_key_release_pending[key]) {
            s_key_release_pending[key]=0;s_pending_releases--;
        }
        if(!down)s_key_release_after[key]=0;
        s_key_packet++;
        changed = 1;
    }
    ReleaseSRWLockExclusive(&s_keyboard_lock);
    if (changed && getenv("RECOMP_INPUT_TRACE"))
        fprintf(stderr, "  [KEYBOARD] key=%02X %s\n", key, down ? "down" : "up");
}

void xbox_InputClearKeys(void)
{
    unsigned i;
    AcquireSRWLockExclusive(&s_keyboard_lock);
    for (i = 0; i < 256; i++) {
        if (s_keys[i]) {
            memset(s_keys, 0, sizeof(s_keys));
            memset(s_key_release_after,0,sizeof(s_key_release_after));
            memset(s_key_release_pending,0,sizeof(s_key_release_pending));
            s_pending_releases=0;
            s_key_packet++;
            break;
        }
    }
    ReleaseSRWLockExclusive(&s_keyboard_lock);
}

static BOOL keyboard_enabled(void)
{
    const char *v = getenv("RECOMP_KEYBOARD");
    return v && *v && *v != '0';
}

static SHORT keyboard_axis(unsigned negative, unsigned positive)
{
    return (SHORT)((s_keys[positive] - s_keys[negative]) * 32767);
}

static void keyboard_state(XBOX_INPUT_STATE *state)
{
    static const unsigned analog_keys[8] = { 'Z', 'X', 'A', 'S', 'E', 'Q', '1', '3' };
    unsigned i;
    int exclusive=0;
    memset(state, 0, sizeof(*state));
    AcquireSRWLockShared(&s_keyboard_lock);
    if(s_pending_releases) {
        ULONGLONG now;
        ReleaseSRWLockShared(&s_keyboard_lock);
        AcquireSRWLockExclusive(&s_keyboard_lock);exclusive=1;
        now=GetTickCount64();
        for(i=0;i<256;i++)if(s_key_release_pending[i] && now>=s_key_release_after[i]) {
            s_keys[i]=s_key_release_pending[i]=0;s_key_release_after[i]=0;
            s_pending_releases--;s_key_packet++;
        }
    }
    state->dwPacketNumber = s_key_packet;
    if (s_keys[VK_UP])      state->Gamepad.wButtons |= XBOX_GAMEPAD_DPAD_UP;
    if (s_keys[VK_DOWN])    state->Gamepad.wButtons |= XBOX_GAMEPAD_DPAD_DOWN;
    if (s_keys[VK_LEFT])    state->Gamepad.wButtons |= XBOX_GAMEPAD_DPAD_LEFT;
    if (s_keys[VK_RIGHT])   state->Gamepad.wButtons |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (s_keys[VK_RETURN])  state->Gamepad.wButtons |= XBOX_GAMEPAD_START;
    if (s_keys[VK_BACK])    state->Gamepad.wButtons |= XBOX_GAMEPAD_BACK;
    if (s_keys[VK_SHIFT])   state->Gamepad.wButtons |= XBOX_GAMEPAD_LEFT_THUMB;
    if (s_keys[VK_CONTROL]) state->Gamepad.wButtons |= XBOX_GAMEPAD_RIGHT_THUMB;
    for (i = 0; i < 8; i++)
        state->Gamepad.bAnalogButtons[i] = s_keys[analog_keys[i]] ? 255 : 0;
    state->Gamepad.sThumbLX = keyboard_axis(VK_NUMPAD4, VK_NUMPAD6);
    state->Gamepad.sThumbLY = keyboard_axis(VK_NUMPAD2, VK_NUMPAD8);
    state->Gamepad.sThumbRX = keyboard_axis('J', 'L');
    state->Gamepad.sThumbRY = keyboard_axis('K', 'I');
    if(exclusive)ReleaseSRWLockExclusive(&s_keyboard_lock);
    else ReleaseSRWLockShared(&s_keyboard_lock);
}

void xbox_InputInit(void)
{
    for (DWORD i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        g_controller_connected[i] = (result == ERROR_SUCCESS);
    }
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    XINPUT_STATE xi_state;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    result = XInputGetState(dwPort, &xi_state);
    if (result != ERROR_SUCCESS) {
        g_controller_connected[dwPort] = FALSE;
        if (dwPort == 0 && keyboard_enabled()) {
            keyboard_state(pState);
            return ERROR_SUCCESS;
        }
        return result;
    }

    g_controller_connected[dwPort] = TRUE;
    g_last_packet[dwPort] = xi_state.dwPacketNumber;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = xi_state.dwPacketNumber;
    pState->Gamepad.wButtons = xi_state.Gamepad.wButtons & 0x00FF;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] = xi_state.Gamepad.bLeftTrigger;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] = xi_state.Gamepad.bRightTrigger;

    pState->Gamepad.sThumbLX = xi_state.Gamepad.sThumbLX;
    pState->Gamepad.sThumbLY = xi_state.Gamepad.sThumbLY;
    pState->Gamepad.sThumbRX = xi_state.Gamepad.sThumbRX;
    pState->Gamepad.sThumbRY = xi_state.Gamepad.sThumbRY;

    /* Merge with a real controller as well. Both sources' packet counters
     * contribute, so an unchanged keyboard cannot hide a physical pad edge. */
    if (dwPort == 0 && keyboard_enabled()) {
        XBOX_INPUT_STATE kb;
        unsigned i;
        keyboard_state(&kb);
        pState->dwPacketNumber += kb.dwPacketNumber;
        pState->Gamepad.wButtons |= kb.Gamepad.wButtons;
        for (i = 0; i < 8; i++)
            if (kb.Gamepad.bAnalogButtons[i] > pState->Gamepad.bAnalogButtons[i])
                pState->Gamepad.bAnalogButtons[i] = kb.Gamepad.bAnalogButtons[i];
        if (kb.Gamepad.sThumbLX) pState->Gamepad.sThumbLX = kb.Gamepad.sThumbLX;
        if (kb.Gamepad.sThumbLY) pState->Gamepad.sThumbLY = kb.Gamepad.sThumbLY;
        if (kb.Gamepad.sThumbRX) pState->Gamepad.sThumbRX = kb.Gamepad.sThumbRX;
        if (kb.Gamepad.sThumbRY) pState->Gamepad.sThumbRY = kb.Gamepad.sThumbRY;
    }

    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    XINPUT_VIBRATION xi_vib;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    xi_vib.wLeftMotorSpeed = pVibration->wLeftMotorSpeed;
    xi_vib.wRightMotorSpeed = pVibration->wRightMotorSpeed;
    {
        DWORD result = XInputSetState(dwPort, &xi_vib);
        return result == ERROR_DEVICE_NOT_CONNECTED && dwPort == 0 && keyboard_enabled()
             ? ERROR_SUCCESS : result;
    }
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    if (dwPort == 0 && keyboard_enabled()) return TRUE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    XINPUT_CAPABILITIES xi_caps;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;

    result = XInputGetCapabilities(dwPort, dwFlags, &xi_caps);
    if (result != ERROR_SUCCESS) {
        if (dwPort != 0 || !keyboard_enabled())
            return result;
        memset(pCaps, 0, sizeof(*pCaps));
        pCaps->Type = 1;
        pCaps->SubType = 1;
        pCaps->Gamepad.wButtons = 0x00FF;
        memset(pCaps->Gamepad.bAnalogButtons, 255, 8);
        pCaps->Gamepad.sThumbLX = pCaps->Gamepad.sThumbLY = 32767;
        pCaps->Gamepad.sThumbRX = pCaps->Gamepad.sThumbRY = 32767;
        return ERROR_SUCCESS;
    }

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type = xi_caps.Type;
    pCaps->SubType = xi_caps.SubType;
    pCaps->Flags = xi_caps.Flags;
    return ERROR_SUCCESS;
}

/* ======================================================================== */
#else /* !_WIN32 */
/* ====================  SDL2 GameController backend  ===================== */
/* ======================================================================== */

#include <SDL.h>

static SDL_GameController *g_pads[XBOX_MAX_CONTROLLERS];
static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS];
static DWORD g_packet[XBOX_MAX_CONTROLLERS];

/* Open up to XBOX_MAX_CONTROLLERS attached game controllers. */
static void open_controllers(void)
{
    int slot = 0;
    for (int i = 0; i < SDL_NumJoysticks() && slot < XBOX_MAX_CONTROLLERS; i++) {
        if (!SDL_IsGameController(i))
            continue;
        if (!g_pads[slot]) {
            g_pads[slot] = SDL_GameControllerOpen(i);
            g_controller_connected[slot] = (g_pads[slot] != NULL);
        }
        slot++;
    }
}

void xbox_InputInit(void)
{
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    open_controllers();
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    SDL_GameController *c = g_pads[dwPort];
    if (!c || !SDL_GameControllerGetAttached(c)) {
        g_controller_connected[dwPort] = FALSE;
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    SDL_GameControllerUpdate();
    g_controller_connected[dwPort] = TRUE;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = ++g_packet[dwPort];

    WORD btn = 0;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP))    btn |= XBOX_GAMEPAD_DPAD_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  btn |= XBOX_GAMEPAD_DPAD_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  btn |= XBOX_GAMEPAD_DPAD_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) btn |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START))      btn |= XBOX_GAMEPAD_START;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK))       btn |= XBOX_GAMEPAD_BACK;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSTICK))  btn |= XBOX_GAMEPAD_LEFT_THUMB;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) btn |= XBOX_GAMEPAD_RIGHT_THUMB;
    pState->Gamepad.wButtons = btn;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? 255 : 0;

    /* SDL trigger axes are 0..32767 -> Xbox analog button 0..255 */
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);

    /* SDL Y axis points down; the Xbox Y axis points up -- invert.
     * Use (-1 - v) so v = -32768 does not overflow SHORT. */
    pState->Gamepad.sThumbLX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    pState->Gamepad.sThumbLY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    pState->Gamepad.sThumbRX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
    pState->Gamepad.sThumbRY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));

    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    SDL_GameController *c = g_pads[dwPort];
    if (!c) return ERROR_DEVICE_NOT_CONNECTED;

    /* SDL rumble needs a duration; refresh for ~1s on each call (the game
     * polls vibration continuously). */
    SDL_GameControllerRumble(c, pVibration->wLeftMotorSpeed,
                             pVibration->wRightMotorSpeed, 1000);
    return ERROR_SUCCESS;
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    (void)dwFlags;
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!g_pads[dwPort])
        return ERROR_DEVICE_NOT_CONNECTED;

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type    = 1;   /* XINPUT_DEVTYPE_GAMEPAD */
    pCaps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
    pCaps->Flags   = 0;
    return ERROR_SUCCESS;
}

#endif /* _WIN32 */
