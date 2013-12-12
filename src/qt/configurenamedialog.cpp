#include "configurenamedialog.h"
#include "ui_configurenamedialog.h"

#include "guiutil.h"
#include "addressbookpage.h"
#include "walletmodel.h"
#include "guiconstants.h"
#include "../main.h"
#include "../hook.h"
#include "../wallet.h"
#include "../namecoin.h"

#include "../json/json_spirit.h"
#include "../json/json_spirit_utils.h"
#include "../json/json_spirit_reader_template.h"
#include "../json/json_spirit_writer_template.h"

#include <boost/foreach.hpp>

#include <QMessageBox>
#include <QClipboard>

ConfigureNameDialog::ConfigureNameDialog(const QString &name_, const QString &data, const QString &address_, bool firstUpdate_, QWidget *parent) :
    QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint),
    name(name_),
    address(address_),
    firstUpdate(firstUpdate_),
    initialized(false),
    ui(new Ui::ConfigureNameDialog)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC
    ui->transferToLayout->setSpacing(4);
#endif

    GUIUtil::setupAddressWidget(ui->transferTo, this, true);

    ui->labelName->setText(GUIUtil::HtmlEscape(name));
    ui->dataEdit->setText(data);
    ui->labelAddress->setText(GUIUtil::HtmlEscape(address));
    ui->labelAddress->setFont(GUIUtil::bitcoinAddressFont());

    ui->dataEdit->setMaxLength(GUI_MAX_VALUE_LENGTH);

    returnData = data;

    if (name.startsWith("d/"))
        ui->labelDomain->setText(GUIUtil::HtmlEscape(name.mid(2) + ".bit"));
    else
        ui->labelDomain->setText(tr("(not a domain name)"));
        
    // Try to select the most appropriate wizard
    json_spirit::Value val;
    if (data.trimmed().isEmpty())
    {
        // Empty data - select DNS for domains, custom for other
        if (name.startsWith("d/"))
            ui->tabWidget->setCurrentWidget(ui->tab_dns);
        else
            ui->tabWidget->setCurrentWidget(ui->tab_json);
    }
    else if (!json_spirit::read_string(data.toStdString(), val) || val.type() != json_spirit::obj_type)
    {
        // Non-JSON data - select custom tab
        ui->tabWidget->setCurrentWidget(ui->tab_json);
    }
    else
    {
        // Check conformance to DNS type
        bool ok = true;
        QStringList ns;
        QString translate, tls;
        BOOST_FOREACH(json_spirit::Pair& item, val.get_obj())
        {
            if (item.name_ == "ns")
            {
                if (item.value_.type() == json_spirit::array_type)
                {
                    BOOST_FOREACH(json_spirit::Value val, item.value_.get_array())
                    {
                        if (val.type() == json_spirit::str_type)
                            ns.append(QString::fromStdString(val.get_str()));
                        else
                        {
                            ok = false;
                            break;
                        }
                    }
                }
                else
                    ok = false;
            }
            else if (item.name_ == "translate")
            {
                if (item.value_.type() == json_spirit::str_type)
                    translate = QString::fromStdString(item.value_.get_str());
                else
                    ok = false;
            }
            else if (item.name_ == "tls")
            {
                if (item.value_.type() == json_spirit::str_type)
                    tls = QString::fromStdString(item.value_.get_str());
                else
                    ok = false;
            }
            else
                ok = false;

            if (!ok)
                break;
        }
        if (ns.empty())
            ok = false;

        if (ok)
        {
            ui->nsEdit->setPlainText(ns.join("\n"));
            ui->nsTranslateEdit->setText(translate);
            ui->nsTLSEdit->setText(tls);
            ui->tabWidget->setCurrentWidget(ui->tab_dns);
        }
        else
        {
            // Check conformance to IP type
            json_spirit::Object obj = val.get_obj();
            QString ip, tls;
            json_spirit::Value ipVal = json_spirit::find_value(obj, "ip");
            json_spirit::Value mapVal = json_spirit::find_value(obj, "map");
            json_spirit::Value tlsVal = json_spirit::find_value(obj, "tls");
            int n = 2;
            if (tlsVal.type() == json_spirit::str_type)
            {
                tls = QString::fromStdString(tlsVal.get_str());
                n++;
            }
            if (obj.size() == n && ipVal.type() == json_spirit::str_type && mapVal.type() == json_spirit::obj_type)
            {
                ip = QString::fromStdString(ipVal.get_str());
                json_spirit::Object map = mapVal.get_obj();
                json_spirit::Value starVal = json_spirit::find_value(map, "*");
                if (map.size() == 1 && starVal.type() == json_spirit::str_type)
                    ok = starVal.get_str() == ipVal.get_str();
                else if (map.size() == 1 && starVal.type() == json_spirit::obj_type)
                {
                    json_spirit::Object starMap = starVal.get_obj();
                    json_spirit::Value starIp = json_spirit::find_value(starMap, "ip");
                    if (starMap.size() == 1 && starIp.type() == json_spirit::str_type)
                        ok = starIp.get_str() == ipVal.get_str();
                    else
                        ok = false;
                }
                else
                    ok = false;
            }
            else
                ok = false;

            if (ok)
            {
                ui->ipEdit->setText(ip);
                ui->ipTLSEdit->setText(tls);
                ui->tabWidget->setCurrentWidget(ui->tab_ip);
            }
            else
                ui->tabWidget->setCurrentWidget(ui->tab_json);
        }
    }

    on_dataEdit_textChanged(data);

    if (firstUpdate)
    {
        ui->labelTransferTo->hide();
        ui->labelTransferToHint->hide();
        ui->transferTo->hide();
        ui->addressBookButton->hide();
        ui->pasteButton->hide();
        ui->labelSubmitHint->setText(
            tr("Name_firstupdate transaction will be queued and broadcasted when corresponding name_new is %1 blocks old").arg(MIN_FIRSTUPDATE_DEPTH)
            + "<br/><span style='color:red'>" + tr("Do not close your client while the name is pending!") + "</span>"
        );
    }
    else
    {
        ui->labelSubmitHint->setText(tr("Name_update transaction will be issued immediately"));
        setWindowTitle(tr("Update Name"));
    }

    initialized = true;
}

