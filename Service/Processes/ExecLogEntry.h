#pragma once
#include "../Programs/TraceLogEntry.h"

#include "../../Library/API/PrivacyDefs.h"

class CExecLogEntry: public CTraceLogEntry
{
	//TRACK_OBJECT(CExecLogEntry)
public:
#ifdef DEF_USE_POOL
	CExecLogEntry(FW::AbstractMemPool* pMem, const CFlexGuid& EnclaveGuid, EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, const CFlexGuid& OtherEnclave, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, const struct SVerifierInfo* pInfo, uint32 AccessMask = 0);
#else
	CExecLogEntry(const CFlexGuid& EnclaveGuid, EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, const CFlexGuid& OtherEnclave, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, const struct SVerifierInfo* pInfo, uint32 AccessMask = 0);
#endif

	EEventStatus GetStatus() const { return m_Status; }

	virtual void WriteVariant(StVariantWriter& Entry) const;

protected:

	EExecLogRole			m_Role = EExecLogRole::eUndefined;
	EExecLogType			m_Type = EExecLogType::eUnknown;
	EEventStatus			m_Status = EEventStatus::eUndefined;
	uint64					m_MiscID = 0; // Image UID or program file UID
	CFlexGuid				m_OtherEnclave;
	
	uint32					m_StatusFlags = 0;

	KPH_VERIFY_AUTHORITY	m_PrivateAuthority = KphUntestedAuthority;
	uint32					m_SignLevel = 0;
	uint32					m_SignPolicyBits = 0;

	USignatures				m_FoundSignatures = {0};
	USignatures				m_AllowedSignatures = {0};

	uint32					m_AccessMask = 0;

};

#ifdef DEF_USE_POOL
typedef FW::SharedPtr<CExecLogEntry> CExecLogEntryPtr;
#else
typedef std::shared_ptr<CExecLogEntry> CExecLogEntryPtr;
#endif
