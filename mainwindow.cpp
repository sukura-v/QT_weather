#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adddialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),model(new QStandardItemModel(this))
{
    ui->setupUi(this);
    isCollecting = false;
    //settings
    this->loadSettings();
    this->applyWeather();
    this->applyDatabase();
//collect
    collectTimer = new  QTimer(this);
    this->applyCollectInternal();

    collectManager = new QNetworkAccessManager(this);
    connect(collectTimer,&QTimer::timeout,this,&MainWindow::collectOnce);
    retryTimer = new QTimer(this);
    retryTimer->setSingleShot(true);
    connect(retryTimer,&QTimer::timeout,this,&MainWindow::collectOnce);

    //init table
    ui->tableView_records->setModel(this->model);
    ui->tableView_records->viewport()->installEventFilter(this);
    model->setHorizontalHeaderLabels({
        "记录ID",
        "数据来源",
        "城市名称",
        "城市代码",
        "温度(℃)",
        "湿度(%)",
        "风向",
        "风力",
        "采集时间",
        "采集状态"
    });

    ui->tableView_records->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);

    ui->tableView_records->setSelectionBehavior(
        QAbstractItemView::SelectRows
        );

    ui->tableView_records->setSelectionMode(
        QAbstractItemView::SingleSelection
        );

    connect(
        ui->tableView_records,
        &QTableView::doubleClicked,
        this,
        &MainWindow::on_pushButton_detail_clicked
        );

    ui->tableView_records->setSortingEnabled(false);

//statusbar
    systemTime = new QLabel("系统时间：--");
    currentUser = new QLabel("当前用户：--");
    recordStatus = new QLabel("采集状态：已停止");
    recordNum = new QLabel("记录总数：0");
    ui->statusbar->addPermanentWidget(systemTime);
    ui->statusbar->addPermanentWidget(currentUser);
    ui->statusbar->addPermanentWidget(recordStatus);
    ui->statusbar->addPermanentWidget(recordNum);

    statusTimer = new QTimer(this);
    statusTimer->setInterval(1*1000);
    connect(statusTimer,&QTimer::timeout,this,&MainWindow::statusUpdateTime);
    this->statusTimer->start();
    this->statusSetUser();
    connect(model,&QAbstractItemModel::rowsInserted,this,&MainWindow::statusUpdateNum);
    connect(model,&QAbstractItemModel::rowsRemoved,this,&MainWindow::statusUpdateNum);

    //echo details status
    auto *editCombo = ui->comboBox_status;
    editCombo->setItemData(editCombo->findText("成功"), "OK");
    editCombo->setItemData(editCombo->findText("解析异常"), "PARSE_ERR");
    editCombo->setItemData(editCombo->findText("入库失败"), "DB_ERR");

 // query status
    auto *queryCombo = ui->comboBox_queryStatus;
    queryCombo->setItemData(queryCombo->findText("全部"),     "");
    queryCombo->setItemData(queryCombo->findText("成功"), "OK");
    queryCombo->setItemData(queryCombo->findText("解析异常"), "PARSE_ERR");
    queryCombo->setItemData(queryCombo->findText("入库失败"), "DB_ERR");

    //connect the up and down arrow with echo data;
    connect(ui->tableView_records->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::on_tableView_records_clicked);

    //create the worker thread
    // explict WeatherWorker(const QString &host,int port ,QString &databaseName,QString &userName,QString &password);
    dbThread = new QThread(this);
    dbWorker = new WeatherWorker();
    dbWorker->moveToThread(dbThread);

    //connect work:"saveweather" to the worker thread
    connect(this,&MainWindow::saveWeatherRequested,dbWorker,&WeatherWorker::saveWeather,Qt::QueuedConnection);
    connect(this,&MainWindow::updateDatabaseSettings,dbWorker,&WeatherWorker::getSettings,Qt::QueuedConnection);
    // success insert;
    connect(dbWorker, &WeatherWorker::insertSuccess, this, [this](const QString &cityName, qint64 id, quint64 revision) {
        if (revision != databaseRevision) {
            return;
        }
        QString nowTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        ui->textEdit->append(QString("[%1] 入库成功：%2，记录ID=%3").arg(nowTime).arg(cityName).arg(id));
        this->queryData(this->filter,id);

    },Qt::QueuedConnection);

    //failed to insert
    connect(dbWorker,&WeatherWorker::insertFail,this,[this](const QString &cityName,const QString &error){
        QString time =QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        ui->textEdit->append( QString("[%1] 入库失败：%2，%3").arg(time).arg(cityName).arg(error));
    },Qt::QueuedConnection);

    //reconnection record
    connect(dbWorker, &WeatherWorker::dbReconnection, this, [this](quint64 revision, bool automatic) {
        if (revision != databaseRevision || !automatic) {
            return;
        }
        QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        ui->textEdit->append(QString("[%1] 数据库重新连接成功").arg(time));
        this->applyDatabase(/*reconnectOnly=*/true);
    }, Qt::QueuedConnection);

    //destory the thread
    connect(dbThread, &QThread::finished, dbWorker, &QObject::deleteLater);

    dbThread->start();

    //log
    ui->textEdit->document()->setMaximumBlockCount(2000);
    this->initLogFile();
    connect(ui->textEdit,&QTextEdit::textChanged,this,&MainWindow::saveLogFile);

    if (QSqlDatabase::database(
            QSqlDatabase::defaultConnectionName(), false
            ).isOpen()) {
        this->queryData(this->filter);
    }

    // System tray
    trayIcon = new QSystemTrayIcon(this);

    trayIcon->setIcon(
        style()->standardIcon(QStyle::SP_ComputerIcon)
        );

    trayIcon->setToolTip("天气数据采集系统");

    // Tray context menu
    QMenu *trayMenu = new QMenu(this);

    trayMenu->addAction(ui->actionShowMain);
    trayMenu->addAction(ui->actionSettting);
    trayMenu->addSeparator();
    trayMenu->addAction(ui->actionExit);

    trayIcon->setContextMenu(trayMenu);

    // Restore window by double-clicking tray icon
    connect(
        trayIcon,
        &QSystemTrayIcon::activated,
        this,
        [this](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::DoubleClick) {
                ui->actionShowMain->trigger();
            }
        }
        );

    trayIcon->show();



}

