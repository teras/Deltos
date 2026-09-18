#pragma once
#include <QDir>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTcpServer>
#include <QUdpSocket>

namespace deltos {

// Receives photos on the LAN: from the LocalSend app (https://localsend.org, protocol v2)
// and from a browser through the upload page served at /. Plain HTTP; files are written
// to a temporary directory and reported through fileReceived(). The multicast group and
// port are the protocol defaults; if the TCP port is taken (e.g. by the LocalSend desktop
// app) the next free one is used and discovery relies on multicast alone.
class LocalSendReceiver : public QObject {
    Q_OBJECT
public:
    explicit LocalSendReceiver(QObject* parent = nullptr);
    ~LocalSendReceiver() override;

    bool start();
    void stop();
    bool isRunning() const { return server_.isListening(); }
    quint16 port() const { return port_; }
    QString alias() const { return alias_; }
    QString url() const;   // upload page for a phone browser, e.g. http://192.168.1.5:53317/

signals:
    void fileReceived(const QString& path);

private:
    struct Request { QString method, path; QHash<QString, QString> query; QByteArray body; };
    struct Session { QString peer; QHash<QString, QString> tokens; QHash<QString, QString> names; };

    void readDatagrams();
    void announce(bool announce);
    void newConnection();
    void handle(QTcpSocket* sock, const Request& req);
    void respond(QTcpSocket* sock, int code, const QByteArray& body = {}, const char* type = "application/json");
    QString save(const QString& name, const QByteArray& data);
    QJsonObject info() const;

    QTcpServer server_;
    QUdpSocket udp_;
    QNetworkAccessManager nam_;
    QString alias_, fingerprint_;
    quint16 port_ = 0;
    QDir dir_;
    QHash<QString, Session> sessions_;
};

} // namespace deltos
