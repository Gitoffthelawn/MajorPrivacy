#include "pch.h"

#include <objbase.h>
#include <Netlistmgr.h>
//#include <comdef.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>

#include "NetworkManager.h"
#include "Firewall/Firewall.h"
#include "SocketList.h"
#include "Dns/DnsInspector.h"
#include "Dns/DnsFilter.h"
#include "Dns/DnsClient.h"
#include "Dns/DnsConfigurator.h"

#include "../ServiceCore.h"
#include "../Programs/ProgramManager.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Strings.h"
#include "../Library/Helpers/WinUtil.h"

#define API_TRAFFIC_RECORD_FILE_NAME L"TrafficRecord.dat"
#define API_TRAFFIC_RECORD_FILE_VERSION 1


CNetworkManager::CNetworkManager()
{
	m_NicKeyWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
	m_NicKeyWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters\\Interfaces");
	m_NicKeyWatcher.AddKey(L"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Profiles"); // firewall profile configuration

	m_NicKeyWatcher.RegisterHandler([&](const std::wstring&){
		m_UpdateAdapterInfo = true;
	});

	m_NicKeyWatcher.Start();

	m_pFirewall = new CFirewall();
	m_pSocketList = new CSocketList();
    m_pDnsInspector = new CDnsInspector();
    m_pDnsFilter = new CDnsFilter();
}

CNetworkManager::~CNetworkManager()
{
	m_NicKeyWatcher.Stop();

	delete m_pFirewall;
	delete m_pSocketList;
    delete m_pDnsInspector;
    delete m_pDnsFilter;

    if (CDnsConfigurator::IsAnyLocalDNS())
        CDnsConfigurator::RestoreDNS();
}

DWORD CALLBACK CNetworkManager__LoadProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
    MySetThreadDescription(GetCurrentThread(), L"CNetworkManager__LoadProc");
#endif

    CNetworkManager* This = (CNetworkManager*)lpThreadParameter;

    uint64 uStart = GetTickCount64();
    This->Reconfigure();
	DbgPrint(L"CNetworkManager::Reconfigure() took %llu ms\n", GetTickCount64() - uStart);

    uStart = GetTickCount64();
    STATUS Status = This->Load();
    DbgPrint(L"CNetworkManager::Load() took %llu ms\n", GetTickCount64() - uStart);

    NtClose(This->m_hStoreThread);
    This->m_hStoreThread = NULL;
    return 0;
}

STATUS CNetworkManager::Init()
{
    //ULONGLONG Start = GetTickCount64();

    UpdateAdapterInfo();

    //ULONGLONG End = GetTickCount64();
    //theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent UpdateAdapterInfo took %dms", End - Start);
    //Start = End;

    m_pDnsInspector->Init();

    //End = GetTickCount64();
	//theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent DnsInspector init took %dms", End - Start);
    //Start = End;

	m_pFirewall->Init();

    //End = GetTickCount64();
	//theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Firewall init took %dms", End - Start);
    //Start = End;

	m_pSocketList->Init();

    //End = GetTickCount64();
	//theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent SocketList init took %dms", End - Start);
    //Start = End;

	m_pDnsFilter->Init();

    //End = GetTickCount64();
	//theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent DnsFilter init took %dms", End - Start);

    m_hStoreThread = CreateThread(NULL, 0, CNetworkManager__LoadProc, (void*)this, 0, NULL);

	return OK;
}

DWORD CALLBACK CNetworkManager__StoreProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
    MySetThreadDescription(GetCurrentThread(), L"CNetworkManager__StoreProc");
#endif

    CNetworkManager* This = (CNetworkManager*)lpThreadParameter;

    uint64 uStart = GetTickCount64();
    STATUS Status = This->Store();
    DbgPrint(L"CNetworkManager::Store() took %llu ms\n", GetTickCount64() - uStart);

    NtClose(This->m_hStoreThread);
    This->m_hStoreThread = NULL;
    return 0;
}

STATUS CNetworkManager::StoreAsync()
{
    if (m_hStoreThread) return STATUS_ALREADY_COMMITTED;
    m_hStoreThread = CreateThread(NULL, 0, CNetworkManager__StoreProc, (void*)this, 0, NULL);
    return STATUS_SUCCESS;
}

void CNetworkManager::Update()
{
    if (m_UpdateAdapterInfo)
    {
        UpdateAdapterInfo();

        UpdateDnsConfig(theCore->Config()->GetBool("Service", "DnsEnableFilter", false) && theCore->Config()->GetBool("Service", "DnsInstallFilter", true));
    }

    m_pDnsInspector->Update();
    m_pSocketList->Update();
    m_pFirewall->Update();
}