MainWindow::~MainWindow()
{
    this->collectTimer->stop();
    QMetaObject::invokeMethod( dbWorker,"closeConnection",Qt::BlockingQueuedConnection);
   this-> dbThread->quit();
    this->dbThread->wait();
    delete ui;
}
//cancle the select row when I click the blank
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->tableView_records->viewport()
        && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);

        if (mouseEvent->button() == Qt::LeftButton
            && !ui->tableView_records->indexAt(mouseEvent->position().toPoint()).isValid()) {
            ui->tableView_records->selectionModel()->clear();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}
//status update time
void MainWindow::statusUpdateTime()
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    systemTime->setText(time);
}
//status set current user
void MainWindow::statusSetUser()
{
    QString USERNAME = QProcessEnvironment::systemEnvironment().value("USERNAME");
    currentUser->setText(USERNAME);
}
//status update recordNum
void MainWindow::statusUpdateNum()
{
    recordNum->setText(QString("记录总数：%1").arg(model->rowCount()));
}

//log
void MainWindow::initLogFile()
{
    QString rootPath = QCoreApplication::applicationDirPath();
    QString logDirPath = QDir(rootPath).filePath(QStringLiteral("logs"));
    QDir logDir(logDirPath);
    if(!logDir.mkpath(".")){
        QMessageBox::warning(this,"warning","Log dir created failed");
        return;
    }
    QString logDate = QDate::currentDate().toString("yyyyMMdd");
    QString logName = "collect-"+logDate+".log";
     logPath = logDir.filePath(logName);
    QFile logFile(logPath);
    if(logFile.exists())
    {
        if(!logFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::warning(this,"warning","The log can't open!");
            return;
        }
        QTextStream in(&logFile);
        QString text = in.readAll();
        logFile.close();
        ui->textEdit->setPlainText(text);
        ui->textEdit->moveCursor(QTextCursor::End);

    }
    else{
        if(!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)){
            QMessageBox::warning(this,"warning","The log can't create!");
            return;
        }
        logFile.close();
    }
}

void MainWindow::saveLogFile()
{
    QString log = ui->textEdit->document()->lastBlock().text();
    QDate logDate = QDate::fromString(log.mid(1, 10), "yyyy-MM-dd");
    QString logName = QString("collect-%1.log")
                          .arg(logDate.toString("yyyyMMdd"));
    QDir logDir(QFileInfo(logPath).absolutePath());
    QString newLogPath = logDir.filePath(logName);
    if (newLogPath != logPath) {
        logPath = newLogPath;
    }
    QFile file(logPath);
    if (!file.open(QIODevice::WriteOnly |
                   QIODevice::Append |
                   QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << log << '\n';
    out.flush();
    file.close();
}
//settings
void MainWindow::loadSettings()
{
    QSettings settings (QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),QSettings::IniFormat);
    if(!settings.contains("collect/intervalSeconds")){
        settings.setValue("collect/intervalSeconds",60);
    }
    const QString weatherApiUrl = settings.value("weather/apiUrl").toString();
    if (!settings.contains("weather/apiUrl")
        || weatherApiUrl == "http://www.weather.com.cn/data/sk/"
        || weatherApiUrl == "https://www.weather.com.cn/data/sk/") {
        settings.setValue("weather/apiUrl", "https://d1.weather.com.cn/sk_2d/");
    }
    if (!settings.contains("weather/cityCode")) {
        settings.setValue("weather/cityCode", "101010100");
    }
    settings.sync();
}

void MainWindow::applyCollectInternal()
{
    QSettings settings (QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),QSettings::IniFormat);
    intervalSeconds = settings.value("collect/intervalSeconds").toInt();
    this->collectTimer->setInterval(intervalSeconds*1000);
}

