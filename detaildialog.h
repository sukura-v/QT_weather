#ifndef DETAILDIALOG_H
#define DETAILDIALOG_H

#include <QDialog>
#include <QtGlobal>
#include <QSqlQuery>
#include <QMessageBox>
namespace Ui {
class DetailDialog;
}

class DetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DetailDialog(qint64 id,QWidget *parent = nullptr);
    ~DetailDialog();

private slots:
    void on_pushButton_back_clicked();

private:
    Ui::DetailDialog *ui;
    void loadRecord();
    qint64 id;
};

#endif // DETAILDIALOG_H
