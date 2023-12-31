#include "pch.h"
#include "FirewallRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Service/ServiceAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"

CFirewallRuleWnd::CFirewallRuleWnd(const CFwRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget* parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	//flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	setWindowFlags(flags);

	ui.setupUi(this);


	m_pRule = pRule;
	bool bNew = m_pRule->m_Guid.isEmpty();

	setWindowTitle(bNew ? tr("Create Firewall Rule") : tr("Edit Firewall Rule"));

	foreach(auto pItem, Items) 
	{
		CProgramFilePtr pFile = pItem.objectCast<CProgramFile>();
		CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
		CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
		if(!pFile && !pSvc && !pApp) // windows firewall only supports these types of identifiers
			continue;

		m_Items.append(pItem);
		ui.cmbProgram->addItem(pItem->GetNameEx());
	}

	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));

	connect(ui.radProfiles, SIGNAL(idToggled(int, bool)), this, SLOT(OnProfilesChanged()));
	connect(ui.radNICs, SIGNAL(idToggled(int, bool)), this, SLOT(OnInterfacesChanged()));

	connect(ui.cmbDirection, SIGNAL(currentIndexChanged(int)), this, SLOT(OnDirectionChanged()));

	connect(ui.cmbProtocol, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProtocolChanged()));
	connect(ui.cmbProtocol, SIGNAL(currentTextChanged(const QString&)), this, SLOT(OnProtocolChanged()));

	connect(ui.radLocalIP, SIGNAL(idToggled(int, bool)), this, SLOT(OnLocalIPChanged()));
	//connect(ui.btnLocalIPAdd, SIGNAL(clicked(bool)), this, SLOT(OnLocalIPAdd()));
	connect(ui.lstLocalIP, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnLocalIPEdit()));
	//connect(ui.lstLocalIP, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(OnLocalIPEdit()));
	connect(ui.btnLocalIPSet, SIGNAL(clicked(bool)), this, SLOT(OnLocalIPSet()));

	connect(ui.radRemoteIP, SIGNAL(idToggled(int, bool)), this, SLOT(OnRemoteIPChanged()));
	//connect(ui.btnRemoteIPAdd, SIGNAL(clicked(bool)), this, SLOT(OnRemoteIPAdd()));
	connect(ui.lstRemoteIP, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnRemoteIPEdit()));
	//connect(ui.lstRemoteIP, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(OnRemoteIPEdit()));
	connect(ui.btnRemoteIPSet, SIGNAL(clicked(bool)), this, SLOT(OnRemoteIPSet()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	ui.cmbAction->addItem(tr("Allow"), SVC_API_FW_ALLOW);
	ui.cmbAction->addItem(tr("Block"), SVC_API_FW_BLOCK);

	if(bNew)
		ui.cmbDirection->addItem(tr("Bidirectional"), SVC_API_FW_BIDIRECTIONAL);
	ui.cmbDirection->addItem(tr("Inbound"), SVC_API_FW_INBOUND);
	ui.cmbDirection->addItem(tr("Outbound"), SVC_API_FW_OUTBOUND);

	ui.cmbProtocol->addItem(tr("Any Protocol"), (quint32)EKnownProtocols::Any);
	for(auto I: g_KnownProtocols)
		ui.cmbProtocol->addItem(QString::fromStdWString(I.second), I.first);

	FixComboBoxEditing(ui.cmbGroup);
	FixComboBoxEditing(ui.cmbProtocol);
	FixComboBoxEditing(ui.cmbICMP);
	FixComboBoxEditing(ui.cmbLocalPorts);
	FixComboBoxEditing(ui.cmbRemotePorts);

	ui.cmbLocalIPSet->setVisible(false);
	ui.btnLocalIPSet->setVisible(false);

	ui.cmbRemoteIPSet->setVisible(false);
	ui.btnRemoteIPSet->setVisible(false);

	ui.cmbRemoteIPSet->addItem("LocalSubnet");
	ui.cmbRemoteIPSet->addItem("DNS");
	ui.cmbRemoteIPSet->addItem("DHCP");
	ui.cmbRemoteIPSet->addItem("WINS");
	ui.cmbRemoteIPSet->addItem("DefaultGateway");
	// win 8+
	ui.cmbRemoteIPSet->addItem("LocalIntranet");
	//ui.cmbRemoteIPSet->addItem("RemoteIntranet");
	ui.cmbRemoteIPSet->addItem("Internet");
	//ui.cmbRemoteIPSet->addItem("Ply2Renders");


	ui.txtName->setText(m_pRule->m_Name);
	// todo: load groups
	SetComboBoxValue(ui.cmbGroup, m_pRule->m_Grouping);
	ui.txtInfo->setPlainText(m_pRule->m_Description);

	ui.cmbProgram->setEditable(true);
	ui.cmbProgram->lineEdit()->setReadOnly(true);
	CProgramItemPtr pItem = theCore->Programs()->GetProgramByID(m_pRule->m_ProgramID);
	int Index = m_Items.indexOf(pItem);
	ui.cmbProgram->setCurrentIndex(Index);

	SetComboBoxValue(ui.cmbAction, m_pRule->m_Action);

	if (m_pRule->m_Profile.size() == 1 && m_pRule->m_Profile[0] == SVC_API_FW_ALL)
		ui.radProfileAll->setChecked(true);
	else
	{
		ui.radProfileCustom->setChecked(true);
	
		ui.chkPublic->setChecked(m_pRule->m_Profile.contains(SVC_API_FW_PUBLIC));
		ui.chkDomain->setChecked(m_pRule->m_Profile.contains(SVC_API_FW_DOMAIN));
		ui.chkPrivate->setChecked(m_pRule->m_Profile.contains(SVC_API_FW_PRIVATE));
	}

	if(m_pRule->m_Interface.size() == 1 && m_pRule->m_Interface[0] == SVC_API_FW_ANY)
		ui.radNicAll->setChecked(true);
	else
	{
		ui.radNicCustom->setChecked(true);

		ui.chkLAN->setChecked(m_pRule->m_Interface.contains(SVC_API_FW_LAN));
		ui.chkVPN->setChecked(m_pRule->m_Interface.contains(SVC_API_FW_REMOTEACCESS));
		ui.chkWiFi->setChecked(m_pRule->m_Interface.contains(SVC_API_FW_WIRELESS));
	}

	SetComboBoxValue(ui.cmbDirection, m_pRule->m_Direction);

	SetComboBoxValue(ui.cmbProtocol, m_pRule->m_Protocol);

	// ICMP set by OnProtocolChanged
		
	// Ports set by OnDirectionChanged and/or OnProtocolChanged via UpdatePorts

	auto SetAddresses = [&](const QStringList& Addresses, QListWidget* pList, QRadioButton* pAny, QRadioButton* pCustom) {
		if (Addresses.isEmpty() || (Addresses.size() == 1 && Addresses[0] == ""))
			pAny->setChecked(true);
		else
		{
			pCustom->setChecked(true);
			foreach(const QString& IP, Addresses)
			{
				QListWidgetItem* pIP = new QListWidgetItem(IP);
				pIP->setData(Qt::UserRole, IP);
				pList->addItem(pIP);
			}
		}
		QListWidgetItem* pNewIP = new QListWidgetItem(tr("[Add IP]"));
		pNewIP->setData(Qt::UserRole, "");
		pList->addItem(pNewIP);
	};

	SetAddresses(m_pRule->m_LocalAddresses, ui.lstLocalIP, ui.radLocalIPAny, ui.radLocalIPCustom);
	SetAddresses(m_pRule->m_RemoteAddresses, ui.lstRemoteIP, ui.radRemoteIPAny, ui.radRemoteIPCustom);

	OnProfilesChanged();
	OnInterfacesChanged();
	OnProtocolChanged();
	OnLocalIPChanged();
	OnRemoteIPChanged();
}

CFirewallRuleWnd::~CFirewallRuleWnd()
{
}

void CFirewallRuleWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

void CFirewallRuleWnd::OnSaveAndClose()
{
	if (!Save()) {
		QApplication::beep();
		return;
	}

	STATUS Status = theCore->Network()->SetFwRule(m_pRule);	
	if (Status.IsError()) {
		theGUI->CheckResults(QList<STATUS>() << Status, this);
		return;
	}
	accept();
}

bool CFirewallRuleWnd::Save()
{
	m_pRule->m_Name = ui.txtName->text();
	m_pRule->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString();
	m_pRule->m_Description = ui.txtInfo->toPlainText();

	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) 
		return false;

	CProgramItemPtr pItem = m_Items[Index];
	if (m_pRule->m_ProgramID != pItem->GetID()) {
		m_pRule->m_BinaryPath = pItem->GetPath();
		CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
		m_pRule->m_ServiceTag = pSvc ? pSvc->GetSvcTag() : "";
		CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
		m_pRule->m_AppContainerSid = pApp ? pApp->GetAppSid() : "";
	}

	m_pRule->m_Action = GetComboBoxValue(ui.cmbAction).toString();

	if (!ui.radProfileCustom->isChecked())
		m_pRule->m_Profile = QStringList() << SVC_API_FW_ALL;
	else
	{
		m_pRule->m_Profile.clear();
		if(ui.chkPublic->isChecked())
			m_pRule->m_Profile.append(SVC_API_FW_PUBLIC);
		if(ui.chkDomain->isChecked())
			m_pRule->m_Profile.append(SVC_API_FW_DOMAIN);
		if(ui.chkPrivate->isChecked())
			m_pRule->m_Profile.append(SVC_API_FW_PRIVATE);
	}

	if (!ui.radNicCustom->isChecked())
		m_pRule->m_Interface = QStringList() << SVC_API_FW_ANY;
	else
	{
		m_pRule->m_Interface.clear();
		if(ui.chkLAN->isChecked())
			m_pRule->m_Interface.append(SVC_API_FW_LAN);
		if(ui.chkVPN->isChecked())
			m_pRule->m_Interface.append(SVC_API_FW_REMOTEACCESS);
		if(ui.chkWiFi->isChecked())
			m_pRule->m_Interface.append(SVC_API_FW_WIRELESS);
	}

	m_pRule->m_Direction = GetComboBoxValue(ui.cmbDirection).toString();

	m_pRule->m_Protocol = GetComboBoxValue(ui.cmbProtocol).toUInt();

	bool bICMP = (m_pRule->m_Protocol == (quint32)EKnownProtocols::ICMP || m_pRule->m_Protocol == (quint32)EKnownProtocols::ICMPv6);
	if (!bICMP)
		m_pRule->m_IcmpTypesAndCodes.clear();
	else
	{
		if (ui.cmbICMP->currentIndex() != -1)
			m_pRule->m_IcmpTypesAndCodes = QStringList() << ui.cmbICMP->currentData().toString();
		else
			m_pRule->m_IcmpTypesAndCodes = SplitStr(ui.cmbICMP->currentText(), ",");
	}

	bool bPorts = (m_pRule->m_Protocol == (quint32)EKnownProtocols::TCP || m_pRule->m_Protocol == (quint32)EKnownProtocols::UDP);

	auto GetPorts = [&](QComboBox* pBox)
	{
		if (pBox->currentIndex() != -1) {
			QStringList List;
			if (!pBox->currentData().isNull())
				List.append(pBox->currentData().toString());
			return List;
		}
		return SplitStr(pBox->currentText(), ",");
	};

	m_pRule->m_LocalPorts = GetPorts(ui.cmbLocalPorts);
	m_pRule->m_RemotePorts = GetPorts(ui.cmbRemotePorts);

	auto GetAddresses = [&](QListWidget* pList, QRadioButton* pAny, QRadioButton* pCustom) {
		if (pAny->isChecked())
			return QStringList();
		QStringList Addresses;
		for (int i = 0; i < pList->count(); i++) {
			QListWidgetItem* pItem = pList->item(i);
			QString Address = pItem->data(Qt::UserRole).toString();
			if (!Address.isEmpty())
				Addresses.append(Address);
		}
		return Addresses;
	};

	m_pRule->m_LocalAddresses = GetAddresses(ui.lstLocalIP, ui.radLocalIPAny, ui.radLocalIPCustom);
	m_pRule->m_RemoteAddresses = GetAddresses(ui.lstRemoteIP, ui.radRemoteIPAny, ui.radRemoteIPCustom);

	return true;
}

