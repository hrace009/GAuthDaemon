#include "AuthClient.h"

AuthClient::AuthClient(qintptr socketDescriptor, QObject *parent) : QThread(parent)
{
    this->socketDescriptor = socketDescriptor;
}

void AuthClient::run()
{
    rc4 = RC4EncDec(Settings::iseckey, Settings::oseckey);
    this->socket = new QTcpSocket();
    connect(socket, &QTcpSocket::readyRead, this, &AuthClient::readyRead, Qt::DirectConnection);
    connect(socket, &QTcpSocket::disconnected, this, &AuthClient::disconnected, Qt::DirectConnection);
    socket->setSocketDescriptor(socketDescriptor);
    goldTimer = new QTimer();
    goldTimer->setInterval(Settings::gold_timeout * 1000);
    connect(goldTimer, &QTimer::timeout, this, &AuthClient::goldChecker, Qt::DirectConnection);
    goldTimer->moveToThread(this);
    goldTimer->start();
    exec();
}

void AuthClient::readyRead()
{
    QByteArray data = socket->readAll();
    //data = rc4.Decrypt(data);
    OctetsStream stream(data, false);
    uint OpCode = stream.uncompact_uint32();
    uint Lenght = stream.uncompact_uint32();
    int id = QMetaType::type(QString::number(OpCode).toStdString().c_str());
    if (id != QMetaType::UnknownType)
    {
        void *ptr = QMetaType::create(id);
        OctetsStream *stream = static_cast<OctetsStream*>(ptr);
        stream->setData(data);
        stream->socket = socket;
        stream->Unmarshal();
        free(ptr);
    }
    else
    {
        Utils::print(QString("Unknown protocol %1, size %2").arg(OpCode).arg(Lenght));
    }
}

void AuthClient::disconnected()
{
    Utils::print("AuthServer::OnDisconnected");
    socket->deleteLater();
    exit(1);
}

void AuthClient::goldChecker()
{
    Database *db = Database::Instance();
    QList<QVariantList> cashnow = db->getUseCashNow(0);
    for (int i = 0; i < cashnow.length(); ++i)
    {
        QVariantList now = cashnow[i];
        OctetsStream *stream = new OctetsStream();
        stream->socket = socket;
        stream->writeInt32(-1);
        stream->writeInt32(now[0].toInt());
        stream->writeInt32(now[1].toInt());
        stream->Send(514);
        Utils::print(QString("GetAddCashSN userid: %1, zoneid: %2").arg(now[0].toInt()).arg(now[1].toInt()));
    }
}