ConfigureNameDialog::~ConfigureNameDialog()
{
    delete ui;
}

void ConfigureNameDialog::accept()
{
    if (!walletModel)
        return;

    QString addr;
    if (!firstUpdate)
    {
        if (!ui->transferTo->hasAcceptableInput())
        {
            ui->transferTo->setValid(false);
            return;
        }

        addr = ui->transferTo->text();

        if (addr != "" && !walletModel->validateAddress(addr))
        {
            ui->transferTo->setValid(false);
            return;
        }
    }
    
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())
        return;

    returnData = ui->dataEdit->text();

    QString err_msg;
    try
    {
        if (firstUpdate)
            err_msg = walletModel->nameFirstUpdatePrepare(name, returnData);
        else
            err_msg = walletModel->nameUpdate(name, returnData, addr);
    }
    catch (std::exception& e) 
    {
        err_msg = e.what();
    }

    if (!err_msg.isEmpty())
    {
        if (err_msg == "ABORTED")
            return;

        QMessageBox::critical(this, tr("Name update error"), err_msg);
        return;
    }

    QDialog::accept();
}

void ConfigureNameDialog::reject()
{
    QDialog::reject();
}

void ConfigureNameDialog::setModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
}

void ConfigureNameDialog::on_copyButton_clicked()
{
    QApplication::clipboard()->setText(address);
}

void ConfigureNameDialog::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->transferTo->setText(QApplication::clipboard()->text());
}

void ConfigureNameDialog::on_addressBookButton_clicked()
{
    if (!walletModel)
        return;
    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);
    dlg.setModel(walletModel->getAddressTableModel());
    if (dlg.exec())
        ui->transferTo->setText(dlg.getReturnValue());
}

