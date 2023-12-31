#pragma once
#include "../lib_global.h"

LIBRARY_EXPORT std::wstring DosPathToNtPath(const std::wstring& dosPath);

LIBRARY_EXPORT std::wstring NtPathToDosPath(const std::wstring& ntPath);

LIBRARY_EXPORT sint32 GetLastWin32ErrorAsNtStatus();

LIBRARY_EXPORT uint64 FILETIME2ms(uint64 fileTime);

LIBRARY_EXPORT uint64 FILETIME2time(uint64 fileTime);

LIBRARY_EXPORT std::wstring NormalizeFilePath(std::wstring FilePath, bool bLowerCase = true);

LIBRARY_EXPORT bool SetAdminFullControl(const std::wstring& folderPath);

LIBRARY_EXPORT BOOL GetProcessUserSID(DWORD processID, PSID* userSID);
LIBRARY_EXPORT std::wstring GetProcessUserSID(DWORD processID);