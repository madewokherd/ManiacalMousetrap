
#include <Windows.h>
#include <Windowsx.h>

#include "resource.h"

BOOL is_locking_mouse = FALSE;

HHOOK keyboard_hook;
HWND main_dialog;

void SetLockingMouse(BOOL new_value)
{
	if (is_locking_mouse != new_value)
	{
		is_locking_mouse = new_value;
		if (main_dialog)
			Button_SetCheck(GetDlgItem(main_dialog, IDC_CHECKACTIVE), is_locking_mouse);
	}
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	switch (nCode)
	{
	case HC_ACTION:
	{
		LPKBDLLHOOKSTRUCT hs = LPKBDLLHOOKSTRUCT(lParam);
		if (wParam == WM_KEYDOWN && hs->vkCode == VK_SCROLL)
		{
			if ((GetKeyState(VK_SCROLL) & 0x1) == 1) // Scroll lock currently activated, will be deactivated
				SetLockingMouse(FALSE);
			else
				SetLockingMouse(TRUE);
		}
		break;
	}
	}
	return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

INT_PTR CALLBACK MainDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		main_dialog = hwndDlg;
		Button_SetCheck(GetDlgItem(main_dialog, IDC_CHECKACTIVE), is_locking_mouse);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CHECKACTIVE:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				SetLockingMouse(Button_GetCheck((HWND)lParam));
				return TRUE;
			}
			break;
		}
		break;
	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		return TRUE;
	}

	return FALSE;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOGMAIN), NULL, MainDialogProc);

	return 0;
}
