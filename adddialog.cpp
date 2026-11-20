#include "adddialog.h"
#include "ui_adddialog.h"

AddDialog::AddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddDialog)
{
    ui->setupUi(this);
    auto *editCombo = ui->comboBox_status;
    editCombo->setItemData(editCombo->findText("成功"), "OK");
    editCombo->setItemData(editCombo->findText("解析异常"), "PARSE_ERR");
    editCombo->setItemData(editCombo->findText("入库失败"), "DB_ERR");

    connect(ui->buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &AddDialog::onChange);

    connect(ui->buttonBox,
            &QDialogButtonBox::rejected,
            this,
            &AddDialog::onBack);
    ui->dateTimeEdit_collectedAt->setDateTime(QDateTime::currentDateTime());
}


AddDialog::~AddDialog()
{
    delete ui;
}

qint64 AddDialog::getId()
{
    return this->id;
}

QString AddDialog::getCityName()
{
    return this->cityName;
}

void AddDialog::onChange()
{ 
    //
    QString source = ui->lineEdit_source->text().trimmed();
    QString cityName = ui->lineEdit_cityName_record->text().trimmed();
    this->cityName = cityName;
    QString cityCode = ui->lineEdit_cityCode->text().trimmed();
    double temp = ui->doubleSpinBox_temp->value();
    double humidity = ui->doubleSpinBox_humidity->value();
    QString windDirection = ui->lineEdit_windDirection->text().trimmed();
    QString windForce = ui->lineEdit_windForce->text().trimmed();
    QDateTime collectedAt = ui->dateTimeEdit_collectedAt->dateTime();
    QString status = ui->comboBox_status->currentData().toString();

    QSqlQuery query;
    QString sql =
        "INSERT INTO weather "
        "(source, city_name, city_code, temperature, "
        "humidity, wind_direction, wind_force, "
        "collected_at, status) "
        "VALUES "
        "(:source, :city_name, :city_code, :temperature, "
        ":humidity, :wind_direction, :wind_force, "
        ":collected_at, :status)";
    query.prepare(sql);
    query.bindValue(":source",         source);
    query.bindValue(":city_name",      cityName);
    query.bindValue(":city_code",      cityCode);
    query.bindValue(":temperature",    temp);
    query.bindValue(":humidity",       humidity);
    query.bindValue(":wind_direction", windDirection);
    query.bindValue(":wind_force",     windForce);
    query.bindValue(":collected_at",   collectedAt);
    query.bindValue(":status",         status);
    if(!query.exec()){
        QMessageBox::warning(this,"Warnning","Failed to add: "+query.lastError().text());
        return;
    }
    this->id = query.lastInsertId().toLongLong();
    QMessageBox::information(this,"Success","Success to add!");

   accept();
}


void AddDialog::onBack()
{
    reject();
}

