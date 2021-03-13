/*
	znet.h
	Copyright 2021, zhiayang

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	Version 0.2.0
	=============



	Documentation
	=============

	This socket library is a lightweight wrapper around POSIX/BSD network sockets, with both UDP and TCP
	support. SSL is also supported for TCP sockets, using OpenSSL >= 1.1. This library currently always
	requires threading support in the form of `std::thread` for the asynchronous API, though it will be
	configurable in the future.

	Both TCP and UDP sockets have a asynchronous callback interface as well as a synchronous, blocking
	interface. Currently, it is not possible to set either socket to non-blocking mode. Furthermore, arbitarily
	switching between the sync and async functions is not supported, since the callback-calling thread
	continues to try and read the socket simultaneously --- which might get into weird behaviour depending on
	which thread your kernel decides to respond to.

	Since the main socket objects are not templated, this library follows the stb_* style of header-only
	libraries --- in exactly one cpp file, #define ZNET_IMPLEMENTATION to generate the definitions of the
	various functions. When this macro is not defined, only declarations are made.

	optional #define macros to control behaviour:

	- ZNET_ENABLE_SSL
		this is *FALSE* by default. Define and set it to 1 to enable SSL support. This will pull in OpenSSL
		headers, so you need them installed, obviously.

	- ZNET_TCP_IMPLEMENTATION, ZNET_UDP_IMPLEMENTATION
		Similar to the top-level ZNET_IMPLEMENTATION, but generates definitions only for the specified kind
		of socket. Useful to reduce code size if you only need one kind of socket and not the other. You can
		also separate the definitions into two translation units, if you need to for some perverse reason.







	Version History
	===============

	0.2.0 - 10/03/2021
	------------------
	- add timeout support for TCP connect.
	- switch to seconds (as a double) for timeouts everywhere.
	- misc fixes
	- add more documentation


	0.1.0 - 10/03/2021
	------------------
	Initial release. Missing functionality includes but is not limited to:
	- TCP serverside support (listen, accept, bind)
	- SSL error handling
	- proper/tested IPv6 support
*/

#pragma once

#include <thread>
#include <functional>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#ifndef ZNET_ENABLE_SSL
	#define ZNET_ENABLE_SSL 0
#endif // ZNET_ENABLE_SSL

#if ZNET_ENABLE_SSL
	#include <openssl/ssl.h>
	#include <openssl/err.h>
#endif // ZNET_ENABLE_SSL

#if ZNET_IMPLEMENTATION
	#define ZNET_UDP_IMPLEMENTATION 1
	#define ZNET_TCP_IMPLEMENTATION 1
#endif // ZNET_IMPLEMENTATION

#if ZNET_UDP_IMPLEMENTATION || ZNET_TCP_IMPLEMENTATION
	#define ZNET_SOME_IMPLEMENTATION 1
#endif // ZNET_UDP_IMPLEMENTATION || ZNET_TCP_IMPLEMENTATION

#define ZNET_ERROR_ABORT(fmt, ...) do {     \
	fprintf(stderr, fmt, ##__VA_ARGS__);    \
	abort();                                \
} while(0)

#define ZNET_ERROR_RETURN(value, fmt, ...) do { \
	fprintf(stderr, fmt, ##__VA_ARGS__);        \
	return value;                               \
} while(0)

#define ZNET_ERROR_RETURN_VOID(fmt, ...) do {   \
	fprintf(stderr, fmt, ##__VA_ARGS__);        \
	return;                                     \
} while(0)


namespace znet
{
	namespace detail
	{
		void set_timeout(int sock, double timeout_secs);
		void set_blocking(int sock, bool block);
		bool get_blocking(int sock);
	}

	// sockaddr is toxic.
	struct IPAddress
	{
		IPAddress() : length(0), storage({}) { }

		explicit IPAddress(struct sockaddr_in inet4) : storage({ })
		{
			this->length = sizeof(struct sockaddr_in);
			memcpy(&this->storage, &inet4, this->length);
		}

		explicit IPAddress(struct sockaddr_in6 inet6) : storage({ })
		{
			this->length = sizeof(struct sockaddr_in6);
			memcpy(&this->storage, &inet6, this->length);
		}

		IPAddress(struct sockaddr* addr, size_t len) : storage({ })
		{
			this->length = len;
			memcpy(&this->storage, addr, this->length);
		}

		IPAddress(struct sockaddr_storage* addr, size_t len) : storage({ })
		{
			this->length = len;
			memcpy(&this->storage, addr, this->length);
		}

