#include "SSHHandler.h"

ZSSHHandler::ZSSHHandler() : Session(nullptr), Channel(nullptr), Connected(false) 
{
	
}

ZSSHHandler::~ZSSHHandler() 
{
	Disconnect();
}	


bool ZSSHHandler::Connect(const std::string& IP, int Port, const std::string& Username, const std::string& PubKeyPath, const std::string& PrvKeyPath, const std::string& Passphrase)
{
	if (libssh2_init(0) != 0) {
		return false;
	}


	Session = libssh2_session_init();
	if (!Session) {
		return false;
	}

	libssh2_session_set_blocking(Session, 1);


	const char* version = libssh2_version(0);
	if (version) {
		std::cout << "libssh2 version string: " << version << std::endl;
	}

	int wsResult;
	wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsResult != 0) {
		return false;
	}


	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(Port);
	sin.sin_addr.s_addr = inet_addr(IP.c_str());
	
	if (connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
		int ErrorCode = WSAGetLastError();
		Logger::log("Failed to connect socket ! : ", ErrorCode);
		libssh2_session_free(Session);
		Session = nullptr;
		return false;
	}


	int rc = libssh2_session_handshake(Session, sock);
	if (rc != 0) {
		char* errmsg;
		int errlen;
		libssh2_session_last_error(Session, &errmsg, &errlen, 0);
		Logger::log("Handshake failed with code " + std::to_string(rc) + ": " + errmsg);

		closesocket(sock);
		libssh2_session_free(Session);
		Session = nullptr;
		return false;
	}
	if (libssh2_userauth_publickey_fromfile(Session, Username.c_str(), PubKeyPath.c_str(), PrvKeyPath.c_str(), Passphrase.c_str()) != 0) {
		libssh2_session_disconnect(Session, "Authentication failed");

		char* errmsg;
		int errlen;
		libssh2_session_last_error(Session, &errmsg, &errlen, 0);
		Logger::log("Authentication Failed: " + std::string(errmsg, errlen));

		libssh2_session_free(Session);
		Session = nullptr;
		return false;
	}
	Connected = true;
	return true;
}


void ZSSHHandler::Disconnect() 
{
	if (Connected) {
		libssh2_session_disconnect(Session, "Normal Shutdown");
		libssh2_session_free(Session);
		Session = nullptr;
		Connected = false;
	}
}

bool ZSSHHandler::ExecuteCommand(const std::string& Command, std::string& Output) 
{
	if (!Connected) return false;
	Channel = libssh2_channel_open_session(Session);
	if (!Channel) return false;
	if (libssh2_channel_exec(Channel, Command.c_str()) != 0) {
		libssh2_channel_close(Channel);
		libssh2_channel_free(Channel);
		Channel = nullptr;
		return false;
	}
	char buffer[1024];
	ssize_t n;
	while ((n = libssh2_channel_read(Channel, buffer, sizeof(buffer))) > 0) {
		Output.append(buffer, n);
	}
	libssh2_channel_close(Channel);
	libssh2_channel_free(Channel);
	Channel = nullptr;
	return true;
}