void MainWindow::applyWeather()
{
    QSettings settings (QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),QSettings::IniFormat);
    const QString weatherApiUrl = settings.value("weather/apiUrl").toString();
    weatherCityCode = settings.value("weather/cityCode").toString();
    weatherTargetUrl = QString("%1%2.html").arg(weatherApiUrl, weatherCityCode);
}

bool MainWindow::applyDatabase(bool reconnectOnly)
{
    QSettings settings(
        QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),
        QSettings::IniFormat
        );
    QString databaseName = settings.value("database/databaseName").toString();
    QString userName = settings.value("database/userName").toString();
    QString password = settings.value("database/password").toString();

    bool databaseChanged = false;
    bool wasOpen = false;
    QSqlDatabase db;
    if (QSqlDatabase::contains()) {
        db = QSqlDatabase::database(
            QSqlDatabase::defaultConnectionName(), false
            );
        wasOpen = db.isOpen();
        databaseChanged = !reconnectOnly && dbWorker
                          && (databaseName != db.databaseName()
                              || userName != db.userName()
                              || password != db.password());
        if (databaseChanged) {
            ++databaseRevision;
            this->clearRecords();
        }
        db.close();
    }
    else {
        db = QSqlDatabase::addDatabase("QMYSQL");
    }
    if (!reconnectOnly) {
        db.setHostName("127.0.0.1");
        db.setPort(3306);
        db.setDatabaseName(databaseName);
        db.setUserName(userName);
        db.setPassword(password);
    }
    bool connected = db.open();
    if (!connected && isCollecting && !reconnectOnly) {
        this->on_actionStop_triggered();
    }
    ui->actionStart->setEnabled(connected && !isCollecting);
    ui->actionImmediate->setEnabled(connected);

    if (!reconnectOnly) {
        emit updateDatabaseSettings(databaseRevision, databaseName, userName, password);
    }
    if (!connected) {
        QMessageBox::warning(
            this,
            "数据库连接失败",
            db.lastError().text()
            );
        return false;
    }
    if (dbWorker && (databaseChanged || reconnectOnly || !wasOpen)) {
        this->queryData(this->filter);
    }
    return true;
}

void MainWindow::on_actionSettting_triggered()
{
    const QSettings settings(
        QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"),
        QSettings::IniFormat
        );
    const int previousInterval = settings.value("collect/intervalSeconds").toInt();
    const QString previousApiUrl = settings.value("weather/apiUrl").toString();
    const QString previousCityCode = settings.value("weather/cityCode").toString();
    const QString previousDatabaseName = settings.value("database/databaseName").toString();
    const QString previousUserName = settings.value("database/userName").toString();
    const QString previousPassword = settings.value("database/password").toString();

    SettingDialog settingdialog(this);
    if (settingdialog.exec() != QDialog::Accepted) {
        return;
    }

    const int newInterval = settings.value("collect/intervalSeconds").toInt();
    const QString newCityCode = settings.value("weather/cityCode").toString();
    const bool apiUrlChanged = previousApiUrl != settings.value("weather/apiUrl").toString();
    const bool cityCodeChanged = previousCityCode != newCityCode;
    const bool databaseChanged = previousDatabaseName != settings.value("database/databaseName").toString()
                              || previousUserName != settings.value("database/userName").toString()
                              || previousPassword != settings.value("database/password").toString();

    QStringList changes;
    if (previousInterval != newInterval) {
        this->applyCollectInternal();
        changes.append(QString("采集间隔从 %1 秒改为 %2 秒").arg(previousInterval).arg(newInterval));
    }
    if (apiUrlChanged || cityCodeChanged) {
        this->applyWeather();
        if (apiUrlChanged) {
            changes.append(QStringLiteral("天气接口地址已更新"));
        }
        if (cityCodeChanged) {
            changes.append(QString("城市代码从 %1 改为 %2").arg(previousCityCode, newCityCode));
        }
    }
    if (databaseChanged) {
        changes.append(this->applyDatabase()
                       ? QStringLiteral("数据库重新连接成功")
                       : QStringLiteral("数据库重新连接失败"));
    }
    if (!changes.isEmpty()) {
        const QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        ui->textEdit->append(QString("[%1] %2").arg(time, changes.join(QStringLiteral("，"))));
    }
}


