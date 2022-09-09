#pragma once

#include <Handler.h>
#include <Messenger.h>


class ServerHandler: public BHandler {
public:
	enum {
		closureSendMsg = 1,
	};

	ServerHandler();
	virtual ~ServerHandler();

	void MessageReceived(BMessage *msg) final;
};

extern ServerHandler gServerHandler;
extern BMessenger gServerMessenger;