void CNetworkManager::Reconfigure(bool bWithResolvers, bool bWithBlocklist)
{
    bool bEnableFilter = theCore->Config()->GetBool("Service", "DnsEnableFilter", false);

    if (m_pDnsFilter->IsRunning() != bEnableFilter)
    {
        if(bEnableFilter){
            if (m_pDnsFilter->Start())
                m_pDnsFilter->LoadHosts(theCore->GetDataFolder() + L"\\dns\\hosts");
            else
                theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_DNS_INIT_FAILED, L"Failed to start DNS server");
        } else {
            m_pDnsFilter->Stop();
            m_pDnsFilter->Clear();
        }
    }

    if (!bEnableFilter)
        m_pDnsState = eNoDnsFilter;
    else if(!m_pDnsFilter->IsRunning())
		m_pDnsState = eDnsFilterFailed;
	else
    {
        m_pDnsState = eDnsFilterOk;

        if (bWithResolvers || m_pDnsFilter->GetForwardAddress().empty())
        {
            auto Resolvers = SplitStr(theCore->Config()->GetValue("Service", "DnsResolvers"), L";");
            //bool contains = std::find(Resolvers.begin(), Resolvers.end(), m_pDnsFilter->GetForwardAddress()) == Resolvers.end();

            std::wstring ForwardAddress;
            std::wstring TestHost = theCore->Config()->GetValue("Service", "DnsTestHost", L"example.com");
            for (auto& Resolver : Resolvers)
            {
                if (!TestHost.empty()) {
                    CDnsClient TestClient(Resolver);
                    CDnsSocket::SAddress Addr;
                    bool bOk = TestClient.ResolveHost(w2s(TestHost), Addr);
                    //auto Ret = Addr.ntop();
                    if (!bOk)
                        continue;
                }
                ForwardAddress = Resolver;
                break;
            }

            if (ForwardAddress.empty()) {
                theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_DNS_INIT_FAILED, L"No functional upstream DNS resolver found");
                m_pDnsState = eDnsFilterFailed;
            }
            m_pDnsFilter->SetForwardAddress(ForwardAddress);
        }

        if(bWithBlocklist || !m_pDnsFilter->AreBlockListsLoaded())
            m_pDnsFilter->UpdateBlockLists();
    }

    bool bInstallFilter = bEnableFilter && theCore->Config()->GetBool("Service", "DnsInstallFilter", true);
	UpdateDnsConfig(bInstallFilter);
}

void CNetworkManager::UpdateDnsConfig(bool bInstallFilter)
{
    if (bInstallFilter)
    {
        if(!CDnsConfigurator::IsLocalDNS())
            CDnsConfigurator::SetLocalDNS();
    }
    else // off
    {
        if(CDnsConfigurator::IsAnyLocalDNS())
            CDnsConfigurator::RestoreDNS();
    }
}