//Start record
void MainWindow::on_actionStart_triggered()
{
    retryTimer->stop();
    retryCount = 0;
    collectTimer->start();
    isCollecting =true;
    recordStatus->setText("采集状态：采集中");
    ui->actionStart->setEnabled(false);
    ui->actionStop->setEnabled(true);
    ui->textEdit->append(QString("[%1] 开始采集").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
    this->collectOnce();
}

//Stop record
void MainWindow::on_actionStop_triggered()
{
    collectTimer->stop();
    isCollecting  =false;
    retryTimer->stop();
    retryCount = 0;
    if (currentReply != nullptr) {
        currentReply->abort();
    }
    recordStatus->setText("采集状态：已停止");
    QSqlDatabase db = QSqlDatabase::database(
        QSqlDatabase::defaultConnectionName(), false
        );
    ui->actionStart->setEnabled(db.isOpen());
    ui->actionStop->setEnabled(false);
    ui->textEdit->append(
        QString("[%1] 停止采集")
            .arg(QDateTime::currentDateTime()
                     .toString("yyyy-MM-dd HH:mm:ss"))
        );
}

//immediate record
void MainWindow::on_actionImmediate_triggered()
{
    this->collectOnce();
}

void MainWindow::collectOnce()
{
    if (currentReply != nullptr) {
        return;
    }
    if (retryTimer->isActive()) {
        retryTimer->stop();
        retryCount = 0;
    }
    const QUrl url(weatherTargetUrl);
    const bool usesObservationApi = url.host() == "d1.weather.com.cn"
                                    && url.path().startsWith("/sk_2d/");
    QNetworkRequest request{url};
    request.setTransferTimeout(10000);
    if (usesObservationApi) {
        request.setRawHeader("Referer", "https://www.weather.com.cn/");
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                           "AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36");
    }
    QNetworkReply *reply = collectManager->get(request);
    currentReply = reply;
    connect(reply, &QNetworkReply::finished,
            this, [this, reply, url, usesObservationApi, requestedCityCode = weatherCityCode]()
            {
                currentReply = nullptr;
        //user cancle the request
                if (reply->error() == QNetworkReply::OperationCanceledError)
                {
                    reply->deleteLater();
                    return;
                }
                //network error cancle the request
                if (reply->error() != QNetworkReply::NoError) {
                    QString error = reply->errorString();
                    reply->deleteLater();
                    if(retryCount<3){
                        collectTimer->stop();
                        ++retryCount;
                        int delay = (1<<(retryCount-1))*1000;
                        recordStatus->setText(QString("采集状态：重试中 %1/3").arg(retryCount));
                        ui->textEdit->append(QString("[%1] 网络采集失败：%2，重试 %3/3").arg(QDateTime::currentDateTime()
                                .toString("yyyy-MM-dd HH:mm:ss"))
                                .arg(error)
                                .arg(retryCount)
                            );
                        retryTimer->start(delay);
                    }
                    else{
                        ui->textEdit->append(QString("[%1] 网络采集失败：%2，3次重试全部失败，停止采集")
                                                 .arg(QDateTime::currentDateTime()
                                                          .toString("yyyy-MM-dd HH:mm:ss"))
                                                 .arg(error));
                        on_actionStop_triggered();
                    }
                    return;
                }
                //success request
                retryCount =0;
                if(isCollecting&&!collectTimer->isActive()){
                    collectTimer->start();
                }
                recordStatus->setText(isCollecting?"采集状态：采集中":"采集状态：已停止");

                const QDateTime collectedAt = QDateTime::currentDateTime();
                const QString source = reply->url().toString();
                QByteArray data = reply->readAll();
                reply->deleteLater();

                const auto saveParseError = [&](const QString &reason, const QString &cityName) {
                    ui->textEdit->append(QString("[%1] PARSE_ERR：%2（城市代码 %3）")
                            .arg(collectedAt.toString("yyyy-MM-dd HH:mm:ss"), reason, requestedCityCode));
                    const QVariant missingNumber(QMetaType::fromType<double>());
                    emit saveWeatherRequested(url.toString(), cityName, requestedCityCode,
                                              missingNumber, missingNumber, QString(), QString(),
                                              collectedAt, QStringLiteral("PARSE_ERR"));
                };

                if (usesObservationApi) {
                    data = data.trimmed();
                    const QByteArray prefix("var dataSK=");
                    if (!data.startsWith(prefix)) {
                        saveParseError("响应缺少 dataSK 实况数据", QString());
                        return;
                    }
                    data.remove(0, prefix.size());
                    if (data.endsWith(';')) {
                        data.chop(1);
                    }
                }

                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
                if (parseError.error != QJsonParseError::NoError) {
                    saveParseError("JSON解析失败：" + parseError.errorString(), QString());
                    return;
                }
                if (!document.isObject()) {
                    saveParseError("JSON 顶层不是对象", QString());
                    return;
                }

                const QJsonObject root = document.object();
                if (!usesObservationApi && !root.value("weatherinfo").isObject()) {
                    saveParseError("响应缺少 weatherinfo 对象", QString());
                    return;
                }
                const QJsonObject weather = usesObservationApi ? root : root.value("weatherinfo").toObject();
                const QString city = weather.value(usesObservationApi ? "cityname" : "city").toString();
                const QString cityCode = weather.value(usesObservationApi ? "city" : "cityid").toString();
                const QString tempText = weather.value("temp").toString();
                QString humidityText = weather.value("SD").toString();
                const QString windDirection = weather.value("WD").toString();
                const QString windForce = weather.value("WS").toString();
                humidityText.remove('%');

                bool tempOk = false;
                bool humidityOk = false;
                const double temperature = tempText.toDouble(&tempOk);
                const double humidity = humidityText.toDouble(&humidityOk);
                if (usesObservationApi && !cityCode.isEmpty() && cityCode != requestedCityCode) {
                    saveParseError("响应城市代码与请求不符：" + cityCode, QString());
                    return;
                }
                if (city.isEmpty() || cityCode.isEmpty() || !tempOk || !humidityOk
                    || windDirection.isEmpty() || windForce.isEmpty()) {
                    saveParseError("天气字段缺失或格式错误", cityCode == requestedCityCode ? city : QString());
                    return;
                }
                ui->textEdit->append(
                    QString("[%1] 天气解析成功：%2 %3℃ 湿度%4% %5 %6")
                        .arg(collectedAt.toString("yyyy-MM-dd HH:mm:ss"))
                        .arg(city)
                        .arg(temperature)
                        .arg(humidity)
                        .arg(windDirection)
                        .arg(windForce)
                    );

                emit saveWeatherRequested(source, city, cityCode, QVariant(temperature), QVariant(humidity),
                                          windDirection, windForce, collectedAt, QStringLiteral("OK"));

            });
}



