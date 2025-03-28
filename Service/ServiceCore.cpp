#include "pch.h"

#include <objbase.h>

#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/IPC/PipeServer.h"
#include "../Library/IPC/AlpcPortServer.h"
#include "Network/NetworkManager.h"
#include "Etw/EtwEventMonitor.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"
#include "Processes/ProcessList.h"
#include "Enclaves/EnclaveManager.h"
#include "Programs/ProgramManager.h"
#include "Network/SocketList.h"
#include "Network/Firewall/Firewall.h"
#include "Access/AccessManager.h"
#include "Volumes/VolumeManager.h"
#include "Tweaks/TweakManager.h"
#include "../Library/Helpers/Service.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Common/Exception.h"
#include "../MajorPrivacy/version.h"
#include "../Library/Helpers/NtPathMgr.h"

CServiceCore* theCore = NULL;


CServiceCore::CServiceCore()
	: m_Pool(4)
{
#ifdef _DEBUG
	TestVariant();
#endif

	m_pLog = new CEventLogger(API_SERVICE_NAME);

	m_pUserPipe = new CPipeServer();
	m_pUserPort = new CAlpcPortServer();

	m_pEnclaveManager = new CEnclaveManager();

	m_pProgramManager = new CProgramManager();

	m_pProcessList = new CProcessList();

	m_pAccessManager = new CAccessManager();

	m_pNetworkManager = new CNetworkManager();

	m_pVolumeManager = new CVolumeManager();

	m_pTweakManager = new CTweakManager();

	m_pDriver = new CDriverAPI();

	m_pEtwEventMonitor = new CEtwEventMonitor();

	RegisterUserAPI();

	CNtPathMgr::Instance()->RegisterDeviceChangeCallback(DeviceChangedCallback, this);
}

CServiceCore::~CServiceCore()
{
	delete m_pEtwEventMonitor;

	delete m_pDriver;

	delete m_pTweakManager;

	delete m_pVolumeManager;

	delete m_pNetworkManager;

	delete m_pAccessManager;

	delete m_pProcessList;

	delete m_pProgramManager;

	delete m_pEnclaveManager;

	delete m_pUserPipe;
	delete m_pUserPort;

	delete m_pLog;

	delete m_pConfig; // could be NULL


	CNtPathMgr::Instance()->UnRegisterDeviceChangeCallback(DeviceChangedCallback, this);
}

STATUS CServiceCore::Startup(bool bEngineMode)
{
	if (theCore)
		return ERR(SPAPI_E_DEVINST_ALREADY_EXISTS);

	//
	// Check if our event log exists, and if not create it
	//

	if (!EventSourceExists(API_SERVICE_NAME))
		CreateEventSource(API_SERVICE_NAME, APP_NAME);
	
	if (!EventSourceExists(API_DRIVER_NAME))
		CreateEventSource(API_DRIVER_NAME, APP_NAME);

	// todo start driver if not already started

	theCore = new CServiceCore();
	theCore->m_bEngineMode = bEngineMode;

	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Starting PrivacyAgent v%S", VERSION_STR);

	STATUS Status = theCore->Init();
	if (Status.IsError())
		Shutdown();
	
	return Status;
}

VOID CALLBACK CServiceCore__TimerProc(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	CServiceCore* This = (CServiceCore*)lpArgToCompletionRoutine;

	This->OnTimer();
}

