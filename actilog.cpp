/// actilog - sums up mouse movements (in pixels), counts nClicks and
///           key presses, writes statistics to standard output.
///
///    Copyright (C) 2013 Oliver Lau <ola@ct.de>
///
///    This program is free software: you can redistribute it and/or modify
///    it under the terms of the GNU General Public License as published by
///    the Free Software Foundation, either version 3 of the License, or
///    (at your option) any later version.
///
///    This program is distributed in the hope that it will be useful,
///    but WITHOUT ANY WARRANTY; without even the implied warranty of
///    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///    GNU General Public License for more details.
///
///    You should have received a copy of the GNU General Public License
///    along with this program.  If not, see <http://www.gnu.org/licenses/>.
///

#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "getopt/getopt.h"
#include "log.h"
#include "util.h"

static const UINT DefaultTimerInterval = 10;

enum _long_options {
	SELECT_HELP = 0x1,
	SELECT_MOVE_INTERVAL,
	SELECT_OUTPUT_FILE,
	SELECT_OVERWRITE
};

static struct option long_options[] = {
	{ "output",        required_argument, 0, SELECT_OUTPUT_FILE },
	{ "move-interval", required_argument, 0, SELECT_MOVE_INTERVAL },
	{ "help",          no_argument, 0, SELECT_HELP },
	{ "overwrite",     no_argument, 0, SELECT_OVERWRITE },
	{ NULL,            0, 0, 0 }
};


Logger logger;
POINT ptLastMousePos = { LONG_MAX, LONG_MAX };
float fMouseDist = 0;
int nClicks = 0;
int nWheel = 0;
bool bVerbose = false;
bool bOverwrite = false;
int aHisto[256];
int aLastHisto[256];


bool hasHistoChanged()
{
	for (int i = 0; i < 256; ++i)
		if (aHisto[i] != aLastHisto[i])
			return true;
	return false;
}


void clearHistogram()
{
	for (int i = 0; i < 256; ++i) {
		aLastHisto[i] = aHisto[i];
		aHisto[i] = 0;
	}
}


LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;
	switch (wParam)
	{
	case WM_MOUSEMOVE:
		if (ptLastMousePos.x < LONG_MAX && ptLastMousePos.y < LONG_MAX)
			fMouseDist += sqrt((float)squared(ptLastMousePos.x - pMouse->pt.x) + (float)squared(ptLastMousePos.y - pMouse->pt.y));
		ptLastMousePos = pMouse->pt;
		break;
	case WM_MOUSEWHEEL:
		++nWheel;
		break;
	case WM_LBUTTONUP:
		// fall-through
	case WM_RBUTTONUP:
		++nClicks;
		break;
	}
	return 0;
}


LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
	switch (wParam)
	{
	case WM_KEYUP:
		{
			if (pKeyBoard->vkCode >= 0 && pKeyBoard->vkCode < 256)
				++aHisto[pKeyBoard->vkCode];
			break;
		}
	default:
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}
	return 0;
}


void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	if (fMouseDist > 0) {
		logger.logWithTimestamp("MOVE %f", fMouseDist);
		fMouseDist = 0;
	}
	if (nWheel > 0) {
		logger.logWithTimestamp("WHEEL %d", nWheel);
		nWheel = 0;
	}
	if (nClicks > 0) {
		logger.logWithTimestamp("CLICK %d", nClicks);
		nClicks = 0;
	}
	if (hasHistoChanged()) {
		logger.logWithTimestampNoLF("KEYSTAT ");
		for (int i = 0; i < 256; ++i) {
			logger.log("%d", aHisto[i]);
			if (i < 255)
				logger.log(",");
		}
		logger.flush();
		clearHistogram();
	}
}


void disclaimer()
{
	printf("\n\n\n"
		" Copyright (c) 2013 Oliver Lau <ola@ct.de>\n"
		"\n"
		" This program is free software: you can redistribute it and/or modify\n"
		" it under the terms of the GNU General Public License as published by\n"
		" the Free Software Foundation, either version 3 of the License, or\n"
		" (at your option) any later version.\n"
		"\n"
		" This program is distributed in the hope that it will be useful,\n"
		" but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		" GNU General Public License for more details.\n"
		"\n"
		" You should have received a copy of the GNU General Public License\n"
		" along with this program.  If not, see http://www.gnu.org/licenses/ \n\n");
}


void usage()
{
	printf("actilog - sums up mouse movements (in pixels), counts nClicks and\n"
		"key presses, writes statistics to standard output.\n"
		"\n"
		"Usage: actilog [options]\n"
		"\n"
		"  -o file\n"
		"  --output file\n"
		"     write to 'file' instead of console\n"
		"  --overwrite\n"
		"     do not append to file\n"
		"  -i interval\n"
		"     sum up mouse movements every 'interval' seconds\n"
		"     (default: %d seconds)\n"
		"  -h\n"
		"  -?\n"
		"  --help\n"
		"     show this help (and license information)\n"
		"\n",
		DefaultTimerInterval);
}


int main(int argc, TCHAR* argv[])
{
	UINT uTimerInterval = DefaultTimerInterval;
	for (;;) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "h?i:vo:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c)
		{
		case 'v':
			bVerbose = true;
			break;
		case '?':
			// fall-through
		case 'h':
			usage();
			disclaimer();
			return EXIT_SUCCESS;
		case SELECT_OVERWRITE:
			bOverwrite = true;
			break;
		case 'o':
			// fall-through
		case SELECT_OUTPUT_FILE:
			if (optarg == NULL) {
				usage();
				return EXIT_FAILURE;
			}
			logger.setFilename(optarg);
			break;
		case 'i':
			// fall-through
		case SELECT_MOVE_INTERVAL:
			if (optarg == NULL) {
				usage();
				return EXIT_FAILURE;
			}
			uTimerInterval = atoi(optarg);
			break;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}
	SecureZeroMemory(aHisto, 256*sizeof(aHisto[0]));
	SecureZeroMemory(aLastHisto, 256*sizeof(aLastHisto[0]));
	bool success = logger.open(bOverwrite);
	if (!success) {
		fprintf(stderr, "cannot create file '%s'\n", logger.filename());
		return EXIT_FAILURE;
	}
	if (bVerbose)
		logger.logWithTimestamp("START interval = %d secs", uTimerInterval);
	HINSTANCE hApp = GetModuleHandle(NULL);
	HHOOK hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hApp, 0);
	HHOOK hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hApp, 0);
	UINT_PTR uIDTimer = SetTimer(NULL, 0, 1000 * uTimerInterval, TimerProc);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	KillTimer(NULL, uIDTimer);
	UnhookWindowsHookEx(hMouseHook);
	UnhookWindowsHookEx(hKeyboardHook);
	if (bVerbose)
		logger.logWithTimestamp("STOP");
	logger.close();
	return EXIT_SUCCESS;
}