//Mysql:query
void MainWindow::on_pushButton_query_clicked()
{
    queryFilter tmpFilter;
    tmpFilter.cityName = ui->lineEdit_cityName_query->text().trimmed();
    tmpFilter.queryStatus = ui->comboBox_queryStatus->currentData().toString();
    tmpFilter.useDateRange = ui->checkBox_dataChange->isChecked();
    if(tmpFilter.useDateRange){
        tmpFilter.startTime = ui->dateTimeEdit_start->dateTime();
        tmpFilter.endTime = ui->dateTimeEdit_end->dateTime();
    }
    this->queryData(tmpFilter);
}
void MainWindow::on_actionQuery_triggered()
{
    queryFilter tmpFilter;
    tmpFilter.cityName = ui->lineEdit_cityName_query->text().trimmed();
    tmpFilter.queryStatus = ui->comboBox_queryStatus->currentData().toString();
    tmpFilter.useDateRange = ui->checkBox_dataChange->isChecked();
    if(tmpFilter.useDateRange){
        tmpFilter.startTime = ui->dateTimeEdit_start->dateTime();
        tmpFilter.endTime = ui->dateTimeEdit_end->dateTime();
    }
    this->queryData(tmpFilter);
}
void MainWindow::queryData(const queryFilter&filter,std::optional<qint64> id)
{
    QString sql = "SELECT id, source, city_name, city_code, temperature, humidity, "
                  "wind_direction, wind_force, collected_at, status "
                  "FROM weather WHERE 1=1";
    QMap<QString, QVariant> params;
    QString city = filter.cityName;
    if(!city.isEmpty()){
        sql += " AND city_name LIKE :city_name";
        params.insert(":city_name", QString("%%1%").arg(city));
    }
    QString status  =filter.queryStatus;
    if (!status.isEmpty() && status != "全部") {
        sql += " AND status = :status";
        params.insert(":status", status);
    }
    if(filter.useDateRange){
        sql += " AND collected_at BETWEEN :start_time AND :end_time";
        params.insert(":start_time", filter.startTime);
        params.insert(":end_time", filter.endTime);
    }
    //optional id
    if(id.has_value())
    {
        sql +=" AND id = :id";
        params.insert(":id",id.value());
    }

   sql += " ORDER BY collected_at DESC,id DESC";
    QSqlQuery query;
   query.prepare(sql);
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }
    if(!query.exec())
    {
        QMessageBox::warning(this,"warnning","Failed to query the data");
        return;
    }
    if(id.has_value()){
        //insert
        insertModelRow(query);
    }
    else{
        //fill
        this->filter =filter;
        fillModel(query);
    }

}
void MainWindow::clearRecords()
{
    ui->tableView_records->selectionModel()->clear();

    ui->lineEdit_id->clear();
    ui->lineEdit_source->clear();
    ui->lineEdit_cityName_record->clear();
    ui->lineEdit_cityCode->clear();
    ui->lineEdit_windDirection->clear();
    ui->lineEdit_windForce->clear();
    ui->doubleSpinBox_temp->clear();
    ui->doubleSpinBox_humidity->clear();
    ui->dateTimeEdit_collectedAt->clear();
    ui->comboBox_status->setCurrentIndex(-1);
    model->removeRows(0, model->rowCount());
}

