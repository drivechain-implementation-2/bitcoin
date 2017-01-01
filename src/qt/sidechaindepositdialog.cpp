#include "sidechaindepositdialog.h"
#include "ui_sidechaindepositdialog.h"

#include "base58.h"
#include "consensus/validation.h"
#include "main.h"
#include "net.h"
#include "primitives/transaction.h"
#include "sidechaindb.h"
#include "txdb.h"
#include "wallet/wallet.h"

#include <QComboBox>

SidechainDepositDialog::SidechainDepositDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainDepositDialog)
{
    ui->setupUi(this);

    for (size_t i = 0; i < ARRAYLEN(ValidSidechains); i++) {
        ui->comboBoxSidechains->addItem(QString::fromStdString(ValidSidechains[i].GetSidechainName()));
    }
}

SidechainDepositDialog::~SidechainDepositDialog()
{
    delete ui;
}

void SidechainDepositDialog::on_pushButtonRequestAddress_clicked()
{
    // TODO
}

void SidechainDepositDialog::on_pushButtonDeposit_clicked()
{
    QDialog errorDialog;

    if (pwalletMain->IsLocked()) {
        errorDialog.setWindowTitle("Wallet must be unlocked to make sidechain deposits");
        errorDialog.exec();
        return;
    }

    unsigned int nSidechain = ui->comboBoxSidechains->currentIndex();

    if (nSidechain > ARRAYLEN(ValidSidechains)) {
        // Should never be displayed
        errorDialog.setWindowTitle("Invalid sidechain");
        errorDialog.exec();
        return;
    }

    const Sidechain& s = ValidSidechains[nSidechain];

    // Get keyID
    CBitcoinAddress address(ui->lineEditAddress->text().toStdString());
    CKeyID keyID;
    if (!address.GetKeyID(keyID)) {
        errorDialog.setWindowTitle("Invalid deposit address");
        errorDialog.exec();
        return;
    }

    // Add keyID to a script
    CScript script = CScript() << s.nSidechain << ToByteVector(keyID) << OP_CHECKWORKSCORE;

    // Payment to deposit script (sidechain number + keyID + CHECKWORKSCORE)
    std::vector <CRecipient> vRecipient;
    CRecipient payment = {script, ui->payAmount->value(), false};
    vRecipient.push_back(payment);

    // Create deposit transaction
    CReserveKey reserveKey(pwalletMain);
    CAmount nFee;
    int nChangePos = -1;
    CWalletTx wtx;
    std::string strError;
    if (!pwalletMain->CreateTransaction(vRecipient, wtx, reserveKey, nFee, nChangePos, strError)) {
        // TODO make pretty
        errorDialog.setWindowTitle(QString::fromStdString(strError));
        errorDialog.exec();
        return;
    }

    CValidationState state;
    if (!pwalletMain->CommitTransaction(wtx, reserveKey, g_connman.get(), state)) {
        // TODO make pretty
        errorDialog.setWindowTitle("Failed to commit deposit transaction");
        errorDialog.exec();
        return;
    }

    QString success = "Deposit Tx Created:\n";
    success.append(QString::fromStdString(wtx.GetHash().GetHex()));
    errorDialog.setWindowTitle(success);
    errorDialog.exec();
}
