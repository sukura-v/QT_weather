#include "weatherworker.h"

WeatherWorker::WeatherWorker()
{
    this->m_hostName ="127.0.0.1";
    this->m_port =  3306;
    QSettings settings(QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"), QSettings::IniFormat);
    this->getSettings(0,
                      settings.value("database/databaseName").toString(),
                      settings.value("database/userName").toString(),
                      settings.value("database/password").toString());
}

void WeatherWorker::saveWeather(const QString &source, const QString &cityName, const QString &cityCode,
                                const QVariant &temp, const QVariant &humidity, const QString &wd,
                                const QString &wf, const QDateTime &collectedAt, const QString &status)
{
    const QString &cityLabel = cityName.isEmpty() ? cityCode : cityName;
    isConnectionExit = QSqlDatabase::contains(this->m_connection);
    //get the connection: check the connnection is been registered
    QSqlDatabase db;
    if(!isConnectionExit){
         db = QSqlDatabase::addDatabase("QMYSQL",this->m_connection);
        db.setHostName(this->m_hostName);
        db.setPort(this->m_port);
        db.setDatabaseName(this->m_databaseName);
        db.setUserName(this->m_userName);
        db.setPassword(this->m_password);
    }
    else{
        db = QSqlDatabase::database(this->m_connection,false);
    }
    //open the registerred connection
    if(!db.isOpen()){
        if(!db.open()){
            emit insertFail(cityLabel,"DB_ERR：数据库连接失败：" + db.lastError().text());
            return;
        }
        if(isConnectionExit){
            emit dbReconnection(databaseRevision, true);
        }

    }
    if(!db.transaction()){
        QString error=db.lastError().text();
        db.close();
        emit insertFail(cityLabel, "DB_ERR：无法开始事务：" + error);
        return;
    }
    //get query reply
    bool inserted = false;
    QString insertError;
    qint64 newId = 0;
    {
        QSqlQuery query(db);

        inserted = query.prepare(
            "INSERT INTO weather ("
            "source, city_name, city_code, "
            "temperature, humidity, "
            "wind_direction, wind_force, "
            "collected_at, status"
            ") VALUES ("
            ":source, :city_name, :city_code, "
            ":temperature, :humidity, "
            ":wind_direction, :wind_force, "
            ":collected_at, :status"
            ")"
            );
        if (inserted) {
            const QVariant missingText(QMetaType::fromType<QString>());
            query.bindValue(":source", source);
            query.bindValue(":city_name", cityName.isEmpty() ? missingText : QVariant(cityName));
            query.bindValue(":city_code", cityCode);
            query.bindValue(":temperature", temp);
            query.bindValue(":humidity", humidity);
            query.bindValue(":wind_direction", wd.isEmpty() ? missingText : QVariant(wd));
            query.bindValue(":wind_force", wf.isEmpty() ? missingText : QVariant(wf));
            query.bindValue(":collected_at", collectedAt);
            query.bindValue(":status", status);
            inserted = query.exec();
        }
        if (inserted) {
            newId = query.lastInsertId().toLongLong();
        } else {
            insertError = query.lastError().text();
        }
    }
    //if insert failed
    if(!inserted){
        bool rolledBack = db.rollback();
        emit insertFail(cityLabel, "DB_ERR：INSERT失败：" + insertError + (rolledBack ? "，已回滚" : "，回滚失败"));
        db.close();
        return;
    }
    //if commit failed
    if(!db.commit()){
        QString error = db.lastError().text();
        bool rolledBack = db.rollback();
        emit insertFail(cityLabel,"DB_ERR：事务提交失败：" + error +(rolledBack ? "，已回滚" : "，回滚失败"));
        db.close();
        return;
    }
    emit insertSuccess(cityLabel, newId, databaseRevision);

}

void WeatherWorker::closeConnection()
{
    if(!QSqlDatabase::contains(this->m_connection))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(
            m_connection, false
            );
        db.close();
    }
   QSqlDatabase::removeDatabase(m_connection);
}

void WeatherWorker::getSettings(quint64 revision, const QString &databaseName,
                                const QString &userName, const QString &password)
{
    databaseRevision = revision;
    this->m_databaseName = databaseName;
    this->m_userName = userName;
    this->m_password = password;
    if (!QSqlDatabase::contains(this->m_connection)) {
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(this->m_connection, false);
    db.close();
    db.setHostName(this->m_hostName);
    db.setPort(this->m_port);
    db.setDatabaseName(this->m_databaseName);
    db.setUserName(this->m_userName);
    db.setPassword(this->m_password);
    if (db.open()) {
        emit dbReconnection(databaseRevision, false);
    }
}

