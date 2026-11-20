#include "settingdialog.h"
#include "ui_settingdialog.h"

SettingDialog::SettingDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingDialog)
{
    ui->setupUi(this);
    ui->spinBox_internal->setRange(10,3600);
    this->loadSettings();
    connect(ui->buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &SettingDialog::onChange);

    connect(ui->buttonBox,
            &QDialogButtonBox::rejected,
            this,
            &SettingDialog::onBack);
}

SettingDialog::~SettingDialog()
{
    delete ui;
}

void SettingDialog::onBack()
{
    this->reject();
}

void SettingDialog::onChange()
{
     QSettings settings (QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),QSettings::IniFormat);
    int internal = ui->spinBox_internal->value();
     QVariant internalSeconds = (QVariant) internal;
     QString apiUrl = ui->lineEdit_address->text().trimmed();
    QString cityCode = ui->lineEdit_cityCode->text().trimmed();
     QString databaseName= ui->lineEdit_databaseName->text().trimmed();
    QString userName = ui->lineEdit_userName->text().trimmed();
     QString password = ui->lineEdit_password->text().trimmed();

     settings.setValue("collect/intervalSeconds",internalSeconds);
     settings.setValue("weather/apiUrl",apiUrl);
     settings.setValue("weather/cityCode",cityCode);
     settings.setValue("database/databaseName",databaseName);
     settings.setValue(
         "database/userName", userName
         );

     settings.setValue(
         "database/password", password
         );
    settings.sync();
     this->accept();
}

void SettingDialog::loadSettings()
{
    QSettings settings (QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),QSettings::IniFormat);
    int internal = settings.value("collect/intervalSeconds").toInt();
    QString apiUrl = settings.value("weather/apiUrl").toString();
    QString cityCode =settings.value("weather/cityCode").toString();
    QString databaseName = settings.value("database/databaseName").toString();
    QString userName = settings.value("database/userName").toString();
    QString password = settings.value("database/password").toString();
    ui->spinBox_internal->setValue(internal);
    ui->lineEdit_address->setText(apiUrl);
    ui->lineEdit_cityCode->setText(cityCode);
    ui->lineEdit_databaseName->setText(databaseName);
    ui->lineEdit_userName->setText(userName);
    ui->lineEdit_password->setText(password);
}


