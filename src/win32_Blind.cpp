#include "Types.h"

global_var const u32 WindowWidth = 1280;
global_var const u32 WindowHeight = 960;
global_var b8 IsRunning = true;

#include "Blind.cpp"

#define UNICODE
#define _UNICODE
#include <Windows.h>
#include "win32_Direct3d.cpp"
#include <time.h>
#include <stdlib.h>

#pragma comment(lib, "user32.lib")

struct win32_keyboard_state {
	class_const u32 KeyCount = 256;
	u8 keys[KeyCount];
};

f32 RandomFloat(f32 Min, f32 Max) {
	// @TODO We could just swap Min and Max when Min is greater than Max
	Assert(Min < Max);
	local_persist b8 Init = false;
	if (!Init) {
		Init = true;
		srand(time(0));
	}

	f32 Proportion = ((f32)rand())/((f32)RAND_MAX);
	f32 Result = Min + Proportion * (Max - Min);
	return Result;
};

LRESULT MyWindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;

	switch (Message) {

	case WM_CLOSE:
	case WM_DESTROY: {
		IsRunning = false;
	} break;

	default: {
		Result = DefWindowProc(Window, Message, WParam, LParam);
	} break;
		
	}

	return Result;
}

int WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCommand) {
	{
		WNDCLASS WindowClass = {
			CS_VREDRAW | CS_HREDRAW,
			MyWindowProc,
			0,
			0,
			Instance,
			0,
			0,
			0,
			0,
			L"MyClass",
		};
		RegisterClass(&WindowClass);
	}

	HWND Window = CreateWindowEx(0, L"MyClass", L"Suppose", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth + 16, WindowHeight + 39, 0, 0, Instance, 0);
	
	ShowWindow(Window, ShowCommand);
	
	Render_Init(Window);
	GameState.CurrentLevel = 0;
	GameState.Init = false;

	input_state InputState = {};
	win32_keyboard_state Keyboard = {};

	MSG Message = {};
	while (IsRunning) {
		while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
			DispatchMessage(&Message);
		}

		// keyboard and mouse input
		if (!GetKeyboardState(Keyboard.keys)) {
			Assert(false);
		}
		// convert keyboard into controller
		InputState.Current.Up = Keyboard.keys[VK_UP] & 0x80;
		InputState.Current.Down = Keyboard.keys[VK_DOWN] & 0x80;
		InputState.Current.Left = Keyboard.keys[VK_LEFT] & 0x80;
		InputState.Current.Right = Keyboard.keys[VK_RIGHT] & 0x80;
		InputState.Current.A = Keyboard.keys['Z'] & 0x80;
		InputState.Current.B = Keyboard.keys['X'] & 0x80;
		InputState.Current.X = Keyboard.keys['A'] & 0x80;
		InputState.Current.Y = Keyboard.keys['S'] & 0x80;

		InputState.Mouse.LeftClick = Keyboard.keys[VK_LBUTTON] & 0x80;
		InputState.Mouse.RightClick = Keyboard.keys[VK_RBUTTON] & 0x80;

		{
			POINT MousePos;
			if (!GetCursorPos(&MousePos)) {
				Assert(false);
			}

			if (!ScreenToClient(Window, &MousePos)) {
				Assert(false);
			}

#if 0 
			if (InputState.Mouse.LeftClick)
				__debugbreak();
#endif
			InputState.Mouse.Position = {MousePos.x, MousePos.y};
		}

		// @TODO(Michael) Actually calculate DeltaTime.
		// Direct3d11 gives us v-sync by default, so we should be around 60 frames per second, so assuming deltatime is 0.016 isn't the worst thing in the world.
		// This shouldn't be something that we're assuming though!
		const f32 DeltaTime = 0.016f;
		BlindSimulateAndRender(DeltaTime, InputState);

		memcpy(&InputState.Prevous, &InputState.Current, sizeof(InputState.Prevous));
	}	
}
