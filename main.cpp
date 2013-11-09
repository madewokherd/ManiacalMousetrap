
#include <Windows.h>
#include <Windowsx.h>

#include "resource.h"

BOOL is_locking_mouse = FALSE;

HHOOK keyboard_hook;
HHOOK mouse_hook;
HWND main_dialog;

enum LOCKTO {
	LOCKTO_FOREGROUNDWINDOW,
	LOCKTO_FOREGROUNDBORDER,
};
LOCKTO lockto = LOCKTO_FOREGROUNDWINDOW;

enum LOCKMETHOD {
	LOCKMETHOD_CLIPCURSOR,
};
LOCKMETHOD lockmethod = LOCKMETHOD_CLIPCURSOR;

HWND lock_window;

void SetLockingMouse(BOOL new_value)
{
	if (is_locking_mouse != new_value)
	{
		is_locking_mouse = new_value;
		if (new_value && (lockto == LOCKTO_FOREGROUNDWINDOW || lockto == LOCKTO_FOREGROUNDBORDER))
			lock_window = GetForegroundWindow();
		if (!new_value && lockmethod == LOCKMETHOD_CLIPCURSOR)
			ClipCursor(NULL);
		if (main_dialog)
			Button_SetCheck(GetDlgItem(main_dialog, IDC_CHECKACTIVE), is_locking_mouse);
	}
}

void SetLockTo(LOCKTO new_value)
{
	if (new_value != lockto)
	{
		lockto = new_value;
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_FOREGROUND), lockto == LOCKTO_FOREGROUNDWINDOW);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_FOREGROUNDBORDER), lockto == LOCKTO_FOREGROUNDBORDER);
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

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (is_locking_mouse)
	{
		RECT lock_rect;

		switch (lockto)
		{
		case LOCKTO_FOREGROUNDWINDOW:
		{
			POINT pt = {0, 0};
			if (GetForegroundWindow() != lock_window)
			{
				// Foreground window changed since we were activated. Release the lock.
				SetLockingMouse(FALSE);
				goto end;
			}
			GetClientRect(lock_window, &lock_rect);

			ClientToScreen(lock_window, &pt);

			lock_rect.left += pt.x;
			lock_rect.top += pt.y;
			lock_rect.right += pt.x;
			lock_rect.bottom += pt.y;

			break;
		}
		case LOCKTO_FOREGROUNDBORDER:
			if (GetForegroundWindow() != lock_window)
			{
				// Foreground window changed since we were activated. Release the lock.
				SetLockingMouse(FALSE);
				goto end;
			}
			GetWindowRect(lock_window, &lock_rect);

			break;
		}

		switch (lockmethod)
		{
		case LOCKMETHOD_CLIPCURSOR:
			ClipCursor(&lock_rect);
			break;
		}
	}

end:
	return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
}

INT_PTR CALLBACK MainDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		main_dialog = hwndDlg;
		Button_SetCheck(GetDlgItem(main_dialog, IDC_CHECKACTIVE), is_locking_mouse);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_FOREGROUND), lockto == LOCKTO_FOREGROUNDWINDOW);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_FOREGROUNDBORDER), lockto == LOCKTO_FOREGROUNDBORDER);
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
		case IDC_RADIO_LOCKTO_FOREGROUND:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck((HWND)lParam))
					SetLockTo(LOCKTO_FOREGROUNDWINDOW);
				return TRUE;
			}
			break;
		case IDC_RADIO_LOCKTO_FOREGROUNDBORDER:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck((HWND)lParam))
					SetLockTo(LOCKTO_FOREGROUNDBORDER);
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

	mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOGMAIN), NULL, MainDialogProc);

	return 0;
}