void CNetworkManager::UpdateAdapterInfo()
{
    HRESULT hr = S_OK;

    std::map<std::string, EFwProfiles> NetworkProfiles;

    auto NetCatToProfile = [](DWORD nlmCategory) {
        switch (nlmCategory) {
        case NLM_NETWORK_CATEGORY_PRIVATE:              return EFwProfiles::Private; 
        case NLM_NETWORK_CATEGORY_PUBLIC:               return EFwProfiles::Public; 
        case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED: return EFwProfiles::Domain; 
        default:                                        return EFwProfiles::Invalid;
        }
    };

    //hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    //if (FAILED(hr))
    //    return;

    INetworkListManager* pNetworkListManager;
    hr = CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL, IID_INetworkListManager, (LPVOID*)&pNetworkListManager);
    if (SUCCEEDED(hr)) 
    {
        IEnumNetworks* pEnumNetworks;
        hr = pNetworkListManager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &pEnumNetworks);
        if (SUCCEEDED(hr)) 
        {
            INetwork* pNetwork;
            ULONG fetched;

            while (pEnumNetworks->Next(1, &pNetwork, &fetched) == S_OK) 
            {
                NLM_NETWORK_CATEGORY nlmCategory;
                pNetwork->GetCategory(&nlmCategory);

                EFwProfiles FirewallProfile = NetCatToProfile(nlmCategory);
                if(FirewallProfile == EFwProfiles::Invalid){
                    pNetwork->Release();
                    continue;
                }

                IEnumNetworkConnections* pEnumNetworkConnections;
                hr = pNetwork->GetNetworkConnections(&pEnumNetworkConnections);
                if (SUCCEEDED(hr)) 
                {
                    INetworkConnection* pNetworkConnection;
                    while (pEnumNetworkConnections->Next(1, &pNetworkConnection, &fetched) == S_OK) 
                    {
                        GUID adapterId;
                        pNetworkConnection->GetAdapterId(&adapterId);

                        WCHAR guidString[40] = {0};
                        StringFromGUID2(adapterId, guidString, 40);

                        std::wstring guid(guidString);
#pragma warning(push)
#pragma warning(disable : 4244)
                        NetworkProfiles[std::string(guid.begin(), guid.end())] = FirewallProfile;
#pragma warning(pop)

                        pNetworkConnection->Release();
                    }
                    pEnumNetworkConnections->Release();
                }

                pNetwork->Release();
            }

            pEnumNetworks->Release();
        }

        pNetworkListManager->Release();
    }

    //CoUninitialize();

    // Get default behavioure for unidentified networks
    DWORD DefaultNetCategory = RegQueryDWord(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Signatures\\"
        L"010103000F0000F0010000000F0000F0C967A3643C3AD745950DA7859209176EF5B87C875FA20DF21951640E807D7C24",
        L"Category", 0);
    EFwProfiles DefaultProfile = NetCatToProfile(DefaultNetCategory);

    std::map<CAddress, SAdapterInfoPtr> AdapterInfoByIP;

	//MIB_IF_TABLE2* pIfTable = NULL;
	//if (GetIfTable2(&pIfTable) == NO_ERROR)
	//{
	//	uint64 uRecvTotal = 0;
	//	uint64 uRecvCount = 0;
	//	uint64 uSentTotal = 0;
	//	uint64 uSentCount = 0;
	//	for (int i = 0; i < pIfTable->NumEntries; i++)
	//	{
	//		MIB_IF_ROW2* pIfRow = (MIB_IF_ROW2*)&pIfTable->Table[i];
    //
	//		if (pIfRow->InterfaceAndOperStatusFlags.FilterInterface)
	//			continue;
    //
    //      // todo
	//	}
	//}
	//FreeMibTable(pIfTable);

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_WINS_INFO | GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_ALL_INTERFACES;
    ULONG bufferLength = 0x1000;
    std::vector<char> buffer;
    for (ULONG ret = ERROR_BUFFER_OVERFLOW; ret == ERROR_BUFFER_OVERFLOW && bufferLength > buffer.size(); )
    {
        buffer.resize(bufferLength, 0);
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, (PIP_ADAPTER_ADDRESSES)buffer.data(), &bufferLength);
        if (ret != ERROR_SUCCESS)
            continue;
        
        for (PIP_ADAPTER_ADDRESSES itf = (PIP_ADAPTER_ADDRESSES)buffer.data(); itf; itf = itf->Next)
        {
            if (itf->OperStatus == IfOperStatusNotPresent || (!itf->Ipv4Enabled && !itf->Ipv6Enabled))
                continue;
            //DbgPrint("name: %s (%S) %d\n", itf->AdapterName, itf->FriendlyName, itf->OperStatus);
            
            SAdapterInfoPtr Info = std::make_shared<SAdapterInfo>();

            auto F = NetworkProfiles.find(itf->AdapterName);
            if (F != NetworkProfiles.end())
                Info->Profile = F->second;
            else
                Info->Profile = DefaultProfile;

            switch (itf->IfType)
            {
                case IF_TYPE_ETHERNET_CSMACD:
                case IF_TYPE_GIGABITETHERNET:
                case IF_TYPE_FASTETHER:
                case IF_TYPE_FASTETHER_FX:
                case IF_TYPE_ETHERNET_3MBIT:
                case IF_TYPE_ISO88025_TOKENRING:    
                                                    Info->Type = EFwInterfaces::Lan; break;
                case IF_TYPE_IEEE80211:             Info->Type = EFwInterfaces::Wireless; break;
                case IF_TYPE_PPP:                   Info->Type = EFwInterfaces::RemoteAccess; break;
                default:                            Info->Type = EFwInterfaces::All; break;
            }

            for (PIP_ADAPTER_UNICAST_ADDRESS_LH addr = itf->FirstUnicastAddress; addr; addr = addr->Next) {
                CUAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                IP.OnLinkPrefixLength = addr->OnLinkPrefixLength;
                IP.PrefixOrigin = addr->PrefixOrigin;
                IP.SuffixOrigin = addr->SuffixOrigin;
                Info->UnicastAddresses.push_back(IP);
            }
            for (PIP_ADAPTER_ANYCAST_ADDRESS_XP addr = itf->FirstAnycastAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->AnycastAddresses.push_back(IP);
            }
            for (PIP_ADAPTER_MULTICAST_ADDRESS_XP addr = itf->FirstMulticastAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->MulticastAddresses.push_back(IP);
            }

            for (PIP_ADAPTER_GATEWAY_ADDRESS_LH addr = itf->FirstGatewayAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->GatewayAddresses.push_back(IP);
            }

            for(PIP_ADAPTER_DNS_SERVER_ADDRESS_XP addr = itf->FirstDnsServerAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->DnsAddresses.push_back(IP);
            }
            for(PIP_ADAPTER_WINS_SERVER_ADDRESS_LH addr = itf->FirstWinsServerAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->WinsServersAddresses.push_back(IP);
            }

            if (itf->Dhcpv4Server.iSockaddrLength) {
                CAddress IP;
                IP.FromSA(itf->Dhcpv4Server.lpSockaddr, itf->Dhcpv4Server.iSockaddrLength);
                Info->DhcpServerAddresses.push_back(IP);
            }
            if (itf->Dhcpv6Server.iSockaddrLength){
                CAddress IP;
                IP.FromSA(itf->Dhcpv6Server.lpSockaddr, itf->Dhcpv6Server.iSockaddrLength);
                Info->DhcpServerAddresses.push_back(IP);
            }

            for (auto IpInfo : Info->UnicastAddresses)
                AdapterInfoByIP[IpInfo] = Info;
        }
    }

    std::unique_lock Lock(m_AdapterInfoMutex);
    m_AdapterInfoByIP = AdapterInfoByIP;
    m_DefaultProfile = DefaultProfile;
	m_UpdateAdapterInfo = false;
}

