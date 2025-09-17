#include "pch.h"
#include <ph.h>
#include "NtUtil.h"
#include "AppUtil.h"
#include "../Common/Strings.h"
//#include "Reparse.h"

#include <Aclapi.h>
#include <sddl.h>
#include <hndlinfo.h>
#include "../Library/Helpers/NtPathMgr.h"

/*std::wstring DosPathToNtPath(const std::wstring &dosPath, bool bAsIsOnError) 
{
#if 1
    //std::wstring path = CPathReparse::Instance()->TranslateDosToNtPath(dosPath);
    //std::wstring temp = CPathReparse::Instance()->TranslateTempLinks(path, false);
	//return temp.empty() ? path : temp;
	std::wstring ntPath = CNtPathMgr::Instance()->TranslateDosToNtPath(dosPath);
	if (ntPath.empty() && bAsIsOnError)
		return dosPath;
	return ntPath;
#else
    if (dosPath.size() < 2 || dosPath[1] != ':')
        return bAsIsOnError ? dosPath : L""; // not a DOS path

    // Extract drive letter
    wchar_t driveLetter[3] = { dosPath[0], ':', '\0' };

    wchar_t ntDeviceName[MAX_PATH];
    if (QueryDosDeviceW(driveLetter, ntDeviceName, MAX_PATH) == 0)
        return bAsIsOnError ? dosPath : L""; // Failed to query DOS device

    // TODO: !!! make this work for folder mounted mounts !!!

    // Replace drive letter with NT device name
    std::wstring ntPath = ntDeviceName + dosPath.substr(2);
    return ntPath;
#endif
}

std::wstring NtPathToDosPath(const std::wstring& ntPath, bool bAsIsOnError) 
{
#if 1
    //std::wstring temp = CPathReparse::Instance()->TranslateTempLinks(ntPath, true);
    //return CPathReparse::Instance()->TranslateNtToDosPath(temp.empty() ? ntPath : temp, bAsIsOnError);
    std::wstring dosPath = CNtPathMgr::Instance()->TranslateNtToDosPath(ntPath);
	if (dosPath.empty() && bAsIsOnError)
		return ntPath;
	return dosPath;
#else
    std::vector<wchar_t> driveStrings(256);
    DWORD driveStringLength = GetLogicalDriveStringsW((DWORD)driveStrings.size() - 1, &driveStrings[0]);
    if (driveStringLength > 0 && driveStringLength < driveStrings.size()) {
        wchar_t* drive = &driveStrings[0];
        while (*drive) 
        {
            std::wstring driveRoot(drive);
            driveRoot.pop_back();

            std::vector<wchar_t> ntBuffer(256);
            if (QueryDosDeviceW(driveRoot.c_str(), &ntBuffer[0], (DWORD)ntBuffer.size())) {
                std::wstring ntDevicePath(&ntBuffer[0]);
                if (_wcsnicmp(ntDevicePath.c_str(), ntPath.c_str(), ntDevicePath.length()) == 0) {
                    std::wstring dosPath = driveRoot + ntPath.substr(ntDevicePath.length());
                    return dosPath;
                }
            }
            drive += wcslen(drive) + 1;
        }
    }
    return bAsIsOnError ? ntPath : L"";
#endif
}

std::wstring NormalizeFilePath(std::wstring FilePath, bool bLowerCase)
{
	if (FilePath.find(L'%') != std::wstring::npos)
		FilePath = ExpandEnvironmentVariablesInPath(FilePath);

    if (_wcsnicmp(FilePath.c_str(), L"\\SystemRoot\\", 12) == 0) {
        WCHAR windir[MAX_PATH + 8];
        GetWindowsDirectoryW(windir, MAX_PATH);
        FilePath = windir + FilePath.substr(11);
    }

	if (FilePath.size() > 3 && FilePath[1] == L':' && FilePath[2] == L'\\') // X:\...
		FilePath = DosPathToNtPath(FilePath);

	else if (FilePath.size() > 7 && FilePath.substr(0, 4) == L"\\??\\" && FilePath[5] == L':' && FilePath[6] == L'\\') // \??\X:\...
		FilePath = DosPathToNtPath(FilePath.substr(4));
    else if (FilePath.size() > 7 && FilePath.substr(0, 4) == L"\\\\?\\" && FilePath[5] == L':' && FilePath[6] == L'\\') // \\?\X:\...
        FilePath = DosPathToNtPath(FilePath.substr(4));

	// todo: else if (FilePath.substr(0, 11) == L"\\??\\Volume{")

    //if(!FilePath.empty() && FilePath[0] != L'*')
    //    FilePath = GetReparsedPath(FilePath);

return bLowerCase ? MkLower(FilePath) : FilePath; 
}*/

sint32 GetLastWin32ErrorAsNtStatus()
{
    return PhGetLastWin32ErrorAsNtStatus();
}

// MSDN: FILETIME Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).

uint64 FILETIME2ms(uint64 fileTime)
{
	if (fileTime < 116444736000000000ULL)
		return 0;
	return (fileTime - 116444736000000000ULL) / 10000ULL;
}

uint64 FILETIME2time(uint64 fileTime)
{
	return FILETIME2ms(fileTime) / 1000ULL;
}

uint64 GetCurrentTimeAsFileTime()
{
    FILETIME ft;
    ULARGE_INTEGER ui;
    GetSystemTimeAsFileTime(&ft);
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    return ui.QuadPart;
}

