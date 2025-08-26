#include "pch.h"
#include "ProcessTraceView.h"
#include "../Core/PrivacyCore.h"

CProcessTraceView::CProcessTraceView(QWidget *parent)
	:CTraceView(new CProcessTraceModel(), parent)
{
	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessTraceView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbRole = new QComboBox();
	m_pCmbRole->addItem(QIcon(":/Icons/EFence.png"), tr("Any Role"), (qint32)EExecLogRole::eUndefined);
	m_pCmbRole->addItem(QIcon(":/Icons/Export.png"), tr("Actor"), (qint32)EExecLogRole::eActor);
	m_pCmbRole->addItem(QIcon(":/Icons/Entry.png"), tr("Target"), (qint32)EExecLogRole::eTarget);
	connect(m_pCmbRole, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbRole);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Status"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);

	m_pCmbOperation = new QComboBox();
	m_pCmbOperation->addItem(QIcon(":/Icons/RunFilter.png"), tr("Any Operation"), (qint32)EExecLogType::eUnknown);
	m_pCmbOperation->addItem(QIcon(":/Icons/Run.png"), tr("Process Start"), (qint32)EExecLogType::eProcessStarted);
	m_pCmbOperation->addItem(QIcon(":/Icons/Dll.png"), tr("Image Load"), (qint32)EExecLogType::eImageLoad);
	m_pCmbOperation->addItem(QIcon(":/Icons/Cmd.png"), tr("Process Access"), (qint32)EExecLogType::eProcessAccess | (qint32)EExecLogType::eThreadAccess);
	connect(m_pCmbOperation, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbOperation);

	m_pToolBar->addSeparator();

	m_pBtnScroll = new QToolButton();
	m_pBtnScroll->setIcon(QIcon(":/Icons/Scroll.png"));
	m_pBtnScroll->setCheckable(true);
	m_pBtnScroll->setToolTip(tr("Auto Scroll"));
	m_pBtnScroll->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnScroll);

	m_pBtnHold = new QToolButton();
	m_pBtnHold->setIcon(QIcon(":/Icons/Hold.png"));
	m_pBtnHold->setCheckable(true);
	m_pBtnHold->setToolTip(tr("Hold updates"));
	m_pBtnHold->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnHold);

	m_pToolBar->addSeparator();
	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear Trace Log"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(OnClearTraceLog()));
	m_pToolBar->addWidget(m_pBtnClear);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);
}

CProcessTraceView::~CProcessTraceView()
{
	theConf->SetBlob("MainWindow/ProcessTraceView_Columns", m_pTreeView->saveState());
}

void CProcessTraceView::Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid)
{
	CTraceView::Sync(Log, Programs, Services, EnclaveGuid);
}

void CProcessTraceView::UpdateFilter()
{
	((CProcessTraceModel*)m_pItemModel)->SetFilter((EExecLogRole)m_pCmbRole->currentData().toInt(), (EEventStatus)m_pCmbAction->currentData().toInt(), (EExecLogType)m_pCmbOperation->currentData().toInt());
	m_FullRefresh = true;
}

void CProcessTraceView::OnClearTraceLog()
{
	ClearTraceLog(ETraceLogs::eExecLog);
}