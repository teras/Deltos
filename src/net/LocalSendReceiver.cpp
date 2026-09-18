#include "LocalSendReceiver.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QUuid>

namespace deltos {

static const quint16 kPort = 53317;
static const QHostAddress kGroup("224.0.0.167");
static const QString kApi = "/api/localsend/v2/";

LocalSendReceiver::LocalSendReceiver(QObject* parent) : QObject(parent) {
    alias_ = QString("Deltos (%1)").arg(QSysInfo::machineHostName());
    fingerprint_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dir_ = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/deltos-" +
                QString::number(QCoreApplication::applicationPid()));
    connect(&server_, &QTcpServer::newConnection, this, &LocalSendReceiver::newConnection);
    connect(&udp_, &QUdpSocket::readyRead, this, &LocalSendReceiver::readDatagrams);
}

LocalSendReceiver::~LocalSendReceiver() {
    if (dir_.exists()) dir_.removeRecursively();
}

bool LocalSendReceiver::start() {
    for (quint16 p = kPort; p < kPort + 10 && !server_.isListening(); ++p)
        if (server_.listen(QHostAddress::AnyIPv4, p)) port_ = p;
    if (!server_.isListening()) return false;
    if (udp_.bind(QHostAddress::AnyIPv4, kPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces())
            if (iface.flags().testFlag(QNetworkInterface::CanMulticast) && iface.flags().testFlag(QNetworkInterface::IsUp))
                udp_.joinMulticastGroup(kGroup, iface);
        announce(true);
    }
    return true;
}

void LocalSendReceiver::stop() {
    server_.close();
    udp_.close();
    sessions_.clear();
    port_ = 0;
}

QString LocalSendReceiver::url() const {
    for (const QHostAddress& a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && !a.isLinkLocal())
            return QString("http://%1:%2/").arg(a.toString()).arg(port_);
    return QString("http://<this computer>:%1/").arg(port_);
}

QJsonObject LocalSendReceiver::info() const {
    return {{"alias", alias_}, {"version", "2.1"}, {"deviceModel", QSysInfo::prettyProductName()},
            {"deviceType", "desktop"}, {"fingerprint", fingerprint_}, {"port", port_},
            {"protocol", "http"}, {"download", false}};
}

void LocalSendReceiver::announce(bool announce) {
    QJsonObject o = info();
    o["announce"] = announce;
    udp_.writeDatagram(QJsonDocument(o).toJson(QJsonDocument::Compact), kGroup, kPort);
}