void MainWindow::fillModel(QSqlQuery &query)
{
    this->clearRecords();
    while (query.next()) {
        QList<QStandardItem *> row;
        for (int col = 0; col < 10; ++col) {
            auto *item = new QStandardItem;
            item->setEditable(false);
            if (col == 8) {
                QDateTime time = query.value(col).toDateTime();

                item->setText(
                    time.toString("yyyy-MM-dd HH:mm:ss")
                    );

                item->setData(time, Qt::UserRole);
            }
            else {
                item->setData(query.value(col), Qt::EditRole);
            }
            row.append(item);
        }
        model->appendRow(row);
    }
    if(model->rowCount()==0){
        QMessageBox::information(this,"Query outcome","No records meet the requirements.");
    }
}

void MainWindow::insertModelRow(QSqlQuery &query)
{
    if (!query.next()) {
        return;
    }

    qint64 newId = query.value(0).toLongLong();
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::EditRole).toLongLong() == newId) {
            return;
        }
    }
    QDateTime newTime = query.value(8).toDateTime();
    //identify the insert position
    int position = 0;
    while (position < model->rowCount()) {

        qint64 existingId =
            model->index(position, 0)
                .data(Qt::EditRole).toLongLong();

        QDateTime existingTime =
            model->index(position, 8)
                .data(Qt::UserRole).toDateTime();
        if (newTime > existingTime ||
            (newTime == existingTime && newId > existingId))
        {
            break;
        }

        ++position;
    }
    QList<QStandardItem *> row;
    for (int col = 0; col < 10; ++col) {

        auto *item = new QStandardItem;
        item->setEditable(false);

        if (col == 8) {
            QDateTime time = query.value(col).toDateTime();

            item->setText(
                time.toString("yyyy-MM-dd HH:mm:ss")
                );

            item->setData(time, Qt::UserRole);
        }
        else {
            item->setData(query.value(col), Qt::EditRole);
        }

        row.append(item);
    }
    model->insertRow(position, row);
}


//Mysql::modify
void MainWindow::on_tableView_records_clicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    int row = index.row();
    ui->lineEdit_id->setText(
        model->index(row, 0).data(Qt::EditRole).toString()
        );

    ui->lineEdit_source->setText(
        model->index(row, 1).data(Qt::EditRole).toString()
        );
    ui->lineEdit_cityName_record->setText(
        model->index(row, 2).data(Qt::EditRole).toString()
        );
    ui->lineEdit_cityCode->setText(
        model->index(row, 3).data(Qt::EditRole).toString()
        );
    const auto setMeasurement = [](QDoubleSpinBox *field, const QVariant &value, double minimum) {
        if (value.isNull()) {
            field->setMinimum(minimum - 0.01);
            field->setSpecialValueText(QStringLiteral("未采集"));
            field->setValue(field->minimum());
        } else {
            field->setMinimum(minimum);
            field->setSpecialValueText(QString());
            field->setValue(value.toDouble());
        }
    };
    setMeasurement(ui->doubleSpinBox_temp, model->index(row, 4).data(Qt::EditRole), -999.99);
    setMeasurement(ui->doubleSpinBox_humidity, model->index(row, 5).data(Qt::EditRole), 0.0);
    ui->lineEdit_windDirection->setText(
        model->index(row, 6).data(Qt::EditRole).toString()
        );
    ui->lineEdit_windForce->setText(
        model->index(row, 7).data(Qt::EditRole).toString()
        );

    ui->dateTimeEdit_collectedAt->setDateTime(
        model->index(row, 8).data(Qt::UserRole).toDateTime()
        );
    QString status = model->index(row, 9)
                         .data(Qt::EditRole)
                         .toString()
                         .trimmed();

    int statusIndex = ui->comboBox_status->findData(status);
    ui->comboBox_status->setCurrentIndex(statusIndex);
}