void CFirewallRuleWnd::OnProgramChanged()
{
	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) return;

	CProgramItemPtr pItem = m_Items[Index];
	//if (pItem) ui.cmbProgram->setCurrentText(pItem->GetName());
	CProgramID ID = pItem->GetID();
	ui.txtPath->setText(QString::fromStdWString(NtPathToDosPath(pItem->GetPath().toStdWString())));
	CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
	ui.txtService->setText(pSvc ? pSvc->GetSvcTag() : "");
	//ui.txtService->setToolTip(pSvc ? pSvc->GetName() : "");
	CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
	ui.txtApp->setText(pApp ? pApp->GetAppSid() : "");
	//ui.txtApp->setToolTip(pApp ? pApp->GetContainerName() : "");
}

void CFirewallRuleWnd::OnProfilesChanged()
{
	ui.chkPublic->setEnabled(ui.radProfileCustom->isChecked());
	ui.chkDomain->setEnabled(ui.radProfileCustom->isChecked());
	ui.chkPrivate->setEnabled(ui.radProfileCustom->isChecked());
}

void CFirewallRuleWnd::OnInterfacesChanged()
{
	ui.chkLAN->setEnabled(ui.radNicCustom->isChecked());
	ui.chkVPN->setEnabled(ui.radNicCustom->isChecked());
	ui.chkWiFi->setEnabled(ui.radNicCustom->isChecked());
}

