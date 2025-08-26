#include "pch.h"
#include "MajorPrivacy.h"
#include "Core\PrivacyCore.h"
#include "../QtSingleApp/src/qtsingleapplication.h"

#include <phnt_windows.h>
#include <phnt.h>

#include <QtWidgets/QApplication>

//#include "../NtCRT/NtCRT.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#ifdef _DEBUG
//#include "../Library/Helpers/Reparse.h"
#include "../Library/Helpers/NtpathMgr.h"
//#include "./Common/QtFlexGuid.h"
#endif

#include "../Library/Hooking/HookUtils.h"

typedef NTSTATUS (*P_NtMapViewOfSection)(
	IN  HANDLE SectionHandle,
	IN  HANDLE ProcessHandle,
	IN  OUT PVOID *BaseAddress,
	IN  ULONG_PTR ZeroBits,
	IN  SIZE_T CommitSize,
	IN  OUT PLARGE_INTEGER SectionOffset OPTIONAL,
	IN  OUT PSIZE_T ViewSize,
	IN  ULONG InheritDisposition,
	IN  ULONG AllocationType,
	IN  ULONG Protect);

P_NtMapViewOfSection NtMapViewOfSectionTramp = NULL;

bool IsMemoryReadable(PVOID Address)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(Address, &mbi, sizeof(mbi)) == 0)
		return false;

	if (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))
		return true;

	return false;
}

NTSTATUS NTAPI MyMapViewOfSection(IN  HANDLE SectionHandle,
	IN  HANDLE ProcessHandle,
	IN  OUT PVOID* BaseAddress,
	IN  ULONG_PTR ZeroBits,
	IN  SIZE_T CommitSize,
	IN  OUT PLARGE_INTEGER SectionOffset OPTIONAL,
	IN  OUT PSIZE_T ViewSize,
	IN  ULONG InheritDisposition,
	IN  ULONG AllocationType,
	IN  ULONG Protect)
{
	NTSTATUS status = NtMapViewOfSectionTramp(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Protect);
	if (NT_SUCCESS(status))
	{
		if (BaseAddress && *BaseAddress && !IsMemoryReadable(*BaseAddress))
		{
			DbgPrint("MyMapViewOfSection: Invalid BaseAddress: %p", *BaseAddress);
			status = STATUS_ACCESS_DENIED;
		}
	}
	return status;
}

int main(int argc, char *argv[])
{
	srand(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

	//NTCRT_DEFINE(MyCRT);
	//InitGeneralCRT(&MyCRT);

	//
	// If a dll is not signed like a shell extension for the default windows file open dialog,
	// or alike, we have a problem as our driver when ImageLoadProtection == TRUE will block the loading of the dll
	// and unmap the just loaded section from the driver, so we add a sanity check for NtMapViewOfSection
	// if it returns no error but the memory is not readable return STATUS_ACCESS_DENIED instead.
	//

	HookFunction(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtMapViewOfSection"), MyMapViewOfSection, (VOID**)&NtMapViewOfSectionTramp);

#ifdef _DEBUG
	
/*	QFlexGuid guid;
	guid.FromQS("{12345678-1234-1234-1234-AbCdEFABcdEf}");
	QString Test = guid.ToQS();
	CVariant var = guid.ToVariant();
	QFlexGuid guid2;
	guid2.FromVariant(var);
	QString Test2 = guid2.ToQS();
	return 0;*/

	//std::wstring path = CPathReparse::Instance()->TranslateDosToNtPath(L"F:\\Projects\\MajorPrivacy");

	//CPathReparse::Instance()->InitDrives();
	//File_InitDrives(0xFFFFFFFF);
	//std::wstring testOld = File_TranslateTempLinks(path);

	//std::wstring testNt = CPathReparse::Instance()->TranslateTempLinks(path, false);

	//std::wstring testNt = L"\\Device\\HarddiskVolume9\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe";

	//std::wstring testNew = CPathReparse::Instance()->TranslateTempLinks(testNt, true);

	//std::wstring testDos = CPathReparse::Instance()->TranslateNtToDosPath(testNew);

	//std::wstring testDos = CPathReparse::Instance()->TranslateNtToDosPath(testNt);

	//std::wstring path = CNtPathMgr::Instance()->TranslateDosToNtPath(L"F:\\Projects\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe");
	//std::wstring path2 = CNtPathMgr::Instance()->TranslateDosToNtPath(L"F:\\Projects\\!Test\\Sandboxie\\Sandboxie\\SandboxiePlus\\x64\\Debug\\SandMan.exe");

	//std::wstring test = CNtPathMgr::Instance()->TranslateNtToDosPath(L"\\Device\\HarddiskVolume9\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe");
	//std::wstring test2 = CNtPathMgr::Instance()->TranslateNtToDosPath(L"\\Device\\HarddiskVolume17\\Sandboxie\\Sandboxie\\SandboxiePlus\\x64\\Debug\\SandMan.exe"); 

#endif

	QString AppDir = QString::fromStdWString(GetApplicationDirectory());
	theConf = new CSettings(AppDir, "MajorPrivacy", "Xanasoft");

	// this must be done before we create QApplication
	int DPI = theConf->GetInt("Options/DPIScaling", 1);
	if (DPI == 1) {
		//SetProcessDPIAware();
		//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		//SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		typedef DPI_AWARENESS_CONTEXT(WINAPI* P_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT dpiContext);
		P_SetThreadDpiAwarenessContext pSetThreadDpiAwarenessContext = (P_SetThreadDpiAwarenessContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
		if(pSetThreadDpiAwarenessContext) // not present on windows 7
			pSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		else
			SetProcessDPIAware();
	}
	else if (DPI == 2) {
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); 
	}
	//else {
	//	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	//}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

	//
	// Qt 6 uses the windows font cache which wants to access our process but our driver blocks it
	// that causes a lot of log entries, hence we disable the use of windows fonr cache.
	//
	qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("windows:nodirectwrite"));

	QtSingleApplication App(argc, argv);
	App.setQuitOnLastWindowClosed(false);

	if (App.sendMessage("ShowWnd"))
		return 0;

	theCore = new CPrivacyCore();

	if (App.style()->name() == "windows11" && !theConf->GetBool("Options/UseW11Style", false))
		App.setStyle("windowsvista");

	CMajorPrivacy* pWnd = new CMajorPrivacy;

	QObject::connect(&App, SIGNAL(messageReceived(const QString&)), pWnd, SLOT(OnMessage(const QString&)), Qt::QueuedConnection);

	pWnd->show();
	int ret = App.exec();

	delete pWnd;

	delete theCore;
	theCore = NULL;

	delete theConf;
	theConf = NULL;

	return ret;
}
