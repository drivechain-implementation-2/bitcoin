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
    void on_pushButtonDeposit_clicked();

    void on_pushButtonPaste_clicked();

    void on_pushButtonClear_clicked();

private:
    Ui::SidechainDepositDialog *ui;

    bool validateDepositAmount();
};

#endif // SIDECHAINDEPOSITDIALOG_H
