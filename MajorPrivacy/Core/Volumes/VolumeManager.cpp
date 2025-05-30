#include "pch.h"
#include "VolumeManager.h"
#include "Core/PrivacyCore.h"
#include "Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "./Common/QtVariant.h"
#include "../../MiscHelpers/Common/Common.h"
#include "Core/Access/AccessManager.h"

CVolumeManager::CVolumeManager(QObject* parent)
 : QObject(parent)
{
	LoadVolumes();
}

CVolumeManager::~CVolumeManager()
{
	SaveVolumes();
}

void CVolumeManager::Update()
{
	auto Ret = theCore->GetVolumes();
	if (Ret.IsError())
		return;

	QtVariant& Volumes = Ret.GetValue();

	QMap<QString, CVolumePtr> OldMap = m_List;

	QtVariantReader(Volumes).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Volume = *(QtVariant*)&vData;

		QString ImagePath = QtVariantReader(Volume).Find(API_V_VOL_PATH).AsQStr().toLower();

		CVolumePtr pVolume = OldMap.take(ImagePath);
		if (pVolume.isNull())
		{
			pVolume = CVolumePtr(new CVolume());
			m_List.insert(ImagePath, pVolume);
		}
		if (!pVolume->IsMounted())
			pVolume->SetMounted(true);
		
		pVolume->FromVariant(Volume);
	});

	foreach(const CVolumePtr& pVolume, OldMap) {
		if (pVolume->IsMounted())
			pVolume->SetMounted(false);
	}
}

void CVolumeManager::AddVolume(const QString& Path)
{
	CVolumePtr pVolume = CVolumePtr(new CVolume());
	pVolume->SetImagePath(Path);
	m_List.insert(Path.toLower(), pVolume);
}

void CVolumeManager::RemoveVolume(const QString& Path)
{
	m_List.remove(Path.toLower());
}

bool CVolumeManager::LoadVolumes()
{
	foreach(const QString & Key, theConf->ListKeys("SecureVolumes"))
	{
		QString Data = theConf->GetValue("SecureVolumes/" + Key).toString();
		QStringList List = SplitStr(Data, "|");

		CVolumePtr pVolume = CVolumePtr(new CVolume());
		pVolume->SetImagePath(List[0].replace("/","\\"));
		if(List.size() >= 2) pVolume->SetName(List[1]);

		m_List.insert(pVolume->GetImagePath().toLower(), pVolume);
	}

	return true;
}

bool CVolumeManager::SaveVolumes()
{
	foreach(const QString & Key, theConf->ListKeys("SecureVolumes"))
		theConf->DelValue("SecureVolumes/" + Key);

	for(auto pVolume: m_List)
	{
		QString Data;
		Data = pVolume->GetImagePath() + "|" + pVolume->GetName();
		theConf->SetValue("SecureVolumes/Volume_" + QCryptographicHash::hash(pVolume->GetImagePath().toLatin1(), QCryptographicHash::Sha256).mid(0,16).toHex(), Data);
	}

	return true;
}

void CVolumeManager::UpdateProtectedFolders()
{
	QMap<CVolumePtr, bool> Folders;

	for (auto pRule : theCore->AccessManager()->GetAccessRules()) 
	{
		if (pRule->GetType() != EAccessRuleType::eProtect || pRule->IsVolumeRule())
			continue;

		QString Path = pRule->GetPath();
		if(Path.startsWith("\\") || !Path.endsWith("*") || Path.contains("/"))
			continue;
		Path.truncate(Path.length() - 1);

		CVolumePtr& pVolume = m_List[Path.toLower()];
		if (pVolume.isNull())
		{
			pVolume = CVolumePtr(new CVolume());
			pVolume->SetImagePath(Path);
		}
		Folders[pVolume] |= pRule->IsEnabled();
	}

	for(auto I = Folders.begin(); I != Folders.end(); ++I)
		I.key()->SetStatus(I.value() ? CVolume::eSecFolder : CVolume::eFolder);
}