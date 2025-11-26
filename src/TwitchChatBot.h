#ifndef TWITCHCHATBOT_H
#define TWITCHCHATBOT_H

#pragma once
#include <QObject>
#include <QFutureWatcher>

template <typename T>
class QFutureWatcher;

class TwitchChatBot : public QObject {
	Q_OBJECT

public:
	static TwitchChatBot &get()
	{
		static TwitchChatBot instance;
		return instance;
	}

	bool sendChatMessage(const QString &message);
	bool updateCategory(const QString &gameName);
	bool isOnCooldown() const;
	int getCooldownRemaining() const;
	void setLastSetCategory(const QString &categoryName);
	QString getLastSetCategory() const;

private:
	TwitchChatBot();
	~TwitchChatBot();

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

#endif // TWITCHCHATBOT_H