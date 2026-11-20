
#include "detaildialog.h"
#include "ui_detaildialog.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>

DetailDialog::DetailDialog(qint64 id, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DetailDialog)
{
    ui->setupUi(this);

    auto *editCombo = ui->comboBox_status;
    editCombo->setItemData(editCombo->findText("成功"), "OK");
    editCombo->setItemData(editCombo->findText("解析异常"), "PARSE_ERR");
    editCombo->setItemData(editCombo->findText("入库失败"), "DB_ERR");

    this->id = id;
    this->loadRecord();
}

DetailDialog::~DetailDialog()
{
    delete ui;
}

void DetailDialog::on_pushButton_back_clicked()
{
    this->close();
}

void DetailDialog::loadRecord()
{
    QSqlQuery query;

    query.prepare(
        "SELECT id, source, city_name, city_code, "
        "temperature, humidity, wind_direction, "
        "wind_force, collected_at, status "
        "FROM weather "
        "WHERE id = :id"
        );

    query.bindValue(":id", this->id);

    if (!query.exec()) {
        QMessageBox::warning(
            this,
            "查询失败",
            query.lastError().text()
            );
        return;
    }

    if (!query.next()) {
        QMessageBox::information(
            this,
            "提示",
            "该天气记录不存在"
            );
        return;
    }

    // 根据 SELECT 的字段顺序读取当前记录

    ui->lineEdit_id->setText(
        query.value(0).toString()
        );

    ui->lineEdit_source->setText(
        query.value(1).toString()
        );

    ui->lineEdit_cityName_record->setText(
        query.value(2).toString()
        );

    ui->lineEdit_cityCode->setText(
        query.value(3).toString()
        );

    const auto setMeasurement = [](QDoubleSpinBox *field, const QVariant &value) {
        if (value.isNull()) {
            field->setSpecialValueText(QStringLiteral("未采集"));
            field->setValue(field->minimum());
        } else {
            field->setValue(value.toDouble());
        }
    };
    setMeasurement(ui->doubleSpinBox_temp, query.value(4));
    setMeasurement(ui->doubleSpinBox_humidity, query.value(5));

    ui->lineEdit_windDirection->setText(
        query.value(6).toString()
        );

    ui->lineEdit_windForce->setText(
        query.value(7).toString()
        );

    ui->dateTimeEdit_collectedAt->setDateTime(
        query.value(8).toDateTime()
        );

    QString status = query.value(9).toString().trimmed();

    int statusIndex = ui->comboBox_status->findData(status);

    ui->comboBox_status->setCurrentIndex(statusIndex);
}
