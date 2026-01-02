#ifndef PLATFORMMANAGER_H
#define PLATFORMMANAGER_H

#pragma once
#include <QObject>
#include <QFutureWatcher>
#include <QTimer>

template <typename T>
class QFutureWatcher;

class PlatformManager : public QObject {
    Q_OBJECT

public:
    static PlatformManager &get()
    {
        static PlatformManager instance;
        return instance;
    }

    bool sendChatMessage(const QString &message);
    bool updateCategory(const QString &gameName);
    void shutdown();
    bool isOnCooldown() const;
    int getCooldownRemaining() const;
    void setLastSetCategory(const QString &categoryName);
    QString getLastSetCategory() const;

private:
    PlatformManager();
    ~PlatformManager();

    void setCooldown();
    QTimer *cooldownTimer;
    bool onCooldown = false;

    QString lastSetCategoryName;
    
    QFutureWatcher<QString> *gameIdWatcher;
    QFutureWatcher<bool> *chatMessageWatcher;
    QFutureWatcher<void *> *categoryUpdateWatcher;

signals:
    void categoryUpdateFinished(bool success, const QString &gameName, const QString &errorString = QString());
    void authenticationRequired();
    void cooldownStarted(int seconds);
    void cooldownFinished();

private slots:
    void onGameIdReceived();
    void onCategoryUpdateCompleted();
    void onChatMessageSent();
};

#endif // PLATFORMMANAGER_H
