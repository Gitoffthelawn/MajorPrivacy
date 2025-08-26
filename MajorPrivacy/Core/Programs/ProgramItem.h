#pragma once

//#include "../Process.h"
#include "ProgramID.h"
#include "./Common/QtFlexGuid.h"

struct SProgramStats
{
	int				ProgramsCount = 0;
	int				ServicesCount = 0;
	int				AppsCount = 0;
	int				GroupCount = 0;

	int				ProcessCount = 0;
	uint64			LastExecution = 0;
	int				ProgRuleCount = 0;
	int				ProgRuleTotal = 0;

	int				ResRuleCount = 0;
	int				ResRuleTotal = 0;
	uint32			AccessCount = 0;
	uint32			HandleCount = 0;

	int				FwRuleCount = 0;
	int				FwRuleTotal = 0;
	int				SocketCount = 0;

	uint64			LastNetActivity = 0;

	uint64			LastFwAllowed = 0;
	uint64			LastFwBlocked = 0;

	uint64			Upload = 0;
	uint64			Download = 0;
	uint64			Uploaded = 0;
	uint64			Downloaded = 0;
};

class CProgramItem: public QObject
{
	Q_OBJECT
public:
	CProgramItem(QObject* parent = nullptr);

	virtual EProgramType GetType() const = 0;
	virtual QString GetTypeStr() const;

	void SetUID(quint64 UID) 						{ m_UID = UID; }
	quint64 GetUID() const							{ return m_UID; }
	void SetID(const CProgramID& ID) 				{ m_ID = ID; }
	const CProgramID& GetID() const					{ return m_ID; }

	virtual void SetName(const QString& Name)		{ QWriteLocker Lock(&m_Mutex); m_Name = Name; }
	virtual QString GetName() const					{ QReadLocker Lock(&m_Mutex); return m_Name; }
	virtual QString GetNameEx() const				{ QReadLocker Lock(&m_Mutex); return m_Name; }
	virtual QString GetPublisher() const			{ return ""; }
	//virtual QStringList GetCategories() const		{ return m_Categories; }
	virtual QIcon DefaultIcon() const;
	virtual void SetIconFile(const QString& IconFile) { QWriteLocker Lock(&m_Mutex); m_IconFile = IconFile; Lock.unlock(); UpdateIconFile(); }
	virtual QString GetIconFile() const				{ QReadLocker Lock(&m_Mutex); return m_IconFile; }
	virtual QIcon GetIcon() const					{ QReadLocker Lock(&m_Mutex); return m_Icon; }
	virtual void SetInfo(const QString& Info)		{ m_Info = Info; }
	virtual QString GetInfo() const					{ QReadLocker Lock(&m_Mutex); return m_Info; }
	virtual QString GetPath() const					{ return ""; }

	virtual ETracePreset GetExecTrace() const		{ return m_ExecTrace; }
	virtual void SetExecTrace(ETracePreset Trace)	{ m_ExecTrace = Trace; }
	virtual ETracePreset GetResTrace() const		{ return m_ResTrace; }
	virtual void SetResTrace(ETracePreset Trace)	{ m_ResTrace = Trace; }
	virtual ETracePreset GetNetTrace() const		{ return m_NetTrace; }
	virtual void SetNetTrace(ETracePreset Trace)	{ m_NetTrace = Trace; }

	virtual ESavePreset GetSaveTrace() const		{ return m_SaveTrace; }
	virtual void SetSaveTrace(ESavePreset Trace)	{ m_SaveTrace = Trace; }


	virtual QList<QWeakPointer<QObject>> GetGroups() const	{ QReadLocker Lock(&m_Mutex); return m_Groups; }

	virtual int GetFwRuleCount() const				{ QReadLocker Lock(&m_Mutex); return m_FwRuleIDs.count(); }
	virtual QSet<QFlexGuid> GetFwRules() const		{ QReadLocker Lock(&m_Mutex); return m_FwRuleIDs; }

	virtual int GetProgRuleCount() const			{ QReadLocker Lock(&m_Mutex); return m_ProgRuleIDs.count(); }
	virtual QSet<QFlexGuid> GetProgRules() const	{ QReadLocker Lock(&m_Mutex); return m_ProgRuleIDs; }

	virtual int GetResRuleCount() const				{ QReadLocker Lock(&m_Mutex); return m_ResRuleIDs.count(); }
	virtual QSet<QFlexGuid> GetResRules() const		{ QReadLocker Lock(&m_Mutex); return m_ResRuleIDs; }

	virtual bool IsMissing() const					{ return m_IsMissing; }

	virtual void CountStats() = 0;
	virtual const SProgramStats* GetStats()			{ return &m_Stats; }

	virtual size_t GetLogMemUsage() const			{ return m_LogMemoryUsed; }

	virtual QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const QtVariant& Data);

protected:
	friend class CProgramManager;

	void SetIconFile();
	void UpdateIconFile();

	virtual void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const QtVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const QtVariant& Data);

	mutable QReadWriteLock m_Mutex;

	quint64								m_UID = 0;
	CProgramID							m_ID;
	QString								m_Name;
	//QStringList							m_Categories;
	QString								m_IconFile;
	QIcon								m_Icon;
	QString								m_Info;

	ETracePreset 						m_ExecTrace = ETracePreset::eDefault;
	ETracePreset 						m_ResTrace = ETracePreset::eDefault;
	ETracePreset 						m_NetTrace = ETracePreset::eDefault;
	ESavePreset 						m_SaveTrace = ESavePreset::eDefault;

	QList<QWeakPointer<QObject>>		m_Groups;

	QSet<QFlexGuid>						m_FwRuleIDs;
	QSet<QFlexGuid>						m_ProgRuleIDs;
	QSet<QFlexGuid>						m_ResRuleIDs;

	bool								m_IsMissing = false;

	SProgramStats						m_Stats;

	size_t								m_LogMemoryUsed = 0;		
};

typedef QSharedPointer<CProgramItem> CProgramItemPtr;
typedef QWeakPointer<CProgramItem> CProgramItemRef;

