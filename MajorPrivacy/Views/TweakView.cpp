#include "pch.h"
#include "TweakView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Tweaks/TweakManager.h"

CTweakView::CTweakView(QWidget *parent)
	:CPanelViewEx<CTweakModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/TweakView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CTweakView::~CTweakView()
{
	theConf->SetBlob("MainWindow/TweakView_Columns", m_pTreeView->saveState());
}

void CTweakView::Sync(const CTweakPtr& pRoot)
{
	QList<QModelIndex> Added = m_pItemModel->Sync(pRoot);

	QTimer::singleShot(10, this, [this, Added]() {
		foreach(const QModelIndex& Index, Added) {
			if(m_pItemModel->GetItem(Index)->GetType() == ETweakType::eGroup)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		}
	});
}

void CTweakView::OnDoubleClicked(const QModelIndex& Index)
{
	STATUS Status;
	CTweakPtr pTweak = m_pItemModel->GetItem(Index);
	// todo.....
}

void CTweakView::OnCheckChanged(const QModelIndex& Index, bool State)
{
	STATUS Status;
	CTweakPtr pTweak = m_pItemModel->GetItem(Index);
	if(State)
		Status = theCore->Tweaks()->ApplyTweak(pTweak);
	else
		Status = theCore->Tweaks()->UndoTweak(pTweak);
	//todo: show error Status
}

void CTweakView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
