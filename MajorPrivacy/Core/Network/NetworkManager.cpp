#include "pch.h"
#include "NetworkManager.h"
#include "../PrivacyCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "./MiscHelpers/Common/Common.h"

CNetworkManager::CNetworkManager(QObject *parent) 
    : QObject(parent)
{
    
}

bool CNetworkManager::UpdateAllFwRules(bool bReLoad)
{
	auto Ret = theCore->GetAllFwRules(bReLoad);
	if (Ret.IsError())
		return false;

	XVariant& Rules = Ret.GetValue();

	QMap<QFlexGuid, CFwRulePtr> OldRules = m_FwRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const XVariant& Rule = Rules[i];

		QFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);

		CProgramID ID;
		ID.FromVariant(Rule[API_V_PROG_ID]);

		CFwRulePtr pRule = OldRules.value(Guid);
		if (pRule) {
			if(ID == pRule->GetProgramID())
				OldRules.remove(Guid);
			else // on ID change compeltely re add the rule as if it would be new
			{
				RemoveFwRule(pRule);
				pRule.clear();
			}
		}

		bool bAdd = false;
		if (!pRule) {
			pRule = CFwRulePtr(new CFwRule(ID));
			bAdd = true;
		} 

		pRule->FromVariant(Rule);

		if(bAdd)
			AddFwRule(pRule);
	}

	foreach(const QFlexGuid& Guid, OldRules.keys())
		RemoveFwRule(OldRules.take(Guid));

	return true;
}

bool CNetworkManager::UpdateFwRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetFwRule(Guid);
	if (Ret.IsError())
		return false;

	XVariant& Rule = Ret.GetValue();

	QString RuleID = Rule[API_V_GUID].AsQStr();

	CProgramID ID;
	ID.FromVariant(Rule[API_V_PROG_ID]);

	CFwRulePtr pRule = m_FwRules.value(RuleID);
	if (pRule) {
		if(ID != pRule->GetProgramID()) // on ID change compeltely re add the rule as if it would be new
		{
			RemoveFwRule(pRule);
			pRule.clear();
		}
	}

	bool bAdd = false;
	if (!pRule) {
		pRule = CFwRulePtr(new CFwRule(ID));
		bAdd = true;
	} 

	pRule->FromVariant(Rule);

	if(bAdd)
		AddFwRule(pRule);

	return true;
}

void CNetworkManager::RemoveFwRule(const QFlexGuid& Guid)
{
	CFwRulePtr pFwRule = m_FwRules.value(Guid);
	if (pFwRule)
		RemoveFwRule(pFwRule);
}

void CNetworkManager::AddFwRule(const CFwRulePtr& pFwRule)
{
	m_FwRules.insert(pFwRule->GetGuid(), pFwRule);

	//const CProgramID& ProgID = pFwRule->GetProgramID();

	//if (!ProgID.GetFilePath().isEmpty())
	//	m_FileRules.insert(ProgID.GetFilePath(), pFwRule);
	//if (!ProgID.GetServiceTag().isEmpty())
	//	m_SvcRules.insert(ProgID.GetServiceTag(), pFwRule);
	//if (!ProgID.GetAppContainerSid().isEmpty())
	//	m_AppRules.insert(ProgID.GetAppContainerSid(), pFwRule);
}

void CNetworkManager::RemoveFwRule(const CFwRulePtr& pFwRule)
{
	m_FwRules.remove(pFwRule->GetGuid());

	//const CProgramID& ProgID = pFwRule->GetProgramID();

	//if (!ProgID.GetFilePath().isEmpty())
	//	m_FileRules.remove(ProgID.GetFilePath(), pFwRule);
	//if (!ProgID.GetServiceTag().isEmpty())
	//	m_SvcRules.remove(ProgID.GetServiceTag(), pFwRule);
	//if (!ProgID.GetAppContainerSid().isEmpty())
	//	m_AppRules.remove(ProgID.GetAppContainerSid(), pFwRule);
}

QSet<QFlexGuid> CNetworkManager::GetFwRuleIDs() const 
{	
	return ListToSet(m_FwRules.keys()); 
}

//QList<CFwRulePtr> CNetworkManager::GetFwRulesFor(const QList<const class CProgramItem*>& Nodes)
//{
//	return QList<CFwRulePtr>();
//}

QList<CFwRulePtr> CNetworkManager::GetFwRules(const QSet<QFlexGuid>& FwRuleIDs)
{
	QList<CFwRulePtr> List;
	foreach(const QFlexGuid& Guid, FwRuleIDs) {
		CFwRulePtr pFwRule = m_FwRules.value(Guid);
		if (pFwRule)List.append(pFwRule);
	}
	return List;
}

STATUS CNetworkManager::SetFwRule(const CFwRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	return theCore->SetFwRule(pRule->ToVariant(Opts));
}

RESULT(CFwRulePtr) CNetworkManager::GetProgramRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetFwRule(Guid);
	if (Ret.IsError())
		return Ret;

	XVariant& Rule = Ret.GetValue();

	CProgramID ID;
	ID.FromVariant(Rule[API_V_PROG_ID]);

	CFwRulePtr pRule = CFwRulePtr(new CFwRule(ID));
	pRule->FromVariant(Rule);

	RETURN(pRule);
}

STATUS CNetworkManager::DelFwRule(const CFwRulePtr& pRule)
{
	return theCore->DelFwRule(pRule->GetGuid());
}

void CNetworkManager::UpdateDnsCache()
{
	auto Ret = theCore->GetDnsCache();
	if (Ret.IsError())
		return;

	XVariant& Cache = Ret.GetValue();

	QMap<quint64, CDnsCacheEntryPtr> OldCache = m_DnsCache;

	Cache.ReadRawList([&](const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		quint64 CacheRef = Data[API_V_DNS_CACHE_REF];

		CDnsCacheEntryPtr pEntry = OldCache.take(CacheRef);
		if (!pEntry) {
			pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry());
			m_DnsCache.insert(CacheRef, pEntry);
		}

		pEntry->FromVariant(Data);
	});

	foreach(const quint64 & CacheRef, OldCache.keys())
		m_DnsCache.remove(CacheRef);
}