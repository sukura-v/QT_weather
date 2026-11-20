#ifndef WEATHERWORKER_H
#define WEATHERWORKER_H
#include <QString>
#include <QVariant>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QSettings>
#include<QCoreApplication>
class WeatherWorker :public QObject
{
    Q_OBJECT
public:
    explicit WeatherWorker();
public slots:
    void saveWeather(const QString &source, const QString &cityName, const QString &cityCode,
                     const QVariant &temp, const QVariant &humidity, const QString &wd,
                     const QString &wf, const QDateTime &collectedAt, const QString &status);
    void closeConnection();

    void getSettings(quint64 revision, const QString &databaseName,
                     const QString &userName, const QString &password);

signals:
    void insertSuccess(const QString &cityName, qint64 id, quint64 revision);
    void insertFail(const QString &cityName,const QString &error);
    void dbReconnection(quint64 revision, bool automatic);
private:
    // db.setHostName("127.0.0.1");
    // db.setPort(3306);
    // db.setDatabaseName("sakura");
    // db.setUserName("root");
    QString m_hostName ;
    int  m_port;
    QString m_databaseName;
    QString m_userName;
    QString m_password;
    QString m_connection ="weather_connection";

    quint64 databaseRevision = 0;
    bool isConnectionExit = false;
};

#endif // WEATHERWORKER_H
