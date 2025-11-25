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

	void sendChatMessage(const QString &message);
	bool updateCategory(const QString &gameName);

private:
	TwitchChatBot();
	~TwitchChatBot();

	QString lastSetCategoryName;
	QFutureWatcher<QString> *gameIdWatcher;
	QFutureWatcher<bool> *chatMessageWatcher;
	QFutureWatcher<void *> *categoryUpdateWatcher; // Usaremos void* para o resultado do update

signals:
	void categoryUpdateFinished(bool success, const QString &gameName, const QString &errorString = QString());
	void authenticationRequired();

private slots:
	void onGameIdReceived();
	void onCategoryUpdateCompleted();
	void onChatMessageSent();
};

#endif // TWITCHCHATBOT_H