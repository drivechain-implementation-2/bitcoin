// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechainpage.h"
#include "ui_sidechainpage.h"

#include "base58.h"
#include "bitcoinunits.h"
#include "coins.h"
#include "consensus/validation.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "optionsmodel.h"
#include "sidechaindb.h"
#include "txdb.h"
#include "wallet/wallet.h"
#include "walletmodel.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QStackedWidget>

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h" /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

SidechainPage::SidechainPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SidechainPage)
{
    ui->setupUi(this);

    // Initialize models and table views
    incomingTableView = new QTableView(this);
    outgoingTableView = new QTableView(this);
    incomingTableModel = new SidechainHistoryTableModel(this);
    outgoingTableModel = new SidechainHistoryTableModel(this);

    // Set table models
    incomingTableView->setModel(incomingTableModel);
    outgoingTableView->setModel(outgoingTableModel);

    // Table style
#if QT_VERSION < 0x050000
    incomingTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    incomingTableView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    incomingTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    incomingTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Display tables
    ui->frameIncoming->layout()->addWidget(incomingTableView);
    ui->frameOutgoing->layout()->addWidget(outgoingTableView);

    generateAddress();
}

SidechainPage::~SidechainPage()
{
    delete ui;
}

void SidechainPage::generateQR(QString data)
{
    if (data.isEmpty())
        return;

    if (data.size() != 34)
        return;

#ifdef USE_QRCODE
    ui->QRCode->clear();

    QRcode *code = QRcode_encodeString(data.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);

    if (code) {
        QImage qr = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
        qr.fill(0xffffff);

        unsigned char *data = code->data;
        for (int y = 0; y < code->width; y++) {
            for (int x = 0; x < code->width; x++) {
                qr.setPixel(x + 4, y + 4, ((*data & 1) ? 0x0 : 0xffffff));
                data++;
            }
        }

        QRcode_free(code);
        ui->QRCode->setPixmap(QPixmap::fromImage(qr).scaled(200, 200));
    }
#endif
}

void SidechainPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if (model && model->getOptionsModel())
    {
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this,
                SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
    }
}

void SidechainPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                               const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    displayBalance(balance, immatureBalance + unconfirmedBalance);
}

void SidechainPage::displayBalance(const CAmount& balance, const CAmount& pending)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    ui->available->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    ui->pending->setText(BitcoinUnits::formatWithUnit(unit, pending, false, BitcoinUnits::separatorAlways));
}

void SidechainPage::on_pushButtonWithdraw_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageWithdraw);
}

void SidechainPage::on_pushButtonDeposit_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageDeposit);
}

void SidechainPage::on_pushButtonCopy_clicked()
{
    GUIUtil::setClipboard(ui->lineEditDepositAddress->text());
}

void SidechainPage::on_pushButtonNew_clicked()
{
    generateAddress();
}

void SidechainPage::on_pushButtonWT_clicked()
{
    QDialog dialog;

    if (!validate()) {
        dialog.setWindowTitle("Invalid entry");
        dialog.exec();
        return;
    }

    if (pwalletMain->IsLocked()) {
        dialog.setWindowTitle("Wallet must be unlocked");
        dialog.exec();
        return;
    }

    // WT
    SidechainWT wt;

    // Set wt KeyID
    CBitcoinAddress address(ui->payTo->text().toStdString());
    if (!address.GetKeyID(wt.keyID)) {
        // TODO make pretty
        dialog.setWindowTitle("Invalid address");
        dialog.exec();
        return;
    }

    // Set wt sidechain ID
    wt.nSidechain = THIS_SIDECHAIN.nSidechain;

    // Send payment to wt script
    std::vector<CRecipient> vecSend;
    CScript wtScript = CScript() << OP_RETURN << THIS_SIDECHAIN.nSidechain; // TODO
    CRecipient recipient = {wtScript, ui->payAmount->value(), false};
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CReserveKey reserveKey(pwalletMain);
    CAmount nFee;
    int nChangePos = -1;
    std::string strError;
    if (!pwalletMain->CreateTransaction(vecSend, wtx, reserveKey, nFee, nChangePos, strError))
    {
        // TODO make pretty
        dialog.setWindowTitle(QString::fromStdString(strError));
        dialog.exec();
        return;
    }

    CValidationState state;
    if (!pwalletMain->CommitTransaction(wtx, reserveKey, g_connman.get(), state))
    {
        // TODO make pretty
        dialog.setWindowTitle("Failed to commit wt transaction");
        dialog.exec();
        return;
    }
}

void SidechainPage::on_addressBookButton_clicked()
{

}

void SidechainPage::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SidechainPage::on_deleteButton_clicked()
{
    ui->payTo->clear();
}

bool SidechainPage::validate()
{
    // Check address
    CBitcoinAddress address(ui->payTo->text().toStdString());
    if (!address.IsValid()) return false;

    if (!ui->payAmount->validate()) return false;

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0)
    {
        ui->payAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        return false;
    }

    return true;
}

void SidechainPage::generateAddress()
{
    // TODO make part of wallet model, use AddressTableModel

    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->TopUpKeyPool();

    CPubKey newKey;
    if (pwalletMain->GetKeyFromPool(newKey)) {
        CKeyID keyID = newKey.GetID();

        CBitcoinAddress address(keyID);
        generateQR(QString::fromStdString(address.ToString()));

        ui->lineEditDepositAddress->setText(QString::fromStdString(address.ToString()));
    }
}
