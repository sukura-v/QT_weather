#ifndef ADDDIALOG_H
#define ADDDIALOG_H

#include <QDialog>
#include <QSqlQuery>
#include <QMessageBox>
#include <QSqlError>
namespace Ui {
class AddDialog;
}

class AddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDialog(QWidget *parent = nullptr);
    ~AddDialog();
    qint64 getId();
    QString getCityName();

private:
    Ui::AddDialog *ui;
    void onChange();
    void onBack();
    qint64 id;
    QString cityName;
};

#endif // ADDDIALOG_H
