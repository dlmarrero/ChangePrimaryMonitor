#include <Windows.h>
#include <stdio.h>

#define MAX_NAME_LEN 32

enum Mode {
	Game,
	Work
};

BOOL GetDisplayNames(LPSTR, LPSTR);
BOOL GetDeviceModes(LPCSTR, PDEVMODE, LPCSTR, PDEVMODE);
BOOL ChangeSettings(enum Mode, LPCSTR, PDEVMODE, LPCSTR, PDEVMODE);
LPCSTR GetDisplayError(LONG error);

int __cdecl main(int argc, char** argv)
{
	enum Mode mode;

	CHAR primaryDisplay[MAX_NAME_LEN] = { 0 };
	CHAR secondaryDisplay[MAX_NAME_LEN] = { 0 };
	DEVMODE primaryMode = { 0 };
	DEVMODE secondaryMode = { 0 };

	if (argc < 2) {
		fprintf(stderr, "USAGE: %s [game | work]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Get desired mode
	if (_stricmp(argv[1], "game") == 0) {
		mode = Game;
	}
	else if (_stricmp(argv[1], "work") == 0) {
		mode = Work;
	}
	else {
		fprintf(stderr, "Invalid mode: %s\n\nUSAGE: %s [game | work]\n", argv[1], argv[0]);
		exit(EXIT_FAILURE);
	}

	// Get primary and secondary device names
	if (!GetDisplayNames(primaryDisplay, secondaryDisplay)) {
		fprintf(stderr, "Failed to get device names\n");
		exit(EXIT_FAILURE);
	}

	// Populate device mode structures
	if (!GetDeviceModes(primaryDisplay, &primaryMode, secondaryDisplay, &secondaryMode)) {
		exit(EXIT_FAILURE);
	}

	// Change settings based on the mode
	if (!ChangeSettings(mode, primaryDisplay, &primaryMode, secondaryDisplay, &secondaryMode)) {
		exit(EXIT_FAILURE);
	}

	puts("OK");
	return EXIT_SUCCESS;
}

BOOL GetDisplayNames(LPSTR primaryDisplay, LPSTR secondaryDisplay)
{
	DWORD iDevNum = 0;
	DISPLAY_DEVICE dev = { 0 };
	dev.cb = sizeof(dev);

	// Popilate display device names
	while (TRUE) {
		if (!EnumDisplayDevices(NULL, iDevNum, &dev, EDD_GET_DEVICE_INTERFACE_NAME)) {
			break;
		}

		// Get names of primary and secondary devices attached to desktop
		if (dev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
			if (dev.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
				strcpy_s(primaryDisplay, MAX_NAME_LEN, dev.DeviceName);
			}
			else {
				strcpy_s(secondaryDisplay, MAX_NAME_LEN, dev.DeviceName);
			}
		}

		iDevNum++;
	}

	if (primaryDisplay[0] == '\0' || secondaryDisplay[0] == '\0') {
		return FALSE;
	}

	return TRUE;
}

BOOL GetDeviceModes(LPCSTR primaryDisplay, PDEVMODE primaryMode,
					LPCSTR secondaryDisplay, PDEVMODE secondaryMode)
{
	// Initialize struct sizes
	primaryMode->dmSize = sizeof(DEVMODE);
	secondaryMode->dmSize = sizeof(DEVMODE);

	// Get primary device mode
	if (!EnumDisplaySettingsA(primaryDisplay, ENUM_CURRENT_SETTINGS, primaryMode)) {
		fprintf(stderr, "Failed to enumerate primary display mode\n");
		return FALSE;
	}

	// Get secondary device mode
	if (!EnumDisplaySettingsA(secondaryDisplay, ENUM_CURRENT_SETTINGS, secondaryMode)) {
		fprintf(stderr, "Failed to enumerate secondary display mode\n");
		return FALSE;
	}

	return TRUE;
}

BOOL ChangeSettings(enum Mode mode, LPCSTR primaryDisplay, PDEVMODE primaryMode,
					LPCSTR secondaryDisplay, PDEVMODE secondaryMode)
{
	LONG ret = 0;
	DWORD hpFlags = 0;
	DWORD dellFlags = 0;
	PDEVMODE pDellMode = NULL;
	LPCSTR pDellName = NULL;
	PDEVMODE pHPMode = NULL;
	LPCSTR pHPName = NULL;

	// Find which display is the HP by checking the pixel width (Dell is always 2560)
	if (primaryMode->dmPelsWidth != 2560) {
		// HP is the primary
		pHPName = primaryDisplay;
		pHPMode = primaryMode;
		pDellName = secondaryDisplay;
		pDellMode = secondaryMode;
	}
	else {
		// HP is the secondary
		pHPName = secondaryDisplay;
		pHPMode = secondaryMode;
		pDellName = primaryDisplay;
		pDellMode = primaryMode;
	}

	if (Work == mode) {
		// Set the HP to portrait and set Dell as primary
		pHPMode->dmPelsWidth = 1080;
		pHPMode->dmPelsHeight = 1920;
		pHPMode->dmPosition.x = 2560;
		pHPMode->dmPosition.y = -263;
		pHPMode->dmDisplayOrientation = DMDO_90;

		pDellMode->dmPosition.x = 0;
		pDellMode->dmPosition.y = 0;

		hpFlags = CDS_UPDATEREGISTRY | CDS_NORESET;
		dellFlags = CDS_SET_PRIMARY | CDS_UPDATEREGISTRY | CDS_NORESET;
	}
	else if (Game == mode) {
		// Set the HP to landscape and set HP as primary
		pHPMode->dmPelsWidth = 1920;
		pHPMode->dmPelsHeight = 1080;
		pHPMode->dmPosition.x = 0;
		pHPMode->dmPosition.y = 0;
		pHPMode->dmDisplayOrientation = DMDO_DEFAULT;

		pDellMode->dmPosition.x = -2560;
		pDellMode->dmPosition.y = -155;

		hpFlags = CDS_SET_PRIMARY | CDS_UPDATEREGISTRY | CDS_NORESET;
		dellFlags = CDS_UPDATEREGISTRY | CDS_NORESET;
	}

	// Set field flags to indicate what we're changing
	pHPMode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYORIENTATION | DM_POSITION;
	pDellMode->dmFields = DM_POSITION;

	// Stage changes to the HP
	ret = ChangeDisplaySettingsExA(pHPName, pHPMode, NULL, hpFlags, NULL);
	if (DISP_CHANGE_SUCCESSFUL != ret) {
		fprintf(stderr, "Failed to stage HP display settings: %s\n", GetDisplayError(ret));
		return FALSE;
	}

	// Stage changes to the Dell
	ret = ChangeDisplaySettingsExA(pDellName, pDellMode, NULL, dellFlags, NULL);
	if (DISP_CHANGE_SUCCESSFUL != ret) {
		fprintf(stderr, "Failed to stage Dell display settings: %s\n", GetDisplayError(ret));
		return FALSE;
	}

	// Apply the changes
	ret = ChangeDisplaySettingsExA(NULL, NULL, NULL, 0, NULL);
	if (DISP_CHANGE_SUCCESSFUL != ret) {
		fprintf(stderr, "Failed to apply display settings: %s\n", GetDisplayError(ret));
		return FALSE;
	}

	return TRUE;
}

LPCSTR GetDisplayError(LONG error)
{
	LPCSTR msg = NULL;

	switch (error) {
	case DISP_CHANGE_BADFLAGS:
		msg = "Invalid flags";
		break;
	case DISP_CHANGE_BADMODE:
		msg = "Graphics mode not supported";
		break;
	case DISP_CHANGE_BADPARAM:
		msg = "Invalid parameter(s)";
		break;
	case DISP_CHANGE_FAILED:
		msg = "Display driver failed specified mode";
		break;
	case DISP_CHANGE_NOTUPDATED:
		msg = "Unable to write settings to registry";
		break;
	default:
		msg = "Unknown error";
	}

	return msg;
}