STATUS CServiceCore::InitDriver()
{
	SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);

	//
	// Check if the driver is ours
	//

	if ((DrvState & SVC_INSTALLED) == SVC_INSTALLED && (DrvState & SVC_RUNNING) != SVC_RUNNING)
	{
		std::wstring BinaryPath = GetServiceBinaryPath(API_DRIVER_NAME);

		std::wstring ServicePath = CServiceCore::NormalizePath(GetFileFromCommand(BinaryPath));
		std::wstring AppDir = CServiceCore::NormalizePath(m_AppDir);
		if (ServicePath.length() < AppDir.length() || ServicePath.compare(0, AppDir.length(), AppDir) != 0)
		{
			theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Updated driver, old path: %s", BinaryPath);
			RemoveService(API_DRIVER_NAME);
			DrvState = SVC_NOT_FOUND;
		}
	}

	//
	// Install the driver
	//

	if ((DrvState & SVC_INSTALLED) == 0)
	{
		uint32 TraceLogLevel = m_pConfig->GetInt("Driver", "TraceLogLevel", 0);
		STATUS Status = m_pDriver->InstallDrv(TraceLogLevel);
		if (Status) 
		{
			CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
			DWORD disposition;
			if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\" API_DRIVER_NAME L"\\Parameters", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disposition) == ERROR_SUCCESS)
			{
				CBuffer Buffer;
				if (ReadFile(theCore->GetDataFolder() + L"\\KernelIsolator.dat", 0, Buffer))
				{
					CVariant DriverData;
					//try {
					auto ret = DriverData.FromPacket(&Buffer, true);

					CVariant ConfigData = DriverData[API_S_CONFIG];
					CBuffer ConfigBuff;
					ConfigData.ToPacket(&ConfigBuff);

					RegSet(hKey, L"Config", L"Data", CVariant(ConfigBuff));
					if (DriverData.Has(API_S_USER_KEY))
					{
						CVariant UserKey = DriverData[API_S_USER_KEY];
						RegSet(hKey, L"UserKey", L"PublicKey", UserKey[API_S_PUB_KEY]);
						if (UserKey.Has(API_S_KEY_BLOB)) RegSet(hKey, L"UserKey", L"KeyBlob", UserKey[API_S_KEY_BLOB]);
					}
					//}
					//catch (const CException&) {
					if (ret != CVariant::eErrNone)
						Status = ERR(STATUS_UNSUCCESSFUL);
					//}
				}
				else // todo: remove fix for 0.97.0 driver failing when no config is found
				{
					CVariant ConfigData;
					ConfigData.BeginMap();
					ConfigData.Finish();
					CBuffer ConfigBuff;
					ConfigData.ToPacket(&ConfigBuff);
					RegSet(hKey, L"Config", L"Data", CVariant(ConfigBuff));
				}
			}
		}
		if (Status.IsError()) {
			theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to install driver, error: 0x%08X", Status.GetStatus());
			return Status;
		}
	}

	//
	// Connect to the driver, its started when not running
	//

	return m_pDriver->ConnectDrv();
}

void CServiceCore::CloseDriver()
{
	theCore->m_pDriver->Disconnect();

	if (theCore->m_bEngineMode)
	{
		KillService(API_DRIVER_NAME);

		for (;;) {
			SVC_STATE SvcState = GetServiceState(API_DRIVER_NAME);
			if ((SvcState & SVC_RUNNING) == SVC_RUNNING) {
				Sleep(100);
				continue;
			}
			break;
		}

		CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\" API_DRIVER_NAME L"\\Parameters", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			CVariant Config = RegQuery(hKey, L"Config", L"Data");
			CBuffer ConfigBuff = Config;
			CVariant ConfigData;
			ConfigData.FromPacket(&ConfigBuff, true);

			CVariant KeyBlob = RegQuery(hKey, L"UserKey", L"KeyBlob");
			CVariant PublicKey = RegQuery(hKey, L"UserKey", L"PublicKey");

			CVariant DriverData;
			DriverData[API_S_CONFIG] = ConfigData;
			if (PublicKey.GetSize() > 0) {
				CVariant UserKey;
				UserKey[API_S_PUB_KEY] = PublicKey;
				if (KeyBlob.GetSize() > 0) UserKey[API_S_KEY_BLOB] = KeyBlob;
				DriverData[API_S_USER_KEY] = UserKey;
			}

			CBuffer Buffer;
			DriverData.ToPacket(&Buffer);
			WriteFile(theCore->GetDataFolder() + L"\\KernelIsolator.dat", 0, Buffer);

			RemoveService(API_DRIVER_NAME);
		}
	}
}

