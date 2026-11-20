
#include "mainwindow.h"

#include <QApplication>
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QAction>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setQuitOnLastWindowClosed(false);

    const QString instanceKey = "WeatherCollector_SingleInstance";
    const QString serverName = "WeatherCollector_Activate";

    // Local communication endpoint
    QLocalServer server;
    server.listen(serverName);

    // Single-instance ownership
    QSharedMemory sharedMemory(instanceKey);

    if (!sharedMemory.create(1)) {

        if (sharedMemory.error() == QSharedMemory::AlreadyExists) {

            // Second instance: close its own server endpoint
            server.close();

            // Notify the existing instance
            QLocalSocket socket;
            socket.connectToServer(serverName);
            socket.waitForConnected(3000);

            return 0;
        }

        return 1;
    }

    if (!server.isListening()) {
        return 1;
    }

    // First instance
    MainWindow w;

    QAction *showAction =
        w.findChild<QAction *>("actionShowMain");

    // Receive activation request from another instance
    QObject::connect(
        &server,
        &QLocalServer::newConnection,
        &w,
        [&server, showAction]()
        {
            while (server.hasPendingConnections()) {

                QLocalSocket *client =
                    server.nextPendingConnection();

                showAction->trigger();

                client->disconnectFromServer();
                client->deleteLater();
            }
        }
        );

    w.show();

    return a.exec();
}