void ConfigureNameDialog::SetDNS()
{
    json_spirit::Object data;
    QStringList vLines = ui->nsEdit->toPlainText().split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

    json_spirit::Array ns;

    for (int i = 0; i < vLines.size(); i++)
    {
        QString item = vLines.at(i).trimmed();
        if (item.isEmpty())
            continue;
        ns.push_back(json_spirit::Value(item.toStdString()));
    }
    data.push_back(json_spirit::Pair("ns", ns));
    QString translate = ui->nsTranslateEdit->text().trimmed();
    if (!translate.isEmpty())
    {
        if (!translate.endsWith("."))
            translate += ".";
        data.push_back(json_spirit::Pair("translate", translate.toStdString()));
    }

	//TODO create a function to avoid copied code
	json_spirit::Object tls_protocols;
	json_spirit::Object tls_ports;
	json_spirit::Array tls_fingerprints;
	json_spirit::Array tls_data;
	
	QString TLSTypeBox = ui->nsTLSTypeBox->currentText().trimmed();
	QString TLSProtocolBox = ui->nsTLSProtocolBox->currentText().trimmed();
    QString TLSEdit = ui->nsTLSEdit->text().trimmed();
	QString TLSPortEdit = ui->nsTLSPortEdit->text().trimmed();
	int TLSincludeSubdomainsCheckBox = (int) ui->nsTLSincludeSubdomainsCheckBox->isChecked();
	if (!TLSEdit.isEmpty() && TLSTypeBox.toStdString() != "None"){
		
		if(TLSTypeBox.toStdString() == "SHA256")
			tls_data.push_back(json_spirit::Value(1));
		else if (TLSTypeBox.toStdString() == "SHA512")
			tls_data.push_back(json_spirit::Value(2));
		
		tls_data.push_back(json_spirit::Value(TLSEdit.toStdString()));
		
		tls_data.push_back(json_spirit::Value(TLSincludeSubdomainsCheckBox));
		
		tls_fingerprints.push_back(json_spirit::Value(tls_data));
		tls_ports.push_back(json_spirit::Pair( TLSPortEdit.toStdString() , tls_fingerprints ));
		tls_protocols.push_back(json_spirit::Pair( TLSProtocolBox.toStdString() , tls_ports ));
		
        data.push_back(json_spirit::Pair("tls", tls_protocols));
	}

    ui->dataEdit->setText(QString::fromStdString(json_spirit::write_string(json_spirit::Value(data), false)));
}

void ConfigureNameDialog::SetIP()
{
    json_spirit::Object data;
    data.push_back(json_spirit::Pair("ip", ui->ipEdit->text().trimmed().toStdString()));
	
	//we need to add the data into a map and also directly because without map, we are 
	//referencing a bare domain and with a wildcard map, we are referencing all subdomains
    json_spirit::Object map, submap;
    submap.push_back(json_spirit::Pair("ip", ui->ipEdit->text().trimmed().toStdString()));

	//TODO create a function to avoid copied code
	json_spirit::Object tls_protocols;
	json_spirit::Object tls_ports;
	json_spirit::Array tls_fingerprints;
	json_spirit::Array tls_data;
	
	QString TLSTypeBox = ui->ipTLSTypeBox->currentText().trimmed();
	QString TLSProtocolBox = ui->ipTLSProtocolBox->currentText().trimmed();
    QString TLSEdit = ui->ipTLSEdit->text().trimmed();
	QString TLSPortEdit = ui->ipTLSPortEdit->text().trimmed();
	int TLSincludeSubdomainsCheckBox = (int) ui->ipTLSincludeSubdomainsCheckBox->isChecked();
	if (!TLSEdit.isEmpty() && TLSTypeBox.toStdString() != "None"){
		
		if(TLSTypeBox.toStdString() == "SHA256")
			tls_data.push_back(json_spirit::Value(1));
		else if (TLSTypeBox.toStdString() == "SHA512")
			tls_data.push_back(json_spirit::Value(2));
		
		tls_data.push_back(json_spirit::Value(TLSEdit.toStdString()));
		
		tls_data.push_back(json_spirit::Value(TLSincludeSubdomainsCheckBox));
		
		tls_fingerprints.push_back(json_spirit::Value(tls_data));
		tls_ports.push_back(json_spirit::Pair( TLSPortEdit.toStdString() , tls_fingerprints ));
		tls_protocols.push_back(json_spirit::Pair( TLSProtocolBox.toStdString() , tls_ports ));
		
		submap.push_back(json_spirit::Pair("tls", tls_protocols));
        data.push_back(json_spirit::Pair("tls", tls_protocols));
	}

	map.push_back(json_spirit::Pair("*", submap));
    data.push_back(json_spirit::Pair("map", map));
	
    ui->dataEdit->setText(QString::fromStdString(json_spirit::write_string(json_spirit::Value(data), false)));
}

void ConfigureNameDialog::on_dataEdit_textChanged(const QString &text)
{
    json_spirit::Value val;
    if (json_spirit::read_string(text.toStdString(), val))
    {
        ui->labelJsonValid->setText(tr("Valid JSON string"));
        ui->labelJsonValid->setStyleSheet("color:green");
    }
    else
    {
        ui->labelJsonValid->setText(tr("Invalid JSON string (can still be used, if not intended as JSON string)"));
        ui->labelJsonValid->setStyleSheet("color:brown");
    }
}

