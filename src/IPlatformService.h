#pragma once
#include <QObject>
#include <QString>

class IPlatformService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IPlatformService() = default;

    virtual void updateCategory(const QString &gameName, const QString &title = QString()) = 0;
    virtual void sendChatMessage(const QString &message) = 0;
    virtual bool isAuthenticated() const = 0;

signals:
    void categoryUpdateFinished(bool success, QString gameName, QString errorMsg);
    void messageSent(bool success, QString message);
};