SAdapterInfoPtr CNetworkManager::GetAdapterInfoByIP(const CAddress& IP)
{
	std::shared_lock Lock(m_AdapterInfoMutex);
    auto F = m_AdapterInfoByIP.find(IP);
    if (F != m_AdapterInfoByIP.end())
        return F->second;
    return SAdapterInfoPtr();
}

//////////////////////////////////////////////////////////////////////////
// Load/Store

STATUS CNetworkManager::Load()
{
    CBuffer Buffer;
    if (!ReadFile(theCore->GetDataFolder() + L"\\" API_TRAFFIC_RECORD_FILE_NAME, Buffer)) {
        theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_TRAFFIC_RECORD_FILE_NAME L" not found");
        return ERR(STATUS_NOT_FOUND);
    }

    StVariant Data;
    //try {
    auto ret = Data.FromPacket(&Buffer, true);
    //} catch (const CException&) {
    //	return ERR(STATUS_UNSUCCESSFUL);
    //}
    if (ret != StVariant::eErrNone) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_TRAFFIC_RECORD_FILE_NAME);
        return ERR(STATUS_UNSUCCESSFUL);
    }

    if (Data[API_S_VERSION].To<uint32>() != API_TRAFFIC_RECORD_FILE_VERSION) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_TRAFFIC_RECORD_FILE_NAME);
        return ERR(STATUS_UNSUCCESSFUL);
    }

    StVariant List = Data[API_S_TRAFFIC_LOG];

    for (uint32 i = 0; i < List.Count(); i++)
    {
        StVariant Item = List[i];

        CProgramID ID;
        if(!ID.FromVariant(StVariantReader(Item).Find(API_V_ID)))
            continue;
        CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
            pProgram->LoadTraffic(Item);
        else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
            pService->LoadTraffic(Item);
    }

    return OK;
}

STATUS CNetworkManager::Store()
{
    SVarWriteOpt Opts;
    Opts.Format = SVarWriteOpt::eIndex;
    Opts.Flags = SVarWriteOpt::eSaveToFile;

    StVariantWriter List;
    List.BeginList();

    bool bSave = theCore->Config()->GetBool("Service", "SaveTrafficRecord", false);

    for (auto pItem : theCore->ProgramManager()->GetItems())
    {
        ESavePreset ePreset = pItem.second->GetSaveTrace();
        if (ePreset == ESavePreset::eDontSave || (ePreset == ESavePreset::eDefault && !bSave))
            continue;

        // StoreTraffic saves API_V_ID
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
            List.WriteVariant(pProgram->StoreTraffic(Opts));
        else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
            List.WriteVariant(pService->StoreTraffic(Opts));
    }

    StVariant Data;
    Data[API_S_VERSION] = API_TRAFFIC_RECORD_FILE_VERSION;
    Data[API_S_TRAFFIC_LOG] = List.Finish();

    CBuffer Buffer;
    Data.ToPacket(&Buffer);
    WriteFile(theCore->GetDataFolder() + L"\\" API_TRAFFIC_RECORD_FILE_NAME, Buffer);

    return OK;
}