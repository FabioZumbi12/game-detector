#ifndef TWITCHCHATBOT_H
#define TWITCHCHATBOT_H

#pragma once
#include <QFutureWatcher>
#include <QObject>

class TwitchChatBot : public QObject {
	Q_OBJECT

public:
	static TwitchChatBot &get()
	{
		static TwitchChatBot instance;
		return instance;
	}

	void sendChatMessage(const QString &message);
	bool updateCategory(const QString &gameName);

private:
	TwitchChatBot();
	~TwitchChatBot();

	QFutureWatcher<QString> *categoryUpdateWatcher;
	QFutureWatcher<bool> *chatMessageWatcher;

signals:
	void categoryUpdateFinished(bool success, const QString &gameName, const QString &errorString = QString());
	void authenticationRequired();
};

#endif // TWITCHCHATBOT_H