#include "pch.h"
#include "ProcessModel.h"
#include "../Core/PrivacyCore.h"
#include "../../MiscHelpers/Common/Common.h"


CProcessModel::CProcessModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bTree = true;
	m_bUseIcons = true;
}

CProcessModel::~CProcessModel()
{
}

QList<QVariant> CProcessModel::MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList)
{
	QList<QVariant> Path;

	quint64 ParentID = pProcess->GetParentId();
	CProcessPtr pParent = ProcessList.value(ParentID);

	if (!pParent.isNull())
	{
		Path = MakeProcPath(pParent, ProcessList);
		Path.append(ParentID);
	}

	return Path;
}

bool CProcessModel::TestProcPath(const QList<QVariant>& Path, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, int Index)
{
	quint64 ParentID = pProcess->GetParentId();
	CProcessPtr pParent = ProcessList.value(ParentID);

	if (!pParent.isNull())
	{
		if(Index >= Path.size() || Path[Path.size() - Index - 1] != ParentID)
			return false;

		return TestProcPath(Path, pParent, ProcessList, Index + 1);
	}

	return Path.size() == Index;
}

QSet<quint64> CProcessModel::Sync(QMap<quint64, CProcessPtr> ProcessList)
{
	QSet<quint64> Added;
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	bool bShow32 = theConf->GetBool("Options/Show32", true);
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);
	bool bShowMaxThread = theConf->GetBool("Options/ShowMaxThread", false);
	int iHighlightMax = theConf->GetInt("Options/HighLoadHighlightCount", 5);
	time_t curTime = GetTime();

	QVector<QList<QPair<quint64, SProcessNode*> > > Highlights;
	if(iHighlightMax > 0)
		Highlights.resize(columnCount());

	foreach (const CProcessPtr& pProcess, ProcessList)
	{
		quint64 ID = pProcess->GetProcessId();

		QModelIndex Index;
		
		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SProcessNode* pNode = I != Old.end() ? static_cast<SProcessNode*>(I.value()) : NULL;
		if(!pNode || (m_bTree ? !TestProcPath(pNode->Path, pProcess, ProcessList) : !pNode->Path.isEmpty()))
		{
			pNode = static_cast<SProcessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if (m_bTree)
				pNode->Path = MakeProcPath(pProcess, ProcessList);
			pNode->pProcess = pProcess;
			New[pNode->Path].append(pNode);
			Added.insert(ID);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
		
		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull() && !pProcess->GetIcon().isNull())
		{
			pNode->Icon = pProcess->GetIcon();
			Changed = 1;
		}

		for(int section = 0; section < columnCount(); section++)
		{
			//if (!m_Columns.contains(section)) // todo xxx
			//	continue; // ignore columns which are hidden

			quint64 CurIntValue = -1;

			QVariant Value;
			switch(section)
			{
				case eProcess:				Value = pProcess->GetName(); break;
				case ePID:					Value = pProcess->GetProcessId(); break;
				case eParentPID:			Value = pProcess->GetParentId(); break;
			}

			SProcessNode::SValue& ColValue = pNode->Values[section];

			bool bChanged = false;
			if (CurIntValue == -1)
			{
				CurIntValue = 0;
				bChanged = (ColValue.Raw != Value);
			}
			else if (ColValue.Raw.isNull())
				bChanged = true;

			if (bChanged)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				/*switch(section)
				{
					case ePID:				if (Value.toLongLong() < 0) ColValue.Formatted = ""; break;
				}*/
			}


			if(!Highlights.isEmpty() && CurIntValue != 0)
			{
				QList<QPair<quint64, SProcessNode*> >& List = Highlights[section];

				if (List.isEmpty() || List.last().first == CurIntValue)
					List.append(qMakePair(CurIntValue, pNode));
				else if (List.last().first < CurIntValue || List.count() < iHighlightMax)
				{
					int i = 0;
					for (; i < List.size(); i++)
					{
						if (List.at(i).first < CurIntValue)
							break;
					}
					List.insert(i, qMakePair(CurIntValue, pNode));

					while (List.count() > iHighlightMax)
						List.removeLast();
				}
			}


			if(State != (Changed != 0))
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));
	}

	if (!Highlights.isEmpty())
	{
		for (int section = eProcess; section < columnCount(); section++)
		{
			QList<QPair<quint64, SProcessNode*> >& List = Highlights[section];

			for (int i = 0; i < List.size(); i++)
				List.at(i).second->Bold.insert(section);
		}
	}

	CTreeItemModel::Sync(New, Old);

	//for (QMap<QList<QVariant>, QList<STreeNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
	//{
	//	foreach(STreeNode* pNode, I.value())
	//	{
	//		QModelIndex Index = Find(m_Root, pNode);
	//		if(Index.isValid())
	//			AllIndexes.append(Index);
	//	}
	//}
	//m_AllIndexes = AllIndexes;

	return Added;
}

QVariant CProcessModel::NodeData(STreeNode* pNode, int role, int section) const
{
    switch(role)
	{
		case Qt::FontRole:
		{
			SProcessNode* pProcessNode = static_cast<SProcessNode*>(pNode);
			if (pProcessNode->Bold.contains(section))
			{
				QFont fnt;
				fnt.setBold(true);
				return fnt;
			}
			break;
		}
	}

	return CTreeItemModel::NodeData(pNode, role, section);
}

CProcessPtr CProcessModel::GetItem(const QModelIndex &index) const
{
	if (!index.isValid())
        return CProcessPtr();

	SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pProcess;
}

int CProcessModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return GetColumHeader(section);
    return QVariant();
}

QString CProcessModel::GetColumHeader(int section) const
{
	switch(section)
	{
		case eProcess:				return tr("Process");
		case ePID:					return tr("PID");
		case eParentPID:			return tr("Parent PID");
		case eStaus:				return tr("Status");
	}
	return "";
}