void CFirewallRuleWnd::OnDirectionChanged()
{
	quint32 Protocol = GetComboBoxValue(ui.cmbProtocol).toUInt();
	bool bPorts = (Protocol == (quint32)EKnownProtocols::TCP || Protocol == (quint32)EKnownProtocols::UDP);

	if(bPorts)
		UpdatePorts();
}

void CFirewallRuleWnd::OnProtocolChanged()
{
	quint32 Protocol = GetComboBoxValue(ui.cmbProtocol).toUInt();

	bool bICMP = (Protocol == (quint32)EKnownProtocols::ICMP || Protocol == (quint32)EKnownProtocols::ICMPv6);
	bool bPorts = (Protocol == (quint32)EKnownProtocols::TCP || Protocol == (quint32)EKnownProtocols::UDP);

	ui.lblICMP->setVisible(bICMP);
	ui.cmbICMP->setVisible(bICMP);
	ui.lblLocalPorts->setVisible(bPorts);
	ui.cmbLocalPorts->setVisible(bPorts);
	ui.cmbRemotePorts->setVisible(bPorts);
	ui.lblRemotePorts->setVisible(bPorts);

	if (bICMP)
	{
		ui.cmbICMP->clear();
		ui.cmbICMP->addItem(tr("All Types"), "*");
		for(auto I: (Protocol == (quint32)EKnownProtocols::ICMPv6) ? g_KnownIcmp6Types : g_KnownIcmp4Types)
			ui.cmbICMP->addItem(QString("%1:* (%2)").arg(I.first).arg(QString::fromStdWString(I.second)), QString("%1:*").arg(I.first));
		ui.cmbICMP->addItem("3:4 (Type 3, Code 4)", "3:4");

		int Pos = m_pRule->m_IcmpTypesAndCodes.count() == 1 ? ui.cmbICMP->findData(m_pRule->m_IcmpTypesAndCodes[0]) : -1;
		ui.cmbICMP->setCurrentIndex(Pos);
		if (Pos == -1)
			ui.cmbICMP->setCurrentText(m_pRule->m_IcmpTypesAndCodes.join(", "));
	}

	if(bPorts)
		UpdatePorts();
}

