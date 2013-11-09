
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
	LOCKTO_MONITOR,
	LOCKTO_MONITORWORKAREA,
};
LOCKTO lockto = LOCKTO_FOREGROUNDWINDOW;

enum LOCKMETHOD {
	LOCKMETHOD_CLIPCURSOR,
	LOCKMETHOD_SETCURSORPOS,
	LOCKMETHOD_REGRESSIVE,
};
LOCKMETHOD lockmethod = LOCKMETHOD_CLIPCURSOR;

HWND lock_window;
HMONITOR lock_monitor;

void SetLockingMouse(BOOL new_value)
{
	if (is_locking_mouse != new_value)
	{
		is_locking_mouse = new_value;
		if (is_locking_mouse)
		{
			POINT cursor;
			lock_window = GetForegroundWindow();
			GetCursorPos(&cursor);
			lock_monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
		}
		if (!is_locking_mouse && lockmethod == LOCKMETHOD_CLIPCURSOR)
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
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_MONITOR), lockto == LOCKTO_MONITOR);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_WORKAREA), lockto == LOCKTO_MONITORWORKAREA);
	}
}

void SetLockMethod(LOCKMETHOD new_value)
{
	if (new_value != lockmethod)
	{
		lockmethod = new_value;
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_METHOD_CLIPCURSOR), lockmethod == LOCKMETHOD_CLIPCURSOR);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_METHOD_SETCURSORPOS), lockmethod == LOCKMETHOD_SETCURSORPOS);
		if (lockmethod != LOCKMETHOD_CLIPCURSOR)
			ClipCursor(NULL);
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

void GetLockRect(RECT *rc)
{
	switch (lockto)
	{
	case LOCKTO_FOREGROUNDWINDOW:
	{
		POINT pt = {0, 0};
		GetClientRect(lock_window, rc);

		ClientToScreen(lock_window, &pt);

		rc->left += pt.x;
		rc->top += pt.y;
		rc->right += pt.x;
		rc->bottom += pt.y;

		break;
	}
	case LOCKTO_FOREGROUNDBORDER:
		GetWindowRect(lock_window, rc);
		break;
	case LOCKTO_MONITOR:
	{
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(lock_monitor, &mi);
		*rc = mi.rcMonitor;
		break;
	}
	case LOCKTO_MONITORWORKAREA:
	{
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(lock_monitor, &mi);
		*rc = mi.rcWork;
		break;
	}
	}
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (is_locking_mouse)
	{
		LPMSLLHOOKSTRUCT hs = (LPMSLLHOOKSTRUCT)lParam;
		RECT lock_rect;
		if ((lockto == LOCKTO_FOREGROUNDBORDER || lockto == LOCKTO_FOREGROUNDWINDOW) &&
			GetForegroundWindow() != lock_window)
		{
			// Foreground window changed since we were activated. Release the lock.
			SetLockingMouse(FALSE);
			goto end;
		}

		switch (lockmethod)
		{
		case LOCKMETHOD_CLIPCURSOR:
			GetLockRect(&lock_rect);
			ClipCursor(&lock_rect);
			break;
		case LOCKMETHOD_REGRESSIVE: // NOT IN THE GUI BECAUSE IT TURNS OUT THIS IS STUPID
		{
			POINT cursor, event_delta, clip_pt;
			int needs_adjustment = 0;
			GetLockRect(&lock_rect);

			GetCursorPos(&cursor);
			event_delta.x = hs->pt.x - cursor.x;
			event_delta.y = hs->pt.y - cursor.y;
			clip_pt = hs->pt;
			if (clip_pt.x < lock_rect.left)
			{
				needs_adjustment = 1;
				clip_pt.x = lock_rect.left;
			}
			else if (clip_pt.x >= lock_rect.right)
			{
				needs_adjustment = 1;
				clip_pt.x = lock_rect.right - 1;
			}
			if (clip_pt.y < lock_rect.top)
			{
				needs_adjustment = 1;
				clip_pt.y = lock_rect.top;
			}
			else if (clip_pt.y >= lock_rect.bottom)
			{
				needs_adjustment = 1;
				clip_pt.y = lock_rect.bottom - 1;
			}
			if (needs_adjustment)
			{
				SetCursorPos(hs->pt.x, hs->pt.y); // Moves outside the window
				SetCursorPos(clip_pt.x - event_delta.x, clip_pt.y - event_delta.y);
				SetCursorPos(clip_pt.x, clip_pt.y);
				return 1;
			}
			break;
		}
		case LOCKMETHOD_SETCURSORPOS:
		{
			POINT clip_pt;
			int needs_adjustment = 0;
			GetLockRect(&lock_rect);

			clip_pt = hs->pt;
			if (clip_pt.x < lock_rect.left)
			{
				needs_adjustment = 1;
				clip_pt.x = lock_rect.left;
			}
			else if (clip_pt.x >= lock_rect.right)
			{
				needs_adjustment = 1;
				clip_pt.x = lock_rect.right - 1;
			}
			if (clip_pt.y < lock_rect.top)
			{
				needs_adjustment = 1;
				clip_pt.y = lock_rect.top;
			}
			else if (clip_pt.y >= lock_rect.bottom)
			{
				needs_adjustment = 1;
				clip_pt.y = lock_rect.bottom - 1;
			}
			if (needs_adjustment)
			{
				SetCursorPos(clip_pt.x, clip_pt.y);
				return 1;
			}
			break;
		}
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
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_MONITOR), lockto == LOCKTO_MONITOR);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_LOCKTO_WORKAREA), lockto == LOCKTO_MONITORWORKAREA);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_METHOD_CLIPCURSOR), lockmethod == LOCKMETHOD_CLIPCURSOR);
		Button_SetCheck(GetDlgItem(main_dialog, IDC_RADIO_METHOD_SETCURSORPOS), lockmethod == LOCKMETHOD_SETCURSORPOS);
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
		case IDC_RADIO_LOCKTO_MONITOR:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck((HWND)lParam))
					SetLockTo(LOCKTO_MONITOR);
				return TRUE;
			}
			break;
		case IDC_RADIO_LOCKTO_WORKAREA:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck((HWND)lParam))
					SetLockTo(LOCKTO_MONITORWORKAREA);
				return TRUE;
			}
			break;
		case IDC_RADIO_METHOD_CLIPCURSOR:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck((HWND)lParam))
					SetLockMethod(LOCKMETHOD_CLIPCURSOR);
				return TRUE;
			}
			break;
		case IDC_RADIO_METHOD_SETCURSORPOS:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck((HWND)lParam))
					SetLockMethod(LOCKMETHOD_SETCURSORPOS);
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
