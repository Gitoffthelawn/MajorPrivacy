#pragma once
#include "../Common/AbstractInfo.h"
#include "../Library/Common/Variant.h"
#include "../Network/Socket.h"
#include "../Network/Dns/DnsProcLog.h"

class CProcess: public CAbstractInfoEx
{
public:
	CProcess(uint64 Pid);
	~CProcess();

	uint64 GetProcessId() const { std::shared_lock Lock(m_Mutex); return m_Pid; }
	uint64 GetParentId() const { std::shared_lock Lock(m_Mutex); return m_ParentPid; }
	std::wstring GetName() const { std::shared_lock Lock(m_Mutex); return m_Name; }
	std::wstring GetFileName() const { std::shared_lock Lock(m_Mutex); return m_FileName; }
	std::wstring GetWorkDir() const;
	uint32 GetEnclave() const { std::shared_lock Lock(m_Mutex); return m_EnclaveId; }

	std::set<std::wstring> GetServices() const { std::shared_lock Lock(m_Mutex);  return m_ServiceList; }

	std::wstring GetAppContainerSid() const { std::shared_lock Lock(m_Mutex); return m_AppContainerSid; }
	std::wstring GetAppContainerName() const { std::shared_lock Lock(m_Mutex); return m_AppContainerName; }

	std::shared_ptr<class CProgramFile> GetProgram() const { std::shared_lock Lock(m_Mutex); return m_pFileRef.lock(); }

	void AddSocket(const CSocketPtr& pSocket);
	void RemoveSocket(const CSocketPtr& pSocket, bool bNoCommit = false);
	std::set<CSocketPtr> GetSocketList() const { std::shared_lock Lock(m_SocketMutex); return m_SocketList;  }
	int GetSocketCount() const { std::shared_lock Lock(m_SocketMutex); return (int)m_SocketList.size(); }

	CDnsProcLog* DnsLog()					{ return &m_DnsLog; }

	uint64	GetLastActivity() const			{ std::shared_lock Lock(m_Mutex); return m_LastActivity;}

	uint64	GetUpload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRate.Get();}
	uint64	GetDownload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRate.Get();}
	uint64	GetUploaded() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRaw;}
	uint64	GetDownloaded() const			{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRaw;}

	void	AddNetworkIO(int Type, uint32 TransferSize);

	CVariant ToVariant() const;

	static const wchar_t* NtOsKernel_exe; 

protected:
	friend class CProcessList;
	friend class CServiceList;
	friend class CProgramManager;

	void AddService(const std::wstring& Service) { std::unique_lock Lock(m_Mutex); m_ServiceList.insert(Service); }
	void RemoveService(const std::wstring& Service) { std::unique_lock Lock(m_Mutex); m_ServiceList.erase(Service); }

	bool Init();
	bool Update();

	bool Init(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo);
	bool Update(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo);

	void UpdateMisc();

	void SetRawCreationTime(uint64 TimeStamp);
	bool InitOther();

	uint64 m_Pid = -1;
	uint64 m_CreationTime = 0;
	//uint64 m_SequenceNr = -1;
	uint64 m_ParentPid = -1;
	std::wstring m_Name;
	std::wstring m_FileName;
	std::wstring m_CommandLine;

	std::vector<uint8> m_ImageHash;
	uint32 m_EnclaveId = 0;

	std::set<std::wstring>		m_ServiceList;

	std::wstring m_AppContainerSid;
	std::wstring m_AppContainerName;
	//std::wstring m_PackageFullName;

	mutable std::shared_mutex	m_SocketMutex;
	std::set<CSocketPtr>		m_SocketList;

	CDnsProcLog					m_DnsLog;

	//
	// Note: For windows services the m_pFileRef is NULL as one process svchost.exe 
	// may contain multiple services, when handling services always use thair SvcId
	// and reffer to the service man in the process manager
	//

	std::weak_ptr<class CProgramFile> m_pFileRef; 

	uint64						m_LastActivity = 0;
	// I/O stats
	mutable std::shared_mutex	m_StatsMutex;
	SProcStats					m_Stats;
};

typedef std::shared_ptr<CProcess> CProcessPtr;