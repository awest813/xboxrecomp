/* Standalone Windows regression test with a deterministic physical-pad backend. */
#include "xinput_xbox.h"
#include <xinput.h>
#include <stdio.h>
#include <string.h>

static int pad_connected;
static XINPUT_STATE pad_state;
static DWORD WINAPI mock_get_state(DWORD port, XINPUT_STATE *out)
{
    if (port || !pad_connected) return ERROR_DEVICE_NOT_CONNECTED;
    *out = pad_state;
    return ERROR_SUCCESS;
}
static DWORD WINAPI mock_set_state(DWORD port, XINPUT_VIBRATION *vibration)
{
    (void)vibration;
    return port || !pad_connected ? ERROR_DEVICE_NOT_CONNECTED : ERROR_SUCCESS;
}
static DWORD WINAPI mock_get_caps(DWORD port, DWORD flags, XINPUT_CAPABILITIES *out)
{
    (void)flags;
    if (port || !pad_connected) return ERROR_DEVICE_NOT_CONNECTED;
    memset(out, 0, sizeof(*out));
    out->Type = out->SubType = 1;
    return ERROR_SUCCESS;
}

/* Exercise the real implementation without needing a connected controller. */
#define XInputGetState mock_get_state
#define XInputSetState mock_set_state
#define XInputGetCapabilities mock_get_caps
#include "xinput_device.c"
#undef XInputGetState
#undef XInputSetState
#undef XInputGetCapabilities

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #c); return 1; } } while (0)

int main(void)
{
    XBOX_INPUT_STATE state, before;
    XBOX_INPUT_CAPABILITIES caps;
    XBOX_VIBRATION vibration = { 100, 200 };

    _putenv_s("RECOMP_KEYBOARD", "0");
    xbox_InputSetKeyState('Z', 1);
    CHECK(xbox_InputGetState(0, &state) == ERROR_DEVICE_NOT_CONNECTED);
    CHECK(!xbox_InputIsConnected(0));
    xbox_InputClearKeys();

    _putenv_s("RECOMP_KEYBOARD", "1");
    CHECK(xbox_InputIsConnected(0));
    CHECK(!xbox_InputIsConnected(1));
    CHECK(xbox_InputGetState(1, &state) == ERROR_DEVICE_NOT_CONNECTED);
    CHECK(xbox_InputGetState(0, NULL) == ERROR_DEVICE_NOT_CONNECTED);
    CHECK(xbox_InputGetState(0, &before) == ERROR_SUCCESS);
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber == before.dwPacketNumber);

    xbox_InputSetKeyState('Z', 1);
    xbox_InputSetKeyState(VK_RETURN, 1);
    xbox_InputSetKeyState(VK_NUMPAD6, 1);
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.Gamepad.wButtons & XBOX_GAMEPAD_START);
    CHECK(state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    CHECK(state.Gamepad.sThumbLX == 32767);
    CHECK(state.dwPacketNumber != before.dwPacketNumber);
    before = state;
    xbox_InputSetKeyState('Z', 1); /* key repeat is not another state edge */
    xbox_InputSetKeyState(256, 1); /* invalid key cannot affect state */
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber == before.dwPacketNumber);
    xbox_InputSetKeyState(VK_NUMPAD4, 1);
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.Gamepad.sThumbLX == 0);

    xbox_InputClearKeys(); /* focus loss releases every control */
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.Gamepad.wButtons == 0);
    CHECK(state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    CHECK(state.Gamepad.sThumbLX == 0);
    CHECK(xbox_InputGetCapabilities(0, 0, &caps) == ERROR_SUCCESS);
    CHECK(caps.Type == 1 && caps.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    CHECK(xbox_InputSetState(0, &vibration) == ERROR_SUCCESS);

    pad_connected = 1;
    pad_state.dwPacketNumber = 10;
    pad_state.Gamepad.wButtons = XINPUT_GAMEPAD_B;
    pad_state.Gamepad.sThumbLY = 1234;
    xbox_InputSetKeyState('Z', 1);
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    CHECK(state.Gamepad.bAnalogButtons[XBOX_BUTTON_B] == 255);
    CHECK(state.Gamepad.sThumbLY == 1234);
    before = state;
    pad_state.dwPacketNumber++;
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber == before.dwPacketNumber + 1);
    before = state;
    xbox_InputSetKeyState('Z', 0);
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber == before.dwPacketNumber + 1);
    CHECK(state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    CHECK(state.Gamepad.bAnalogButtons[XBOX_BUTTON_B] == 255);

    _putenv_s("RECOMP_KEYBOARD", "0");
    CHECK(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber == pad_state.dwPacketNumber);
    puts("keyboard fallback, packet edges, focus release and physical-pad merge passed");
    return 0;
}
