#pragma once
#include "IPlatformService.h"
#include "TwitchAuthManager.h"
#include <QFutureWatcher>

class TwitchServiceAdapter : public IPlatformService {
    Q_OBJECT
public:
    explicit TwitchServiceAdapter(QObject *parent = nullptr);
    void updateCategory(const QString &gameName, const QString &title = QString()) override;
    void sendChatMessage(const QString &message) override;
    bool isAuthenticated() const override;

private:
    QFutureWatcher<QString> *gameIdWatcher;
    QFutureWatcher<TwitchAuthManager::UpdateResult> *updateWatcher;
    QFutureWatcher<bool> *messageWatcher;
};
