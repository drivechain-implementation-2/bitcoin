#ifndef SIDECHAINDEPOSITDIALOG_H
#define SIDECHAINDEPOSITDIALOG_H

#include <QDialog>

namespace Ui {
class SidechainDepositDialog;
}

class SidechainDepositDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainDepositDialog(QWidget *parent = 0);
    ~SidechainDepositDialog();

private Q_SLOTS:
    void on_pushButtonRequestAddress_clicked();

    void on_pushButtonDeposit_clicked();

private:
    Ui::SidechainDepositDialog *ui;
};

#endif // SIDECHAINDEPOSITDIALOG_H