		inline size_t size() const                { return this->length; }
		inline bool empty() const                 { return this->length == 0; }
		inline std::string hostnameString() const { return this->hostname_string; }
		inline struct sockaddr* ptr()             { return reinterpret_cast<struct sockaddr*>(&this->storage); }
		inline const struct sockaddr* ptr() const { return reinterpret_cast<const struct sockaddr*>(&this->storage); }

		// note: 'ip' must be a 4-component IP address, eg. 192.168.1.69
		static IPAddress ip4(const std::string& ip, uint16_t port);

		// hostname is any hostname --- obviously without the URI type (eg. https)
		static IPAddress hostname4(const std::string& host, uint16_t port);

		// equivalent to INADDR_ANY
		static IPAddress any4(uint16_t port);

		// as the name suggests.
		static IPAddress udpBroadcast(uint16_t port);

	private:
		size_t length;
		struct sockaddr_storage storage;

		// only used by the hostname
		std::string hostname_string;
	};



	struct UDPSocket
	{
		~UDPSocket();
		UDPSocket(const IPAddress& local, const IPAddress& remote);

		UDPSocket(UDPSocket&&);
		UDPSocket& operator= (UDPSocket&&);

		UDPSocket(const UDPSocket&) = delete;
		UDPSocket& operator= (const UDPSocket&) = delete;

		bool bind();
		void close();
		void reset();
		bool connected() const;

		ssize_t send(const uint8_t* buf, size_t len);

		// asynchronous
		void onClose(std::function<void ()>&& callback);
		void onReceive(std::function<void (const uint8_t*, size_t, const IPAddress& from)>&& callback);

		// synchronous
		ssize_t receive(uint8_t* buf, size_t len, double timeout_secs = 0, IPAddress* from = nullptr);

		void setBlocking(bool blocking);
		bool isBlocking() const;

	private:
		void setup_receiver();
		ssize_t do_socket_read(uint8_t* buf, size_t len, double timeout_secs, IPAddress* from);

		int m_sock = -1;
		bool m_connected = false;
		uint8_t* m_buffer = nullptr;
		std::thread m_thread = { };
		std::function<void ()> m_closeCallback;
		std::function<void (const uint8_t*, size_t, const IPAddress& from)> m_callback;

		IPAddress m_recvaddr = { };
		IPAddress m_sendaddr = { };

		// 1500 MTU, plus a little extra
		static constexpr size_t BUFFER_SIZE = 8192;
	};

	struct TCPSocket
	{
		TCPSocket(const IPAddress& addr, bool ssl);
		~TCPSocket();

		TCPSocket(TCPSocket&&);
		TCPSocket& operator= (TCPSocket&&);

		TCPSocket(const TCPSocket&) = delete;
		TCPSocket& operator= (const TCPSocket&) = delete;

		bool connect(double timeout_secs = 0);
		void disconnect();
		bool connected() const;

		ssize_t send(const uint8_t* buf, size_t len);

		// asynchronous
		void onClose(std::function<void ()>&& callback);
		void onReceive(std::function<void (const uint8_t*, size_t)>&& callback);

		// synchronous
		ssize_t receive(uint8_t* buf, size_t len, double timeout_secs = 0);

		void setBlocking(bool blocking);
		bool isBlocking() const;

	private:
		void setup_receiver();
		ssize_t do_socket_read(uint8_t* buf, size_t len, double timeout_secs);

		int m_sock = -1;
		bool m_connected = false;
		uint8_t* m_buffer = nullptr;

		std::thread m_thread = { };
		std::function<void ()> m_closeCallback;
		std::function<void (const uint8_t*, size_t)> m_callback;

		IPAddress m_addr = { };


	#if ZNET_ENABLE_SSL

		SSL* m_ssl = nullptr;
		SSL_CTX* m_sslContext = nullptr;

		bool m_useSSL = false;

	#endif // ZNET_ENABLE_SSL


		// 1500 MTU, plus a little extra
		static constexpr size_t BUFFER_SIZE = 2048;
	};

#if ZNET_ENABLE_SSL

	// this initialises ssl when the program starts, by means of a global variable's constructor
	struct SSLInitialiser
	{
		SSLInitialiser();
	};

#endif // ZNET_ENABLE_SSL
}








#if ZNET_TCP_IMPLEMENTATION

#include <cstdio>
#include <cstdlib>