void MainWindow::on_pushButton_modify_clicked()
{
    const auto selectedRows = ui->tableView_records->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this,"Warnning","Need to select one record");
        return;
    }
    const qint64 id = selectedRows.constFirst().data(Qt::EditRole).toLongLong();

    QString source = ui->lineEdit_source->text().trimmed();
    QString cityName = ui->lineEdit_cityName_record->text().trimmed();
    QString cityCode = ui->lineEdit_cityCode->text().trimmed();
    const auto measurement = [](const QDoubleSpinBox *field) {
        if (!field->specialValueText().isEmpty() && field->value() == field->minimum()) {
            return QVariant(QMetaType::fromType<double>());
        }
        return QVariant(field->value());
    };
    const QVariant temp = measurement(ui->doubleSpinBox_temp);
    const QVariant humidity = measurement(ui->doubleSpinBox_humidity);
    QString windDirection = ui->lineEdit_windDirection->text().trimmed();
    QString windForce = ui->lineEdit_windForce->text().trimmed();
    QDateTime collectedAt = ui->dateTimeEdit_collectedAt->dateTime();
    QString status = ui->comboBox_status->currentData().toString();
    if (source.isEmpty() || cityName.isEmpty() || cityCode.isEmpty() || status.isEmpty()) {
        QMessageBox::warning(this, "Warnning", "There is some blank line");
        return;
    }
    QSqlQuery query;
    QString sql = "UPDATE weather "
                  "SET source = :source, "
                  "    city_name = :city_name, "
                  "    city_code = :city_code, "
                  "    temperature = :temperature, "
                  "    humidity = :humidity, "
                  "    wind_direction = :wind_direction, "
                  "    wind_force = :wind_force, "
                  "    collected_at = :collected_at, "
                  "    status = :status "
                  "WHERE id = :id";
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
    query.bindValue(":id",             id);
    if(!query.exec()){
        QMessageBox::warning(this,"Warnning","Failed to modify: "+query.lastError().text());
        return;
    }
    QMessageBox::information(this,"Success","Success to modify!");
    this->queryData(this->filter);
    ui->textEdit->append(QString("[%1] %2 修改成功").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg(id));
}

//Mysql::delete

void MainWindow::on_pushButton_delete_clicked()
{
    const auto selectedRows = ui->tableView_records->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Need to select one record");
        return;
    }
    const qint64 id = selectedRows.constFirst().data(Qt::EditRole).toLongLong();
    const quint64 revision = databaseRevision;
    auto result = QMessageBox::question(
        this,
        "Confirm Delete",
        "Are you sure you want to delete this record?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (result != QMessageBox::Yes || revision != databaseRevision) {
        return;
    }
    QSqlQuery query;
    query.prepare("DELETE FROM weather WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        QMessageBox::warning(this, "Warning",
                             "Failed to delete: " + query.lastError().text());
        return;
    }
    QMessageBox::information(this, "Success", "Deleted successfully!");
    this->queryData(this->filter);
    ui->textEdit->append(QString("[%1] %2 删除成功").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg(id));

    ui->tableView_records->selectionModel()->clear();
    ui->lineEdit_id->clear();
    ui->lineEdit_source->clear();
    ui->lineEdit_cityName_record->clear();
    ui->lineEdit_cityCode->clear();
    ui->lineEdit_windDirection->clear();
    ui->lineEdit_windForce->clear();
    ui->doubleSpinBox_temp->clear();
    ui->doubleSpinBox_humidity->clear();
    ui->dateTimeEdit_collectedAt->clear();
    ui->comboBox_status->setCurrentIndex(-1);
}

//Mysql::manual_add


void MainWindow::on_pushButton_manualRecord_clicked()
{
    AddDialog adddialog(this);
    if(adddialog.exec() == QDialog::Accepted)
    {
        qint64 id = adddialog.getId();
        QString cityName =  adddialog.getCityName();
        ui->textEdit->append( QString("[%1] 手工补录成功：记录ID=%2，城市=%3")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                .arg(id)
                .arg(cityName)
            );
        this->queryData(this->filter);
    }
}
//Mysql::detail
void MainWindow::on_pushButton_detail_clicked()
{
    QModelIndex index =ui->tableView_records->selectionModel()->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(
            this,
            "提示",
            "请先选择一条天气记录"
            );
        return;
    }
    qint64 id = model->index(index.row(), 0)
                    .data(Qt::EditRole).toLongLong();
    DetailDialog detail(id, this);
    detail.exec();
}

