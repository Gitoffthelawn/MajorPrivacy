#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/ProgramLibrary.h"

struct SLibraryItem
{
	QVariant				ID;
	QVariant				Parent;
	CProgramFilePtr			pProg;
	CProgramLibraryPtr		pLibrary;
	SLibraryInfo			Info;
	int						Count = 0;
};

typedef QSharedPointer<SLibraryItem> SLibraryItemPtr;

typedef QPair<quint64, quint64> SLibraryKey;

class CLibraryModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CLibraryModel(QObject* parent = 0);
	~CLibraryModel();
	
	typedef SLibraryItemPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<SLibraryKey, SLibraryItemPtr>& List);
	
	SLibraryItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eTrustLevel,
		eStatus,
		eLastLoadTime,
		eNumber,
		eHash,
		eSigner,
		eModule,
		eCount
	};

protected:
	struct SLibraryNode : STreeNode
	{
		SLibraryNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		SLibraryItemPtr pItem;
		bool IsSignedFile = false;
		bool IsSignedCert = false;
	};

	virtual QVariant	NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SLibraryNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
