#pragma once

#include "../mischelpers_global.h"

class MISCHELPERS_EXPORT CPanelView : public QWidget
{
	Q_OBJECT
public:
	CPanelView(QWidget *parent = 0);
	virtual ~CPanelView();

	static void					SetSimpleFormat(bool bSimple) { m_SimpleFormat = bSimple; }
	static void					SetDarkMode(bool bDarkMode) { m_DarkMode = bDarkMode; }
	static void					SetMaxCellWidth(int iMaxWidth) { m_MaxCellWidth = iMaxWidth; }
	static void					SetCellSeparator(const QString& Sep) { m_CellSeparator = Sep; }

	virtual QList<QStringList>	DumpPanel();

	static QString				m_CopyCell;
	static QString				m_CopyRow;
	static QString				m_CopyPanel;

protected slots:
	virtual void				OnMenu(const QPoint& Point);

	virtual void				OnCopyCell();
	virtual void				OnCopyRow();
	virtual void				OnCopyPanel();


	virtual QTreeView*			GetView() = 0;
	virtual QAbstractItemModel* GetModel() = 0;
	virtual QModelIndex			MapToSource(const QModelIndex& Model) { return Model; }
	static QModelIndexList		MapToSource(QModelIndexList Indexes, QSortFilterProxyModel* pProxy)	{ 
		for (int i = 0; i < Indexes.count(); i++)
			Indexes[i] = pProxy->mapToSource(Indexes[i]);
		return Indexes;
	}

	virtual void				AddPanelItemsToMenu(bool bAddSeparator = true);

	virtual void				ForceColumn(int column, bool bSet = true) { if (bSet) m_ForcedColumns.insert(column); else m_ForcedColumns.remove(column); }

	virtual QStringList			CopyHeader();
	virtual QStringList			CopyRow(const QModelIndex& ModelIndex, int Level = 0);
	virtual void				RecursiveCopyPanel(const QModelIndex& ModelIndex, QList<QStringList>& Rows, int Level = 0);

protected:
	void						UpdateCopyMenu();
	void						AddCopyMenu(QMenu* pMenu, bool bAddSeparator = true);
	void						FormatAndCopy(QList<QStringList> Rows, bool Header = true);

	QMenu*						m_pMenu;

	QAction*					m_pCopyCell;
	QAction*					m_pCopyRow;
	QAction*					m_pCopyPanel;

	//bool						m_CopyAll;
	QSet<int>					m_ForcedColumns;
	static bool					m_SimpleFormat;
	static bool					m_DarkMode;
	static int					m_MaxCellWidth;
	static QString				m_CellSeparator;
};

template <class T, class B = CPanelView>
class CPanelWidget : public B
{
public:
	CPanelWidget(QWidget *parent = 0) : B(parent)
	{
		m_pMainLayout = new QVBoxLayout();
		m_pMainLayout->setContentsMargins(0,0,0,0);
		this->setLayout(m_pMainLayout);

		m_pTreeList = new T();
		m_pTreeList->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pTreeList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
		m_pMainLayout->addWidget(m_pTreeList);
		m_pTreeList->setMinimumHeight(50);
		AddPanelItemsToMenu();

		m_pLastAction = m_pMenu->actions()[0];
	}

	virtual QMenu*				GetMenu()	{ return m_pMenu; }
	virtual void				AddAction(QAction* pAction) { m_pMenu->insertAction(m_pLastAction, pAction); }

	virtual T*					GetTree()	{ return m_pTreeList; }
	virtual QTreeView*			GetView()	{ return m_pTreeList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pTreeList->model(); }

	virtual QVBoxLayout*		GetLayout()	{ return m_pMainLayout; }

protected:
	QVBoxLayout*			m_pMainLayout;

	T*						m_pTreeList;

	QAction*				m_pLastAction;
};

#include "TreeWidgetEx.h"
#include "Finder.h"

class MISCHELPERS_EXPORT CPanelWidgetEx : public CPanelWidget<QTreeWidgetEx>
{
	Q_OBJECT

public:
	CPanelWidgetEx(QWidget *parent = 0) : CPanelWidget<QTreeWidgetEx>(parent) 
	{
		m_pFinder = new CFinder(NULL, this, CFinder::eRegExp | CFinder::eCaseSens);
		m_pMainLayout->addWidget(m_pFinder);
		QObject::connect(m_pFinder, SIGNAL(SetFilter(const QString&, int, int)), this, SLOT(SetFilter(const QString&, int, int)));
	}

	static void ApplyFilter(QTreeWidgetEx* pTree, QTreeWidgetItem* pItem, const QRegularExpression* Exp/*, bool bHighLight = false, int Col = -1*/)
	{
		for (int j = 0; j < pTree->columnCount(); j++) {
			pItem->setForeground(j, (m_DarkMode && Exp && pItem->text(j).contains(*Exp)) ? Qt::yellow : pTree->palette().color(QPalette::WindowText));
			pItem->setBackground(j, (!m_DarkMode && Exp && pItem->text(j).contains(*Exp)) ? Qt::yellow : pTree->palette().color(QPalette::Base));
		}

		for (int i = 0; i < pItem->childCount(); i++)
			ApplyFilter(pTree, pItem->child(i), Exp/*, bHighLight, Col*/);
	}