DWORD CALLBACK CServiceCore__ThreadProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"CServiceCore__ThreadProc");
#endif

	CServiceCore* This = (CServiceCore*)lpThreadParameter;

	NTSTATUS status;
	uint32 uDrvABI;

	//
	// Setup required privileges
	//
	
	HANDLE tokenHandle;
	if (NT_SUCCESS(status = NtOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
	{
		CHAR privilegesBuffer[FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges) + sizeof(LUID_AND_ATTRIBUTES) * 3];
		PTOKEN_PRIVILEGES privileges;
		ULONG i;

		privileges = (PTOKEN_PRIVILEGES)privilegesBuffer;
		privileges->PrivilegeCount = 3;

		for (i = 0; i < privileges->PrivilegeCount; i++)
		{
			privileges->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
			privileges->Privileges[i].Luid.HighPart = 0;
		}

		privileges->Privileges[0].Luid.LowPart = SE_DEBUG_PRIVILEGE;
		privileges->Privileges[1].Luid.LowPart = SE_LOAD_DRIVER_PRIVILEGE;
		privileges->Privileges[2].Luid.LowPart = SE_SECURITY_PRIVILEGE; // set audit policy

		status = NtAdjustPrivilegesToken(tokenHandle, FALSE, privileges, 0, NULL, NULL);

		NtClose(tokenHandle);
	}

	if(!NT_SUCCESS(status))
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to set privileges, error: 0x%08X", status);

	//
	// init COM for this thread
	//

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	//
	// Initialize the driver
	//

	This->m_InitStatus = This->InitDriver();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to connect to driver, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	uDrvABI = This->m_pDriver->GetABIVersion();
	if(uDrvABI != MY_ABI_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Driver ABI version mismatch, expected: %06X, got %06X", MY_ABI_VERSION, uDrvABI);
		goto cleanup;
	}

	//
	// Initialize the rest of the components
	//

	This->m_InitStatus = This->m_pEnclaveManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Enclave Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	This->m_InitStatus = This->m_pProgramManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Program Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}
	
	This->m_InitStatus = This->m_pProcessList->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Process List, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}
	
	This->m_InitStatus = This->m_pAccessManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Access Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	This->m_InitStatus = This->m_pNetworkManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Network Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	This->m_InitStatus = This->m_pVolumeManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Volume Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}
	
	This->m_InitStatus = This->m_pTweakManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Tweak Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	if (This->Config()->GetBool("Service", "UseETW", true)) {
		if (!This->m_pEtwEventMonitor->Init())
			theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to initialize ETW Monitoring");
	}

	//
	// timer being created indicates to CServiceCore::Init() that it can continue
	//

	This->m_hTimer = CreateWaitableTimer(NULL, FALSE, FALSE);

    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -10000000LL; // let the timer start after 1 seconds and the repeat every 250 ms
	if (!SetWaitableTimer(This->m_hTimer, &liDueTime, 250, CServiceCore__TimerProc, This, FALSE))if (!SetWaitableTimer(This->m_hTimer, &liDueTime, 250, CServiceCore__TimerProc, This, FALSE))
        return ERR(GetLastWin32ErrorAsNtStatus());

	//
	// Loop waiting for the timer to do something, or for other events
	//

	while (!This->m_Shutdown)
		WaitForSingleObjectEx(This->m_hTimer, INFINITE, TRUE);


	CloseHandle(theCore->m_hTimer);

	This->m_LastStoreTime = GetTickCount64();
	This->StoreRecords();
	DbgPrint(L"StoreRecords took %llu ms\n", GetTickCount64() - This->m_LastStoreTime);
	

	This->m_pVolumeManager->DismountAll();

cleanup:

	if (theCore->m_pDriver->IsConnected())
		This->CloseDriver();

	if (This->m_Shutdown == 2) {
		delete theCore;
		theCore = NULL;
	}

	CoUninitialize();

	return 0;
}

STATUS CServiceCore::Init()
{
	m_AppDir = GetApplicationDirectory();

	// Initialize this variable here befor any threads are started and later access it without synchronisation for reading only
	m_DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + GROUP_NAME + L"\\" + APP_NAME;
	if (!std::filesystem::exists(m_DataFolder)) 
	{
		if (std::filesystem::create_directories(m_DataFolder)) 
		{
			SetAdminFullControl(m_DataFolder);

			//std::wofstream file(m_DataFolder + L"\\Readme.txt");
			//if (file.is_open()) {
			//	file << L"This folder contains data used by " << APP_NAME << std::endl;
			//	file.close();
			//}
		}
	}

	m_pConfig = new CConfigIni(m_DataFolder + L"\\" + API_SERVICE_NAME + L".ini");

	m_LastStoreTime = GetTickCount64();

	m_hThread = CreateThread(NULL, 0, CServiceCore__ThreadProc, (void *)this, 0, NULL);
    if (!m_hThread)
        return ERR(GetLastWin32ErrorAsNtStatus());

	//
	// Wait for the thread to initialize
	//

	while (!m_hTimer) {
		DWORD exitCode;
		if (GetExitCodeThread(m_hThread, &exitCode) && exitCode != STILL_ACTIVE)
			break; // thread terminated prematurely - init failed
		Sleep(100);
	}
	
	if (!m_InitStatus.IsError())
		m_InitStatus = m_pUserPipe->Open(API_SERVICE_PIPE);

	if (!m_InitStatus.IsError())
		m_InitStatus = m_pUserPort->Open(API_SERVICE_PORT);

	return m_InitStatus;
}

void CServiceCore::Shutdown(bool bWait)
{
	if (!theCore || !theCore->m_hThread)
		return;
	HANDLE hThread = theCore->m_hThread;

	theCore->m_Shutdown = bWait ? 1 : 2;
	if (!bWait) 
		return;

	if (WaitForSingleObject(hThread, 10 * 1000) != WAIT_OBJECT_0)
		TerminateThread(hThread, -1);
	CloseHandle(hThread);

	delete theCore;
	theCore = NULL;
}

