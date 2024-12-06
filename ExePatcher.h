#pragma once
#include "framework.h"
#include <string>

struct PingValueRange {
	int min;
	int max;
};

struct PingValues {
	PingValueRange ranges[5];
	inline PingValueRange& operator[](int index) { return ranges[index]; }
};

bool obtainValuesFromExe(const std::wstring& szFile, PingValues* outValues, std::wstring& importantRemarks, HWND mainWindow);

/// <summary>
/// Contains hardcode for patching GuiltyGearXrd.exe specifically to do this mirror color select patch.
/// </summary>
/// <param name="szFile">The path to GuiltyGearXrd.exe.</param>
/// <param name="importantRemarks">A string containing "\n" delimited lines of info meant to be shown to the user after completion.
/// Each line is a separate remark.</param>
/// <param name="mainWindow">The window used for displaying message boxes when interaction with the user is required.</param>
/// <param name="mayCreateBackup">true if the function is allowed to perform backup copies of the .exe in case it is not yet patched
/// and a patch application is due.</param>
/// <param name="newPings">an array of 5 integers representing the ping values for T0, T1, T2, T3 and T4 respectively.</param>
/// <returns>true on successful patch or if patch is already in place, false on error or cancellation.</returns>
bool performExePatching(const std::wstring& szFile, std::wstring& importantRemarks, HWND mainWindow, bool mayCreateBackup, int* newPings);