	static void ApplyFilter(QTreeWidgetEx* pTree, const QRegularExpression* Exp/*, bool bHighLight = false, int Col = -1*/)
	{
		for (int i = 0; i < pTree->topLevelItemCount(); i++)
			ApplyFilter(pTree, pTree->topLevelItem(i), Exp/*, bHighLight, Col*/);
	}

private slots:
	void SetFilter(const QString& Exp, int iOptions, int Col = -1) // -1 = any
	{
		QScopedPointer<QRegularExpression> pRegExp;
		if (!Exp.isEmpty()) {
			QString ExpStr = ((iOptions & CFinder::eRegExp) == 0) ? Exp : (".*" + QRegularExpression::escape(Exp) + ".*");
			pRegExp.reset(new QRegularExpression(ExpStr, (iOptions & CFinder::eCaseSens) != 0 ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption));
		}
		ApplyFilter(m_pTreeList, pRegExp.data());
	}

private:
	
	CFinder*				m_pFinder;
};

#include "TreeViewEx.h"
#include "SortFilterProxyModel.h"

/*class CPanelViewEx: public CPanelWidget<QTreeViewEx>
{
public:
	CPanelViewEx(QAbstractItemModel* pModel, QWidget *parent = 0) : CPanelWidget<QTreeViewEx>(parent)
	{
		m_pModel = pModel;

		m_pSortProxy = new CSortFilterProxyModel(this);
		m_pSortProxy->setSortRole(Qt::EditRole);
		m_pSortProxy->setSourceModel(m_pModel);
		m_pSortProxy->setDynamicSortFilter(true);

		m_pTreeList->setModel(m_pSortProxy);

		m_pTreeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
		QStyle* pStyle = QStyleFactory::create("windows");
		m_pTreeList->setStyle(pStyle);
#endif
		m_pTreeList->setExpandsOnDoubleClick(false);
		m_pTreeList->setSortingEnabled(true);

		m_pTreeList->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pTreeList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnMenu(const QPoint &)));

		m_pTreeList->setColumnReset(1);
		//connect(m_pTreeList, SIGNAL(ResetColumns()), m_pTreeList, SLOT(OnResetColumns()));
		//connect(m_pBoxTree, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

		m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeList, m_pSortProxy));
	}

protected:
	QAbstractItemModel*		m_pModel;
	QSortFilterProxyModel*	m_pSortProxy;
};*/



class MISCHELPERS_EXPORT CPanelViewX : public CPanelView
{
	Q_OBJECT
public:
	CPanelViewX(QWidget* parent = 0) : CPanelView(parent) {}

signals:
	void						CurrentChanged();
	void						SelectionChanged();

protected slots:
	virtual void				OnDoubleClicked(const QModelIndex& Index) {}
	virtual void				OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous) = 0;
	virtual void				OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected) = 0;
};

template <typename M, class T = QTreeViewEx>
class CPanelViewEx: public CPanelViewX
{
public:
	CPanelViewEx(QWidget *parent = 0) : CPanelViewX(parent)
	{
		m_pMainLayout = new QVBoxLayout();
		m_pMainLayout->setContentsMargins(0, 0, 0, 0);
		this->setLayout(m_pMainLayout);

		m_pTreeView = new QTreeViewEx();
		m_pMainLayout->addWidget(m_pTreeView);

		m_pItemModel = new M();
		//connect(m_pItemModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
		//connect(m_pItemModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

		//m_pItemModel->SetTree(false);

		m_pSortProxy = new CSortFilterProxyModel(this);
		m_pSortProxy->setSortRole(Qt::EditRole);
		m_pSortProxy->setSourceModel(m_pItemModel);
		m_pSortProxy->setDynamicSortFilter(true);

		//m_pTreeView->setItemDelegate(theGUI->GetItemDelegate());
		m_pTreeView->setMinimumHeight(50);

		m_pTreeView->setModel(m_pSortProxy);

		m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
		m_pTreeView->setSortingEnabled(true);

		m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pTreeView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

		connect(m_pTreeView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
		connect(m_pTreeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
		connect(m_pTreeView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));

		m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));
	}

	typename M::ItemType		GetCurrentItem()	{ return m_CurrentItem; }
	QList<typename M::ItemType>	GetSelectedItem()	{ return m_SelectedItems; }

protected:

	void OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous) override
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
		typename M::ItemType pItem = m_pItemModel->GetItem(ModelIndex);

		m_CurrentItem = pItem;
		emit CurrentChanged();
	}

	void OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected) override
	{
		QList<typename M::ItemType> List;
		foreach(const QModelIndex& Index, m_pTreeView->selectedRows())
		{
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			typename M::ItemType pItem = m_pItemModel->GetItem(ModelIndex);
			if (!pItem)
				continue;
			List.append(pItem);
		}

		m_SelectedItems = List;
		emit SelectionChanged();
	}

	QTreeView*					GetView()			{ return m_pTreeView; }
	QAbstractItemModel*			GetModel()			{ return m_pSortProxy; }
	
	QVBoxLayout*				m_pMainLayout;

	T*							m_pTreeView;
	M*							m_pItemModel;
	QSortFilterProxyModel*		m_pSortProxy;

	typename M::ItemType		m_CurrentItem;
	QList<typename M::ItemType>	m_SelectedItems;
};