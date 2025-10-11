#pragma once
#include "../Programs/ProgramID.h"

enum class ENetProtocols
{
	eAny = 0,
	eWeb,
	eTCP,
	eTCP_Server,
	eTCP_Client,
	eUDP,
};

class CTrafficEntry : public QObject
{
	Q_OBJECT
	TRACK_OBJECT(CTrafficEntry)
public:
	CTrafficEntry(QObject* parent = NULL);

	void SetHostName(const QString& HostName)	{ m_HostName = HostName; }
	QString GetHostName() const			{ return m_HostName; }
	void SetIpAddress(const QString& IpAddress);
	QString GetIpAddress() const			{ return m_IpAddress; }

	enum ENetType
	{
		eNoArea = 0,
		eInternet = 1,
		eLocalArea = 2,
		eBroadcast = 4,
		eMulticast = 8,
		eLocalHost = 16,
		eLocalAreaEx = eLocalArea | eBroadcast | eMulticast
	};

	ENetType GetNetType() const			{ return m_Type; }
	static ENetType GetNetType(const QHostAddress& address);

	quint64 GetLastActivity() const		{ return m_LastActivity; }
	quint64 GetUploadTotal() const		{ return m_UploadTotal; }
	quint64 GetDownloadTotal() const	{ return m_DownloadTotal; }

	void Reset();
	void Merge(const QSharedPointer<CTrafficEntry>& pOther);

	void FromVariant(const class QtVariant& TrafficEntry);

protected:

	QString						m_HostName;
	QString						m_IpAddress;
	ENetType					m_Type = eInternet;

	quint64						m_LastActivity = 0;
	quint64						m_UploadTotal = 0;
	quint64						m_DownloadTotal = 0;
};

typedef QSharedPointer<CTrafficEntry> CTrafficEntryPtr;

quint64 CTrafficEntry__LoadList(QHash<QString, CTrafficEntryPtr>& List, QHash<QHostAddress, QSet<CTrafficEntryPtr>>& Unresolved, const class QtVariant& TrafficList);