void CFirewallRuleWnd::UpdatePorts()
{
	ui.cmbLocalPorts->clear();
	ui.cmbRemotePorts->clear();

	QString Direction = ui.cmbDirection->currentData().toString();
	quint32 Protocol = GetComboBoxValue(ui.cmbProtocol).toUInt();

	ui.cmbLocalPorts->addItem("Any Port");
	ui.cmbRemotePorts->addItem("Any Port");

	if (Direction == SVC_API_FW_OUTBOUND)
	{
		if (Protocol == (uint32)EKnownProtocols::TCP)
		{
			ui.cmbRemotePorts->addItem("IPHTTPSOut", "IPHTTPSOut");
		}
	}

	if (Direction == SVC_API_FW_INBOUND)
	{
		if (Protocol == (uint32)EKnownProtocols::TCP)
		{
			ui.cmbLocalPorts->addItem("IPHTTPSIn", "IPHTTPSIn");
			ui.cmbLocalPorts->addItem("RPC-EPMap", "RPC-EPMap");
			ui.cmbLocalPorts->addItem("RPC", "RPC");
		}
		else if (Protocol == (uint32)EKnownProtocols::UDP)
		{
			ui.cmbLocalPorts->addItem("Teredo", "Teredo");
			ui.cmbLocalPorts->addItem("Ply2Disc", "Ply2Disc");
			ui.cmbLocalPorts->addItem("mDNS", "mDNS");
			ui.cmbLocalPorts->addItem("DHCP", "DHCP");
		}
	}

	auto SetPorts = [&](const QStringList& Ports, QComboBox* pBox) {
		int Pos;
		if (Ports.isEmpty() || (Ports.size() == 1 && Ports[0] == ""))
			Pos = 0; // Any Port
		else
			Pos = Ports.count() == 1 ? pBox->findData(Ports[0]) : -1;
		pBox->setCurrentIndex(Pos);
		if (Pos == -1)
			pBox->setCurrentText(Ports.join(", "));
	};

	SetPorts(m_pRule->m_LocalPorts, ui.cmbLocalPorts);
	SetPorts(m_pRule->m_RemotePorts, ui.cmbRemotePorts);
}

