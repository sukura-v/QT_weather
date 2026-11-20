#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include  <QLabel>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlQueryModel>
#include <QMap>
#include <QTimer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QNetworkAccessManager>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QNetworkReply>
#include <QThread>
#include <QTextBlock>
#include <optional>
#include <QDate>
#include <QFileInfo>
#include <QDir>
#include <QStandardItemModel>
#include "weatherworker.h"
#include <QAbstractItemModel>
#include <QTextCursor>
#include <QFileDialog>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QHeaderView>
#include <QAbstractItemView>
#include "detaildialog.h"
#include "settingdialog.h"
#include <QMouseEvent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyle>
#include <QCoreApplication>

#include <QCloseEvent>
#include <QEvent>

class QSystemTrayIcon;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
struct queryFilter{
    QString cityName;
    QString queryStatus;
    bool useDateRange;
    QDateTime startTime;
    QDateTime endTime;

};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pushButton_query_clicked();

    void on_actionQuery_triggered();

    void on_tableView_records_clicked(const QModelIndex &index);

    void on_pushButton_modify_clicked();

    void on_pushButton_manualRecord_clicked();

    void on_pushButton_delete_clicked();

    void on_actionStart_triggered();

    void on_actionImmediate_triggered();

    void on_actionStop_triggered();

    void on_pushButton_detail_clicked();

    void on_actionExport_data_triggered();

    void on_actionStatistics_triggered();

    void on_actionSettting_triggered();

    void on_actionAbout_triggered();

    void on_actionExit_triggered();

    void on_actionShowMain_triggered();
protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    Ui::MainWindow *ui;

    QSystemTrayIcon *trayIcon = nullptr;


    //Mysql
    queryFilter filter{};
    quint64 databaseRevision = 0;
    QStandardItemModel *model;
    void queryData(const queryFilter&filter,std::optional<qint64> id = std::nullopt);
    void collectOnce();
    void clearRecords();
    void fillModel(QSqlQuery &query);
    void insertModelRow(QSqlQuery &query);

    //status
    void statusUpdateTime();
    void statusSetUser();
    void statusUpdateNum();
    QLabel * systemTime;
    QLabel * currentUser;
    QLabel * recordStatus;
    QLabel * recordNum;

     //log
    void initLogFile();
    void saveLogFile();
    QString logPath;

    QThread *dbThread =nullptr;
    WeatherWorker *dbWorker = nullptr;

    //statusbar:
    QTimer* statusTimer;

    //collect from network
    QTimer* collectTimer;
    int intervalSeconds; //settings
    QNetworkAccessManager* collectManager;
    QNetworkReply *currentReply = nullptr;
    QString weatherTargetUrl;//settings
    QString weatherCityCode;

    bool isCollecting;
    QTimer * retryTimer;
    int retryCount = 0;

    //settings
    void loadSettings();
    void applyCollectInternal();
    void applyWeather();
    bool applyDatabase(bool reconnectOnly = false);
signals:
    void saveWeatherRequested(const QString &source, const QString &cityName, const QString &cityCode,
                              const QVariant &temp, const QVariant &humidity, const QString &wd,
                              const QString &wf, const QDateTime &collectedAt, const QString &status);
    void updateDatabaseSettings(quint64 revision, const QString &databaseName,
                                const QString &userName, const QString &password);


};
#endif // MAINWINDOW_H