//export csv
void MainWindow::on_actionExport_data_triggered()
{
    if (model->rowCount() == 0) {
        QMessageBox::information(this, "提示", "当前没有可导出的天气记录");
        return;
    }
    QString time = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = QFileDialog::getSaveFileName(this,"导出天气数据",QString("weather_%1.csv").arg(time),"CSV文件(*.csv)");
    if(fileName.isEmpty()){
        return;
    }
    if (!fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        fileName += ".csv";
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(
            this,
            "导出失败",
            file.errorString()
            );
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    auto csvField = [](QString value) -> QString {
        if (value.contains(',') ||
            value.contains('"') ||
            value.contains('\n') ||
            value.contains('\r'))
        {
            value.replace("\"", "\"\"");
            value = "\"" + value + "\"";
        }
        return value;
    };
    QStringList headers;
    for (int col = 0; col < model->columnCount(); ++col) {
        QString header = model->headerData(
                                  col,
                                  Qt::Horizontal,
                                  Qt::DisplayRole
                                  ).toString();
        headers.append(csvField(header));
    }

    out << headers.join(',') << '\n';
    for (int row = 0; row < model->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < model->columnCount(); ++col) {
            QString value = model->index(row, col)
            .data(Qt::DisplayRole)
                .toString();

            fields.append(csvField(value));
        }
        out << fields.join(',') << '\n';
    }
    out.flush();
    file.close();
    QMessageBox::information(this,"导出成功",QString("成功导出 %1 条天气记录。\n保存位置：%2")
            .arg(model->rowCount())
            .arg(fileName)
        );
}

//data statistics
void MainWindow::on_actionStatistics_triggered()
{
    int totalCount = model->rowCount();

    if (totalCount == 0) {
        QMessageBox::information(
            this,
            "数据统计",
            "当前没有符合条件的天气记录"
            );
        return;
    }

    double totalTemp = 0.0;
    double totalHumidity = 0.0;
    double maxTemp = 0.0;
    double minTemp = 0.0;

    int tempCount = 0;
    int humidityCount = 0;

    int successCount = 0;
    int parseErrorCount = 0;
    int dbErrorCount = 0;
    for (int row = 0; row < totalCount; ++row) {
        QVariant tempValue =  model->index(row, 4).data(Qt::EditRole);

        if (!tempValue.isNull()) {
            bool ok = false;
            double temp = tempValue.toDouble(&ok);

            if (ok) {
                totalTemp += temp;

                if (tempCount == 0) {
                    maxTemp = temp;
                    minTemp = temp;
                }
                else {
                    if (temp > maxTemp) {
                        maxTemp = temp;
                    }

                    if (temp < minTemp) {
                        minTemp = temp;
                    }
                }

                ++tempCount;
            }
        }

        QVariant humidityValue =model->index(row, 5).data(Qt::EditRole);
        if (!humidityValue.isNull()) {
            bool ok = false;
            double humidity = humidityValue.toDouble(&ok);
            if (ok) {
                totalHumidity += humidity;
                ++humidityCount;
            }
        }
        QString status =model->index(row, 9).data(Qt::DisplayRole).toString();
        if (status == "OK") {
            ++successCount;
        }
        else if (status == "PARSE_ERR") {
            ++parseErrorCount;
        }
        else if (status == "DB_ERR") {
            ++dbErrorCount;
        }
    }
    int failCount = parseErrorCount + dbErrorCount;
    QString avgTemp = tempCount > 0 ? QString::number(totalTemp / tempCount, 'f', 2): "无有效数据";
    QString highestTemp = tempCount > 0 ? QString::number(maxTemp, 'f', 2): "无有效数据";
    QString lowestTemp = tempCount > 0   ? QString::number(minTemp, 'f', 2): "无有效数据";
    QString avgHumidity = humidityCount > 0 ? QString::number(totalHumidity / humidityCount, 'f', 2): "无有效数据";
    QString result =QString("当前筛选结果统计\n\n"
                "记录总数：%1 条\n\n"
                "平均温度：%2 ℃\n"
                "最高温度：%3 ℃\n"
                "最低温度：%4 ℃\n"
                "平均湿度：%5 %%\n\n"
                "采集成功：%6 条\n"
                "采集失败：%7 条\n"
                "其中解析异常：%8 条\n"
                "其中入库失败：%9 条")
            .arg(totalCount)
            .arg(avgTemp)
            .arg(highestTemp)
            .arg(lowestTemp)
            .arg(avgHumidity)
            .arg(successCount)
            .arg(failCount)
            .arg(parseErrorCount)
            .arg(dbErrorCount);
    QMessageBox::information( this,"天气数据统计", result);
}

//reffrence

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(
        this,
        "关于",
        "程序名称：天气数据采集系统\n"
        "版本：1.0\n"
        "作者学号：2024141460490\n"
        "版权信息：aaa批发"
        );
}

//exit
void MainWindow::on_actionExit_triggered()
{
    trayIcon->hide();
    QCoreApplication::quit();
}

//Show mainwindow
void MainWindow::on_actionShowMain_triggered()
{
    this->showNormal();
    this->raise();
    this->activateWindow();
}
//hide

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange
        && this->isMinimized())
    {
        this->hide();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    this->hide();
}