// A phone announcing itself: reply by registering with it (HTTP) and by multicast.
void LocalSendReceiver::readDatagrams() {
    while (udp_.hasPendingDatagrams()) {
        QByteArray data(int(udp_.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress from;
        udp_.readDatagram(data.data(), data.size(), &from);
        const QJsonObject o = QJsonDocument::fromJson(data).object();
        if (o["fingerprint"].toString() == fingerprint_ || !o["announce"].toBool()) continue;
        const QString url = QString("%1://%2:%3%4register").arg(o["protocol"].toString("http"),
                                                                from.toString(), QString::number(o["port"].toInt(kPort)), kApi);
        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply* reply = nam_.post(req, QJsonDocument(info()).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::sslErrors, reply, [reply] { reply->ignoreSslErrors(); }); // self-signed peers
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
        announce(false);
    }
}

// Decodes a complete chunked body; false if more data is needed.
static bool dechunk(const QByteArray& in, QByteArray& out) {
    int pos = 0;
    out.clear();
    for (;;) {
        const int eol = in.indexOf("\r\n", pos);
        if (eol < 0) return false;
        bool ok;
        const int size = in.mid(pos, eol - pos).split(';')[0].trimmed().toInt(&ok, 16);
        if (!ok) return true; // malformed: stop with what we have
        if (in.size() < eol + 2 + size + 2) return false;
        if (size == 0) return true;
        out.append(in.mid(eol + 2, size));
        pos = eol + 2 + size + 2;
    }
}

// Minimal HTTP/1.1: one request per connection, body sized by Content-Length or chunked.
void LocalSendReceiver::newConnection() {
    while (QTcpSocket* sock = server_.nextPendingConnection()) {
        auto* buf = new QByteArray;
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        connect(sock, &QObject::destroyed, sock, [buf] { delete buf; });
        connect(sock, &QTcpSocket::readyRead, sock, [this, sock, buf] {
            buf->append(sock->readAll());
            const int end = buf->indexOf("\r\n\r\n");
            if (end < 0) return;
            const QList<QByteArray> lines = buf->left(end).split('\n');
            qint64 length = 0;
            bool chunked = false;
            for (const QByteArray& l : lines) {
                const QByteArray lower = l.toLower().trimmed();
                if (lower.startsWith("content-length:")) length = lower.mid(15).trimmed().toLongLong();
                if (lower.startsWith("transfer-encoding:") && lower.contains("chunked")) chunked = true;
            }
            QByteArray body;
            if (chunked) {
                if (!dechunk(buf->mid(end + 4), body)) return;
            } else {
                if (buf->size() < end + 4 + length) return;
                body = buf->mid(end + 4, int(length));
            }
            if (qEnvironmentVariableIsSet("DELTOS_DEBUG"))
                fprintf(stderr, "localsend: %s (%lld bytes)\n", buf->left(end).replace("\r\n", " | ").constData(), qint64(body.size()));
            const QList<QByteArray> first = lines[0].trimmed().split(' ');
            if (first.size() < 2) { respond(sock, 400); return; }
            const QUrl url = QUrl::fromEncoded(first[1]);
            Request req{QString::fromLatin1(first[0]), url.path(), {}, body};
            for (const auto& [k, v] : QUrlQuery(url).queryItems()) req.query[k] = v;
            buf->clear();
            handle(sock, req);
        });
    }
}

void LocalSendReceiver::respond(QTcpSocket* sock, int code, const QByteArray& body, const char* type) {
    static const QHash<int, QByteArray> reason = {{200, "OK"}, {204, "No Content"}, {400, "Bad Request"},
                                                  {403, "Forbidden"}, {404, "Not Found"}, {500, "Internal Server Error"}};
    sock->write("HTTP/1.1 " + QByteArray::number(code) + " " + reason.value(code, "Error") + "\r\n"
                "Content-Type: " + QByteArray(type) + "\r\nContent-Length: " + QByteArray::number(body.size()) +
                "\r\nConnection: close\r\n\r\n" + body);
    sock->disconnectFromHost();
}

// Browser upload: a page with a file picker that POSTs each photo as a raw body.
static const char* kPage = R"(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deltos</title>
<style>
body{font-family:-apple-system,sans-serif;margin:0;padding:2em;text-align:center;background:#f4f4f4}
label{display:block;background:#2b6cb0;color:#fff;font-size:1.4em;padding:1em;border-radius:12px;margin:2em auto;max-width:20em}
input{display:none}#log{text-align:left;max-width:30em;margin:auto;font-size:1.1em}
</style></head><body>
<h1>Deltos</h1>
<label>Choose photos<input type="file" accept="image/*" multiple></label>
<div id="log"></div>
<script>
const log=document.getElementById('log');
document.querySelector('input').onchange=async e=>{
  for(const f of e.target.files){
    const line=document.createElement('div');line.textContent=f.name+' … ';log.appendChild(line);
    try{
      const r=await fetch('/upload?name='+encodeURIComponent(f.name),{method:'POST',body:f});
      line.textContent=f.name+(r.ok?' ✓':' ✗ '+r.status);
    }catch(err){line.textContent=f.name+' ✗ '+err;}
  }
  e.target.value='';
};
</script></body></html>)";

static bool isImage(const QJsonObject& file) {
    static const QStringList exts = {"jpg", "jpeg", "png", "heic", "heif", "tif", "tiff", "webp", "bmp"};
    return file["fileType"].toString().startsWith("image/") ||
           exts.contains(QFileInfo(file["fileName"].toString()).suffix().toLower());
}

QString LocalSendReceiver::save(const QString& name, const QByteArray& data) {
    dir_.mkpath(".");
    const QFileInfo fi(name.isEmpty() ? QStringLiteral("photo.jpg") : name);
    QString path = dir_.filePath(fi.fileName());
    for (int n = 2; QFile::exists(path); ++n)
        path = dir_.filePath(fi.completeBaseName() + "-" + QString::number(n) + "." + fi.suffix());
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) return {};
    return path;
}

void LocalSendReceiver::handle(QTcpSocket* sock, const Request& req) {
    const auto json = [](const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); };

    if (req.path == "/" && req.method == "GET") { respond(sock, 200, kPage, "text/html; charset=utf-8"); return; }
    if (req.path == "/upload" && req.method == "POST") {
        const QString path = save(QFileInfo(req.query.value("name")).fileName(), req.body);
        if (path.isEmpty()) { respond(sock, 500); return; }
        respond(sock, 200);
        emit fileReceived(path);
        return;
    }

    if (!req.path.startsWith(kApi)) { respond(sock, 404); return; }
    const QString route = req.path.mid(kApi.size());

    if (route == "info" && req.method == "GET") { respond(sock, 200, json(info())); return; }
    if (req.method != "POST") { respond(sock, 404); return; }

    if (route == "register") { respond(sock, 200, json(info())); return; }

    if (route == "prepare-upload") {
        const QJsonObject files = QJsonDocument::fromJson(req.body).object()["files"].toObject();
        if (files.isEmpty()) { respond(sock, 400); return; }
        Session s;
        s.peer = sock->peerAddress().toString();
        QJsonObject tokens;
        for (auto it = files.begin(); it != files.end(); ++it) {
            const QJsonObject f = it.value().toObject();
            if (!isImage(f)) continue;
            const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
            s.tokens[it.key()] = token;
            s.names[it.key()] = QFileInfo(f["fileName"].toString()).fileName();
            tokens[it.key()] = token;
        }
        if (tokens.isEmpty()) { respond(sock, 403); return; }
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        sessions_[id] = s;
        respond(sock, 200, json({{"sessionId", id}, {"files", tokens}}));
        return;
    }

    if (route == "upload") {
        const auto it = sessions_.find(req.query.value("sessionId"));
        const QString fileId = req.query.value("fileId");
        if (it == sessions_.end() || !it->tokens.contains(fileId)) { respond(sock, 400); return; }
        if (it->tokens[fileId] != req.query.value("token") || it->peer != sock->peerAddress().toString()) {
            respond(sock, 403);
            return;
        }
        const QString path = save(it->names[fileId], req.body);
        if (path.isEmpty()) { respond(sock, 500); return; }
        it->tokens.remove(fileId);
        if (it->tokens.isEmpty()) sessions_.erase(it);
        respond(sock, 200);
        emit fileReceived(path);
        return;
    }

    if (route == "cancel") {
        sessions_.remove(req.query.value("sessionId"));
        respond(sock, 200);
        return;
    }
    respond(sock, 404);
}

} // namespace deltos
