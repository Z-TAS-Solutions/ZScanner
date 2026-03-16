#ifndef SSH_HANDLER_H
#define SSH_HANDLER_H

#pragma once
#include <libssh2.h>

#include <string>


class ZSSHHandler
{
public:
	ZSSHHandler();
	~ZSSHHandler();
	bool Connect(const std::string& IP, int Port, const std::string& Username, const std::string& PubKeyPath, const std::string& PrvKeyPath, const std::string& Passphrase);
	void Disconnect();
	bool CheckConnectionStatus() const { return Connected; }
	bool ExecuteCommand(const std::string& Command, std::string& Output);

private:
	LIBSSH2_SESSION* Session;
	LIBSSH2_CHANNEL* Channel;
	bool Connected;
};


#endif