void CFirewallRuleWnd::OnLocalIPChanged()
{
	ui.btnLocalIPSet->setEnabled(ui.radLocalIPCustom->isChecked());
	ui.lstLocalIP->setEnabled(ui.radLocalIPCustom->isChecked());
}

void CFirewallRuleWnd::OnLocalIPEdit()
{
	ui.lstLocalIP->setEnabled(false);
	QListWidgetItem* pItem = ui.lstLocalIP->currentItem();
	ui.cmbLocalIPSet->setCurrentText(pItem->data(Qt::UserRole).toString());
	ui.cmbLocalIPSet->setVisible(true);
	ui.btnLocalIPSet->setVisible(true);
}

void CFirewallRuleWnd::OnLocalIPSet()
{
	QString Address = ui.cmbLocalIPSet->currentText();
	if (!Address.isEmpty() && !ValidateAddress(Address, false))
		return;

	QListWidgetItem* pItem = ui.lstLocalIP->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";
	if (Address.isEmpty())
	{
		if(!bAdd) delete pItem;
		pItem = NULL;
	}
	else if (bAdd) {
		pItem = new QListWidgetItem();
		ui.lstLocalIP->insertItem(ui.lstLocalIP->count() - 1, pItem);
	}
	if (pItem) {
		pItem->setText(Address);
		pItem->setData(Qt::UserRole, Address);
	}

	ui.lstLocalIP->setEnabled(true);
	ui.cmbLocalIPSet->setVisible(false);
	ui.btnLocalIPSet->setVisible(false);
}