bool SetAdminFullControl(const std::wstring& folderPath) 
{
    SECURITY_DESCRIPTOR sd;
    PSID pSID = NULL;
    PACL pACL = NULL;
    bool result = false;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    EXPLICIT_ACCESS ea;

    // Initialize Security Descriptor and Explicit Access structure
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));

    // Create a well-known SID for the Administrators group
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSID)) {
    //if (AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pSID)) {
        // Initialize an EXPLICIT_ACCESS structure for an ACE
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        ea.Trustee.ptstrName = (LPWSTR)pSID;

        // Create a new ACL that contains the new ACEs
        if (SetEntriesInAcl(1, &ea, NULL, &pACL) == ERROR_SUCCESS) {
            if (SetSecurityDescriptorDacl(&sd, TRUE, pACL, FALSE)) {
                // Apply the security descriptor to the folder
                result = (SetFileSecurity(folderPath.c_str(), DACL_SECURITY_INFORMATION, &sd) != 0);
            }
        }
    }

    // Cleanup
    if (pSID) FreeSid(pSID);
    if (pACL) LocalFree(pACL);

    return result;
}

bool SetAdminFullControlAllowUsersRead(const std::wstring& folderPath)
{
    bool result = false;
    PACL pACL = nullptr;
    EXPLICIT_ACCESS ea[2];
    SECURITY_DESCRIPTOR sd;
    BYTE adminSidBuffer[SECURITY_MAX_SID_SIZE];
    BYTE authSidBuffer[SECURITY_MAX_SID_SIZE];
    DWORD cbAdminSid = sizeof(adminSidBuffer);
    DWORD cbAuthSid = sizeof(authSidBuffer);

    // Create well-known SIDs
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminSidBuffer, &cbAdminSid)) {
        return false;
    }
    if (!CreateWellKnownSid(WinAuthenticatedUserSid, nullptr, authSidBuffer, &cbAuthSid)) {
        return false;
    }

    // Initialize security descriptor
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
        return false;
    }

    ZeroMemory(&ea, sizeof(ea));

    // 1) Administrators: full control
    ea[0].grfAccessPermissions = GENERIC_ALL;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; // inherit to subfolders/files
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea[0].Trustee.ptstrName = (LPWSTR)adminSidBuffer;

    // 2) Authenticated Users: read & execute (allows normal users to list/read)
    ea[1].grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
    ea[1].grfAccessMode = SET_ACCESS;
    ea[1].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea[1].Trustee.ptstrName = (LPWSTR)authSidBuffer;

    // Build ACL from the two EXPLICIT_ACCESS entries
    if (SetEntriesInAcl(2, ea, nullptr, &pACL) != ERROR_SUCCESS) {
        goto cleanup;
    }

    // Attach ACL to the security descriptor
    if (!SetSecurityDescriptorDacl(&sd, TRUE, pACL, FALSE)) {
        goto cleanup;
    }

    // Apply to folder (DACL_SECURITY_INFORMATION)
    if (SetFileSecurity(folderPath.c_str(), DACL_SECURITY_INFORMATION, &sd) == 0) {
        goto cleanup;
    }

    result = true;

cleanup:
    if (pACL) LocalFree(pACL);
    return result;
}

BOOL GetProcessUserSID(DWORD processID, PSID *userSID) 
{
    HANDLE processHandle = NULL;
    HANDLE tokenHandle = NULL;
    PTOKEN_USER tokenUser = NULL;
    DWORD tokenSize = 0;
    BOOL result = FALSE;

    // Open the process
    processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processID);
    if (processHandle != NULL) {
        // Get the process token
        if (OpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle)) {
            // Get the user SID
            GetTokenInformation(tokenHandle, TokenUser, NULL, 0, &tokenSize);
            tokenUser = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), 0, tokenSize);
            if (GetTokenInformation(tokenHandle, TokenUser, tokenUser, tokenSize, &tokenSize)) {
                DWORD sidSize = GetLengthSid(tokenUser->User.Sid);
                *userSID = (PSID)HeapAlloc(GetProcessHeap(), 0, sidSize);
                if (CopySid(sidSize, *userSID, tokenUser->User.Sid)) {
                    result = TRUE;
                }
            }
            HeapFree(GetProcessHeap(), 0, tokenUser);
            CloseHandle(tokenHandle);
        }
        CloseHandle(processHandle);
    }

    return result;
}

std::wstring GetProcessUserSID(DWORD processID)
{
    std::wstring sid;
    PSID userSID = NULL;
    if (GetProcessUserSID(processID, &userSID)) 
    {
        LPTSTR sidString = NULL;
        ConvertSidToStringSid(userSID, &sidString);
        sid = sidString;
        LocalFree(sidString);
        HeapFree(GetProcessHeap(), 0, userSID);
    }
    return sid;
}

uint32 GetNtObjectTypeNumber(const wchar_t* name)
{
    PH_STRINGREF fileTypeName;
    PhInitializeStringRef(&fileTypeName, (wchar_t*)name);
    return PhGetObjectTypeNumber(&fileTypeName);
}

std::wstring PHStr2WStr(PPH_STRING phStr, bool bFree = false);

std::wstring GetHandleObjectName(HANDLE hProcess, PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle)
{
    PPH_STRING TypeName = NULL;
    PPH_STRING ObjectName = NULL; // Original Name
    PPH_STRING BestObjectName = NULL; // File Name // todo

    if (!NT_SUCCESS(PhGetHandleInformationEx(hProcess, (HANDLE)handle->HandleValue, handle->ObjectTypeIndex, 0, NULL, NULL, &TypeName, &ObjectName, &BestObjectName, NULL)))
        return L"";

    if(TypeName) PhDereferenceObject(TypeName);
    if(BestObjectName) PhDereferenceObject(BestObjectName);

    return PHStr2WStr(ObjectName);
}