void CServiceCore::StoreRecords(bool bAsync)
{
	m_pProgramManager->Store();

	if(bAsync)
		m_pProcessList->StoreAsync();
	else
		m_pProcessList->Store();

	if(bAsync)
		m_pAccessManager->StoreAsync();
	else
		m_pAccessManager->Store();

	if (bAsync)
		m_pNetworkManager->StoreAsync();
	else
		m_pNetworkManager->Store();
}

void CServiceCore::Reconfigure(const std::string& Key)
{
	if(Key == "LogRegistry")
		m_pProcessList->Reconfigure();

	//m_pProgramManager->Reconfigure();

	//m_pAccessManager->Reconfigure();

	//m_pNetworkManager->Reconfigure();

	//m_pEtwEventMonitor->Reconfigure();
}

STATUS CServiceCore::CommitConfig()
{
	m_pProgramManager->Store();
	m_bConfigDirty = false;

	return OK;
}

STATUS CServiceCore::DiscardConfig()
{
	// todo: reload config from disk
	m_bConfigDirty = false;
	return OK;
}

#ifdef _DEBUG
size_t getHeapUsage() 
{
	_HEAPINFO heapInfo;
	heapInfo._pentry = nullptr;
	size_t usedMemory = 0;

	while (_heapwalk(&heapInfo) == _HEAPOK) {
		if (heapInfo._useflag == _USEDENTRY) {
			usedMemory += heapInfo._size;
		}
	}

	return usedMemory;
}
#endif

void CServiceCore::OnTimer()
{
	m_pProcessList->Update();

	m_pNetworkManager->Update();

	m_pProgramManager->Update();

	m_pAccessManager->Update();

	if (m_LastStoreTime + 60 * 60 * 1000 < GetTickCount64()) // once per hours
	{
		m_LastStoreTime = GetTickCount64();
		StoreRecords(true);
		DbgPrint(L"Async StoreRecords took %llu ms\n", GetTickCount64() - m_LastStoreTime);
	}

#ifdef _DEBUG
	static uint64 LastObjectDump = GetTickCount64();
	if (LastObjectDump + 60 * 1000 < GetTickCount64()) {
		LastObjectDump = GetTickCount64();
		ObjectTrackerBase::PrintCounts();

		//size_t memoryUsed = getHeapUsage();
		//DbgPrint(L"USED MEMORY: %llu bytes\n", memoryUsed);
	}
#endif
}

void CServiceCore::DeviceChangedCallback(void* param)
{
	CServiceCore* This = (CServiceCore*)param;

	// todo: handle device change
}

// DOS Prefixes
// L"\\\\?\\" prefix is used to bypass the MAX_PATH limitation
// L"\\\\.\\" points to L"\Device\"

// NT Prefixes
// L"\\??\\" prefix (L"\\GLOBAL??\\") maps DOS letters and more

std::wstring CServiceCore::NormalizePath(std::wstring Path, bool bLowerCase)
{
	if(Path.empty() || Path[0] == L'*')
		return Path;

	if (Path.length() >= 7 && Path.compare(0, 4, L"\\??\\") == 0 && Path.compare(5, 2, L":\\") == 0) // \??\X:\...
		Path.erase(0, 4);
	else if (Path.length() >= 7 && Path.compare(0, 4, L"\\\\?\\") == 0 && Path.compare(5, 2, L":\\") == 0) // \\?\X:\...
		Path.erase(0, 4);

	if (Path.find(L'%') != std::wstring::npos)
		Path = ExpandEnvironmentVariablesInPath(Path);

	if (MatchPathPrefix(Path, L"\\SystemRoot")) {
		static WCHAR windir[MAX_PATH + 8] = { 0 };
		if (!*windir) GetWindowsDirectoryW(windir, MAX_PATH);
		Path = windir + Path.substr(11);
	}

	if (!CNtPathMgr::IsDosPath(Path) && !MatchPathPrefix(Path, L"\\REGISTRY"))
	{
		//if (MatchPathPrefix(Path, L"\\Device"))
		//	Path = L"\\\\.\\" + Path.substr(8);
		
		if (Path.length() >= 4 && Path.compare(0, 4, L"\\\\.\\") == 0) 
			Path = L"\\Device\\" + Path.substr(4);

		if (MatchPathPrefix(Path, L"\\Device"))
		{
			//std::wstring DosPath = CNtPathMgr::Instance()->TranslateNtToDosPath(L"\\Device\\" + Path.substr(4));
			std::wstring DosPath = CNtPathMgr::Instance()->TranslateNtToDosPath(Path);
			if (!DosPath.empty())
				Path = DosPath;
#ifdef _DEBUG
			else
				DbgPrint("Encountered Unresolved NtPath %S\n", Path.c_str());
#endif
		}
#ifdef _DEBUG
		else
			DbgPrint("Encountered Unecpected NtPath %S\n", Path.c_str());
#endif
	}

	return bLowerCase ? MkLower(Path) : Path;
}