void CFirewallRuleWnd::OnRemoteIPChanged()
{
	ui.btnRemoteIPSet->setEnabled(ui.radRemoteIPCustom->isChecked());
	ui.lstRemoteIP->setEnabled(ui.radRemoteIPCustom->isChecked());
}

void CFirewallRuleWnd::OnRemoteIPEdit()
{
	ui.lstRemoteIP->setEnabled(false);
	QListWidgetItem* pItem = ui.lstRemoteIP->currentItem();
	ui.cmbRemoteIPSet->setCurrentText(pItem->data(Qt::UserRole).toString());
	ui.cmbRemoteIPSet->setVisible(true);
	ui.btnRemoteIPSet->setVisible(true);
}

void CFirewallRuleWnd::OnRemoteIPSet()
{
	QString Address = ui.cmbRemoteIPSet->currentText();
	if (!Address.isEmpty() && !ValidateAddress(Address, true))
		return;

	QListWidgetItem* pItem = ui.lstRemoteIP->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";
	if (Address.isEmpty())
	{
		if(!bAdd) delete pItem;
		pItem = NULL;
	}
	else if (bAdd) {
		pItem = new QListWidgetItem();
		ui.lstRemoteIP->insertItem(ui.lstRemoteIP->count() - 1, pItem);
	}
	if (pItem) {
		pItem->setText(Address);
		pItem->setData(Qt::UserRole, Address);
	}

	ui.lstRemoteIP->setEnabled(true);
	ui.cmbRemoteIPSet->setVisible(false);
	ui.btnRemoteIPSet->setVisible(false);
}

bool CFirewallRuleWnd::ValidateAddress(const QString& Address, bool bRemote)
{
	QStringList BeginEnd = SplitStr(Address, "-");
	if (BeginEnd.size() == 1) // singel IP
	{
		if (bRemote)
		{
			if (Address == "LocalSubnet" ||
				Address == "DNS" ||
				Address == "DHCP" ||
				Address == "WINS" ||
				Address == "DefaultGateway" ||
				// win 8+
				Address == "LocalIntranet" ||
				//Address == "RemoteIntranet" ||
				Address == "Internet" // ||
				//Address == "Ply2Renders" ||
			  )
				return true;
		}

		QHostAddress IP;

		if (Address.contains("/")) // ip/net
		{
			QStringList IpNet = SplitStr(Address, "/");
			if(IpNet.size() != 2) {
				QMessageBox::critical(this, "Major Privacy", tr("Invalid Sub Net"));
				return false;
			}

			// todo text subnet
		}
		else
			IP = QHostAddress(Address);

		if (IP.isNull()) {
			QMessageBox::critical(this, "Major Privacy", tr("Invalid IP Address"));
			return false;
		}
	}
	else if (BeginEnd.size() == 2) // range
	{
		QHostAddress BeginIP(BeginEnd[0]);
		QHostAddress EndIP(BeginEnd[1]);
		if (BeginIP.isNull() || EndIP.isNull()) {
			QMessageBox::critical(this, "Major Privacy", tr("Invalid IP Address"));
			return false;
		}
		if(BeginIP < EndIP || BeginIP.protocol() != EndIP.protocol()) {
			QMessageBox::critical(this, "Major Privacy", tr("Invalid IP Range"));
			return false;
		}
	}
	else {
		QMessageBox::critical(this, "Major Privacy", tr("Invalid IP Range"));
		return false;
	}
	return true;
}