#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>


namespace znet
{
	TCPSocket::TCPSocket(const IPAddress& addr, bool ssl)
	{
		if(this->m_sock = socket(PF_INET, SOCK_STREAM, 0); this->m_sock < 0)
			ZNET_ERROR_ABORT("error creating tcp socket: %s\n", strerror(errno));

		this->m_addr = addr;
		this->m_buffer = new uint8_t[BUFFER_SIZE];
		this->m_callback = [](auto...) { };

		int yes = 1;
		setsockopt(this->m_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		if(ssl && !ZNET_ENABLE_SSL)
			ZNET_ERROR_ABORT("cannot use SSL without ZNET_ENABLE_SSL=1\n");

	#if ZNET_ENABLE_SSL

		this->m_useSSL = ssl;
		if(!ssl) return;

		this->m_sslContext = SSL_CTX_new(TLS_client_method());
		if(!this->m_sslContext) ZNET_ERROR_ABORT("failed to allocate SSL context: %s\n", strerror(errno));

		this->m_ssl = SSL_new(this->m_sslContext);
		if(!this->m_ssl) ZNET_ERROR_ABORT("failed to allocate SSL object: %s\n", strerror(errno));

		SSL_set_min_proto_version(this->m_ssl, TLS1_VERSION);
		SSL_set_tlsext_host_name(this->m_ssl, addr.hostnameString().c_str());

	    SSL_set_fd(this->m_ssl, this->m_sock);

	#endif // ZNET_ENABLE_SSL

	}

	TCPSocket::~TCPSocket()
	{
		if(this->m_connected)
			this->disconnect();

		if(this->m_buffer)
			delete[] this->m_buffer;
	}

	TCPSocket::TCPSocket(TCPSocket&& other)
	{
		this->m_sock            = other.m_sock;         other.m_sock = -1;
		this->m_connected       = other.m_connected;    other.m_connected = false;
		this->m_buffer          = other.m_buffer;       other.m_buffer = nullptr;
		this->m_thread          = std::move(other.m_thread);
		this->m_addr            = std::move(other.m_addr);
		this->m_closeCallback   = std::move(other.m_closeCallback);
		this->m_callback        = std::move(other.m_callback);

	#if ZNET_ENABLE_SSL
		this->m_useSSL          = other.m_useSSL;       other.m_useSSL = false;
		this->m_ssl             = other.m_ssl;          other.m_ssl = nullptr;
		this->m_sslContext      = other.m_sslContext;   other.m_sslContext = nullptr;
	#endif
	}

	TCPSocket& TCPSocket::operator= (TCPSocket&& other)
	{
		if(this != &other)
		{
			this->m_sock            = other.m_sock;         other.m_sock = -1;
			this->m_connected       = other.m_connected;    other.m_connected = false;
			this->m_buffer          = other.m_buffer;       other.m_buffer = nullptr;
			this->m_thread          = std::move(other.m_thread);
			this->m_addr            = std::move(other.m_addr);
			this->m_closeCallback   = std::move(other.m_closeCallback);
			this->m_callback        = std::move(other.m_callback);

		#if ZNET_ENABLE_SSL
			this->m_useSSL          = other.m_useSSL;       other.m_useSSL = false;
			this->m_ssl             = other.m_ssl;          other.m_ssl = nullptr;
			this->m_sslContext      = other.m_sslContext;   other.m_sslContext = nullptr;
		#endif
		}
		return *this;
	}

	void TCPSocket::setBlocking(bool blocking)
	{
		detail::set_blocking(this->m_sock, blocking);
	}

	bool TCPSocket::isBlocking() const
	{
		return detail::get_blocking(this->m_sock);
	}

	bool TCPSocket::connected() const
	{
		return this->m_connected;
	}

	bool TCPSocket::connect(double timeout_secs)
	{
		bool wasBlocking = false;
		if(timeout_secs > 0)
		{
			// make it nonblocking for now
			wasBlocking = this->isBlocking();
			this->setBlocking(false);

			// set the timeout...
			detail::set_timeout(this->m_sock, timeout_secs);
		}

		if(int x = ::connect(this->m_sock, this->m_addr.ptr(), this->m_addr.size()); x < 0 && errno != EINPROGRESS)
			ZNET_ERROR_RETURN(false, "socket connection error: %s\n", strerror(errno));

		if(timeout_secs > 0)
		{
			// now we wait for the idiot to finish. use poll here because i can't be bothered.
			// if you need microsecond precision on your connection timeout, go away.

			auto fds = pollfd { .fd = this->m_sock, .events = POLLOUT };
			auto ret = poll(&fds, 1, static_cast<int>(timeout_secs * 1000));
			if(ret < 0)
			{
				if(errno == EINPROGRESS)
					return false;

				else // it's another kind of error, better print something.
					ZNET_ERROR_RETURN(false, "connection failed: %s\n", strerror(errno));
			}

			if(ret == 0) return false;

			// ok, now reset the timeout.
			this->setBlocking(wasBlocking);
		}


	#if ZNET_ENABLE_SSL
		if(this->m_useSSL)
		{
			if(SSL_connect(this->m_ssl) < 0)
				ZNET_ERROR_RETURN(false, "SSL connection error: %s\n", strerror(errno));
		}
	#endif // ZNET_ENABLE_SSL

		this->m_connected = true;
		this->setup_receiver();

		return true;
	}

	void TCPSocket::disconnect()
	{
		if(this->m_sock == -1)
			ZNET_ERROR_RETURN_VOID("warning: attempted to close socket that was already closed\n");

		if(this->m_closeCallback)
			this->m_closeCallback();

		// tell the listener thread to die
		__atomic_store_n(&this->m_connected, false, __ATOMIC_SEQ_CST);
		if(this->m_thread.joinable())
			this->m_thread.join();

	#if ZNET_ENABLE_SSL
		if(this->m_useSSL)
		{
			if(this->m_ssl)
			{
				SSL_set_shutdown(this->m_ssl, SSL_RECEIVED_SHUTDOWN | SSL_SENT_SHUTDOWN);
				SSL_shutdown(this->m_ssl);
				SSL_free(this->m_ssl);
			}

			if(this->m_sslContext)
			{
				SSL_CTX_free(this->m_sslContext);
			}
		}
	#endif // ZNET_ENABLE_SSL

		::close(this->m_sock);
	}

	ssize_t TCPSocket::send(const uint8_t* buf, size_t len)
	{
	#if ZNET_ENABLE_SSL
		if(this->m_useSSL)
		{
			// TODO: handle SSL errors
			size_t bytes = 0;
			SSL_write_ex(this->m_ssl, buf, len, &bytes);
			return bytes;
		}
		else
	#endif // ZNET_ENABLE_SSL
		{
			auto bytes = ::send(this->m_sock, buf, len, 0);
			if(bytes < 0) ZNET_ERROR_RETURN(-1, "socket error: %s\n", strerror(errno));

			return bytes;
		}
	}

	void TCPSocket::onClose(std::function<void ()>&& callback)
	{
		this->m_closeCallback = std::move(callback);
	}

	void TCPSocket::onReceive(std::function<void (const uint8_t*, size_t)>&& callback)
	{
		this->m_callback = std::move(callback);
	}

	void TCPSocket::setup_receiver()
	{
		using namespace std::chrono_literals;

		this->m_thread = std::thread([this]() {

			while(true)
			{
				if(!this->m_connected)
					break;

				auto bytes = this->do_socket_read(this->m_buffer, BUFFER_SIZE, (0.2s).count());
				if(bytes <= 0) continue;

				this->m_callback(this->m_buffer, bytes);
			}
		});
	}

	ssize_t TCPSocket::receive(uint8_t* buf, size_t len, double timeout_secs)
	{
		return this->do_socket_read(buf, len, timeout_secs);
	}

	ssize_t TCPSocket::do_socket_read(uint8_t* buf, size_t len, double timeout_secs)
	{
		detail::set_timeout(this->m_sock, timeout_secs);

	#if ZNET_ENABLE_SSL
		if(this->m_useSSL)
		{
			// TODO: handle SSL errors
			size_t bytes = 0;
			SSL_read_ex(this->m_ssl, buf, len, &bytes);
			return bytes;
		}
		else
	#endif // ZNET_ENABLE_SSL
		{
			auto bytes = recv(this->m_sock, buf, len, 0);
			if(bytes < 0)
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
					return 0;

				if(errno == ECONNRESET)
					this->m_connected = false;

				ZNET_ERROR_RETURN(-1, "socket error: %s\n", strerror(errno));
			}

			return bytes;
		}
	}


#if ZNET_ENABLE_SSL
	SSLInitialiser::SSLInitialiser()
	{
		OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
			nullptr);

		OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS,
			nullptr
		);
	}

	static SSLInitialiser sslInitialiser;
#endif // ZNET_ENABLE_SSL
}

#endif // ZNET_TCP_IMPLEMENTATION




#if ZNET_UDP_IMPLEMENTATION

#include <cstdio>
#include <cstdlib>

#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

namespace znet
{
	UDPSocket::UDPSocket(const IPAddress& local, const IPAddress& remote)
	{
		if(this->m_sock = socket(PF_INET, SOCK_DGRAM, 0); this->m_sock < 0)
			ZNET_ERROR_ABORT("error creating udp socket: %s\n", strerror(errno));

		this->m_recvaddr = local;
		this->m_sendaddr = remote;

		if(this->m_sendaddr.empty())
			this->m_sendaddr = local;

		// enable broadcast, just in case. it doesn't matter anyway.
		int yes = 1;
		setsockopt(this->m_sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

		int size = 1024 * 1024 * 64;
		setsockopt(this->m_sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

		this->m_buffer = new uint8_t[BUFFER_SIZE];
		this->m_callback = [](auto...) { };

		this->setup_receiver();
	}

	UDPSocket::~UDPSocket()
	{
		if(this->m_connected)
			this->close();

		if(this->m_buffer)
			delete[] this->m_buffer;
	}

	UDPSocket::UDPSocket(UDPSocket&& other)
	{
		if(other.connected())
			ZNET_ERROR_ABORT("cannot move socket while it is connected!");

		this->m_sock            = other.m_sock;         other.m_sock = -1;
		this->m_connected       = other.m_connected;    other.m_connected = false;
		this->m_buffer          = other.m_buffer;       other.m_buffer = nullptr;
		this->m_thread          = std::move(other.m_thread);
		this->m_recvaddr        = std::move(other.m_recvaddr);
		this->m_sendaddr        = std::move(other.m_sendaddr);
		this->m_closeCallback   = std::move(other.m_closeCallback);
		this->m_callback        = std::move(other.m_callback);
	}

	UDPSocket& UDPSocket::operator= (UDPSocket&& other)
	{
		if(this != &other)
		{
			if(other.connected())
				ZNET_ERROR_ABORT("cannot move socket while it is connected!");

			this->m_sock            = other.m_sock;         other.m_sock = -1;
			this->m_connected       = other.m_connected;    other.m_connected = false;
			this->m_buffer          = other.m_buffer;       other.m_buffer = nullptr;
			this->m_thread          = std::move(other.m_thread);
			this->m_recvaddr        = std::move(other.m_recvaddr);
			this->m_sendaddr        = std::move(other.m_sendaddr);
			this->m_closeCallback   = std::move(other.m_closeCallback);
			this->m_callback        = std::move(other.m_callback);
		}
		return *this;
	}

	void UDPSocket::setBlocking(bool blocking)
	{
		detail::set_blocking(this->m_sock, blocking);
	}

	bool UDPSocket::isBlocking() const
	{
		return detail::get_blocking(this->m_sock);
	}

	bool UDPSocket::bind()
	{
		if(::bind(this->m_sock, this->m_recvaddr.ptr(), this->m_recvaddr.size()) < 0)
			ZNET_ERROR_RETURN(false, "failed to bind udp socket: %s\n", strerror(errno));

		__atomic_store_n(&this->m_connected, true, __ATOMIC_SEQ_CST);
		return true;
	}

	void UDPSocket::close()
	{
		if(this->m_sock == -1)
			ZNET_ERROR_RETURN_VOID("warning: attempted to close socket that was already closed\n");

		if(this->m_closeCallback)
			this->m_closeCallback();

		// tell the listener thread to die
		__atomic_store_n(&this->m_connected, false, __ATOMIC_SEQ_CST);
		if(this->m_thread.joinable())
			this->m_thread.join();

		::close(this->m_sock);
		this->m_sock = -1;
	}

	bool UDPSocket::connected() const
	{
		return this->m_connected;
	}

	void UDPSocket::reset()
	{
		this->m_callback = { };
		this->m_closeCallback = { };
	}

	void UDPSocket::onClose(std::function<void ()>&& callback)
	{
		this->m_closeCallback = std::move(callback);
	}

	void UDPSocket::onReceive(std::function<void (const uint8_t*, size_t, const IPAddress& from)>&& callback)
	{
		this->m_callback = std::move(callback);
	}

	void UDPSocket::setup_receiver()
	{
		using namespace std::chrono_literals;

		this->m_thread = std::thread([this]() {

			while(true)
			{
				if(!this->m_connected)
					break;

				IPAddress addr = { };
				auto bytes = this->do_socket_read(this->m_buffer, BUFFER_SIZE, (0.2s).count(), &addr);
				if(bytes <= 0) continue;

				this->m_callback(this->m_buffer, bytes, addr);
			}
		});
	}

	ssize_t UDPSocket::send(const uint8_t* buf, size_t len)
	{
		return sendto(this->m_sock, buf, len, 0, this->m_sendaddr.ptr(), this->m_sendaddr.size());
	}

	ssize_t UDPSocket::receive(uint8_t* buf, size_t len, double timeout_secs, IPAddress* from)
	{
		return this->do_socket_read(buf, len, timeout_secs, from);
	}

	ssize_t UDPSocket::do_socket_read(uint8_t* buf, size_t len, double timeout_secs, IPAddress* from)
	{
		detail::set_timeout(this->m_sock, timeout_secs);

		socklen_t sa_len = 0;
		struct sockaddr_storage sa = { };

		auto bytes = recvfrom(this->m_sock, buf, len,
			0, reinterpret_cast<struct sockaddr*>(&sa), &sa_len);

		if(bytes < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK))
			ZNET_ERROR_RETURN(-1, "socket error: %s\n", strerror(errno));

		if(from)
			*from = IPAddress(&sa, sa_len);

		return bytes;
	}
}
#endif // ZNET_UDP_IMPLEMENTATION





// everybody needs this.
#if ZNET_SOME_IMPLEMENTATION


#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/select.h>

namespace znet
{
	IPAddress IPAddress::ip4(const std::string& ip, uint16_t port)
	{
		struct sockaddr_in sa;
		sa.sin_family = AF_INET;
		sa.sin_port = htons(port);

		if(inet_pton(AF_INET, ip.c_str(), &sa.sin_addr.s_addr) < 0)
			ZNET_ERROR_ABORT("invalid ipv4 address '%s'\n", ip.c_str());

		auto ret = IPAddress(sa);
		ret.hostname_string = ip;
		return ret;
	}

	IPAddress IPAddress::hostname4(const std::string& host, uint16_t port)
	{
		struct addrinfo hints = {
			.ai_family = AF_INET
		};

		struct addrinfo* info = nullptr;
		int res = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info);
		if(res < 0) ZNET_ERROR_ABORT("could not getaddrinfo(%s): %s\n", host.c_str(), strerror(errno));

		auto ret = IPAddress(info->ai_addr, info->ai_addrlen);
		ret.hostname_string = host;

		freeaddrinfo(info);
		return ret;
	}

	IPAddress IPAddress::any4(uint16_t port)
	{
		struct addrinfo hints = {
			.ai_family = AF_INET,
			.ai_flags = AI_PASSIVE
		};

		struct addrinfo* info = nullptr;
		int res = getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &info);
		if(res < 0) ZNET_ERROR_ABORT("could not getaddrinfo(): %s\n", strerror(errno));

		auto ret = IPAddress(info->ai_addr, info->ai_addrlen);
		freeaddrinfo(info);

		return ret;
	}

	IPAddress IPAddress::udpBroadcast(uint16_t port)
	{
		return IPAddress::ip4("255.255.255.255", port);
	}

	namespace detail
	{
		void set_timeout(int sock, double timeout_secs)
		{
			auto seconds = floor(timeout_secs);
			auto micros = (timeout_secs - seconds) * 1e6;

			struct timeval tv = {
				.tv_sec = static_cast<time_t>(seconds),
				.tv_usec = static_cast<suseconds_t>(micros)
			};

			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		}

		void set_blocking(int sock, bool block)
		{
			const auto flags = fcntl(sock, F_GETFL, 0);
			const auto newflags = !block ? (flags | O_NONBLOCK) : (flags ^ O_NONBLOCK);
			if(fcntl(sock, F_SETFL, newflags) < 0)
				ZNET_ERROR_RETURN_VOID("could not set socket to non-blocking: %s\n", strerror(errno));
		}

		bool get_blocking(int sock)
		{
			return !(fcntl(sock, F_GETFL, 0) & O_NONBLOCK);
		}
	}
}
#endif // ZNET_SOME_IMPLEMENTATION



#undef ZNET_ERROR_ABORT
#undef ZNET_ERROR_RETURN
#undef ZNET_ERROR_RETURN_VOID

