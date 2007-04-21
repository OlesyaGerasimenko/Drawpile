/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   
   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "sockets.h"

#include <iostream>
#include <string>
#include <cassert>

#ifndef WIN32
	#include <fcntl.h>
	#include <sstream>
#endif

using std::cout;
using std::endl;
using std::cerr;

Net::Net() throw(std::exception)
{
	#ifndef NDEBUG
	cout << "Net()" << endl;
	#endif
	
	#if defined(WIN32)
	WSADATA info;
	if (WSAStartup(MAKEWORD(2,0), &info))
		throw std::exception();
	
	if (LOBYTE(info.wVersion) != 2
		or HIBYTE(info.wVersion) != 0)
	{
		cerr << "Invalid WSA version: "
			<< LOBYTE(info.wVersion) << "." << HIBYTE(info.wVersion) << endl;
		WSACleanup( );
		exit(1); 
	}
	#endif
}

Net::~Net() throw()
{
	#ifndef NDEBUG
	cout << "~Net()" << endl;
	#endif
	
	#if defined(WIN32)
	WSACleanup();
	#endif
}

/* *** Socket class *** */

fd_t Socket::create() throw()
{
	#ifdef WIN32
	
	#ifdef IPV6_SUPPORT
	sock = WSASocket(AF_INET6, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
	#else // IPv4
	sock = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
	#endif
	
	#else // POSIX
	
	#ifdef IPV6_SUPPORT
	sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	#else // IPv4
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	#endif
	
	#endif
	
	if (sock == INVALID_SOCKET)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EPROTONOSUPPORT);
		assert(s_error != EPROTOTYPE);
		assert(s_error != ESOCKTNOSUPPORT);
		assert(s_error != EINVAL);
		
		switch (s_error)
		{
		#ifdef WIN32
		case EINPROGRESS:
			break;
		case ENETDOWN:
			cerr << "socket: network sub-system failure" << endl;
			break;
		#else // POSIX
		// TODO
		#endif
		case EMFILE:
			cerr << "socket: socket limit reached" << endl;
			break;
		case ENOBUFS:
			cerr << "socket: out of buffers" << endl;
			break;
		default:
			cerr << "Socket::create() - unknown error: " << s_error << endl;
			assert(s_error);
			break;
		}
	}
	
	return sock;
}

void Socket::close() throw()
{
	#if defined(HAVE_XPWSA)
	::DisconnectEx(sock, 0, TF_REUSE_SOCKET, 0);
	#elif defined(WIN32)
	::closesocket(sock);
	#else // POSIX
	::close(sock);
	#endif
	
	sock = INVALID_SOCKET;
}

Socket Socket::accept() throw()
{
	assert(sock != INVALID_SOCKET);
	
	// temporary address struct
	#ifdef IPV6_SUPPORT
	sockaddr_in6 sa;
	#else
	sockaddr_in sa;
	#endif
	
	#ifdef WIN32
	int addrlen
	#else
	socklen_t addrlen
	#endif
		= sizeof(sa);
	
	#ifdef WIN32
	fd_t n_fd = ::WSAAccept(sock, reinterpret_cast<sockaddr*>(&sa), &addrlen, 0, 0);
	#else // POSIX
	fd_t n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &addrlen);
	#endif
	
	if (n_fd != INVALID_SOCKET)
	{
		return Socket(n_fd, sa);
	}
	else
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		
		#ifdef EPROTO
		assert(s_error != EPROTO);
		#endif
		
		#ifdef LINUX
		// no idea what these are
		assert(s_error != ENOSR); // ?
		assert(s_error != ESOCKTNOSUPPORT); // ?
		assert(s_error != EPROTONOSUPPORT); // Protocol not supported
		assert(s_error != ETIMEDOUT); // Timed out
		assert(s_error != ERESTARTSYS); // ?
		#endif
		
		switch (s_error)
		{
		case EINTR: // interrupted
		case EAGAIN: // would block
			break;
		case EMFILE:
			cerr << "socket: process FD limit reached" << endl;
			break;
		case ENOBUFS:
			cerr << "socket: out of network buffers" << endl;
			break;
		case ECONNABORTED:
			cerr << "socket: incoming connection aborted" << endl;
			break;
		case ENOMEM:
			cerr << "socket: out of memory" << endl;
			break;
		case EPERM:
			cerr << "socket: firewall blocked incoming connection" << endl;
			break;
		#ifndef WIN32 // POSIX
		case ENFILE:
			cerr << "socket: system FD limit reached" << endl;
			break;
		#endif
		default:
			cerr << "Socket.accept() - unknown error: " << s_error << endl;
			exit(1);
		}
		
		return Socket();
	}
}

bool Socket::block(const bool x) throw()
{
	#ifndef NDEBUG
	cout << "Socket.block(fd: " << sock << ", " << (x?"true":"false") << ")" << endl;
	#endif // NDEBUG
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WIN32
	uint32_t arg = (x ? 1 : 0);
	return (WSAIoctl(sock, FIONBIO, &arg, sizeof(arg), 0, 0, 0, 0, 0) == 0);
	#else // POSIX
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? false : true;
	#endif
}

bool Socket::reuse(const bool x) throw()
{
	#ifndef NDEBUG
	cout << "Socket.reuse(fd: " << sock << ", " << (x?"true":"false") << ")" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifndef SO_REUSEPORT
	// Windows (for example) does not have it
	return (x==true);
	#else // POSIX
	int val = (x ? 1 : 0);
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
	
	if (r == SOCKET_ERROR)
	{
		s_error = errno;
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		cerr << "Socket.reuse() - unknown error: " << s_error << endl;
		exit(1);
	}
	
	return (r == 0);
	#endif
}

bool Socket::linger(const bool x, const uint16_t delay) throw()
{
	::linger lval;
	lval.l_onoff = (x ? 1 : 0);
	lval.l_linger = delay;
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lval), sizeof(lval));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		cerr << "Socket.linger() - unknown error: " << s_error << endl;
		exit(1);
	}
	else
		return (r == 0);
}

int Socket::bindTo(const std::string& address, const uint16_t _port) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "Socket.bindTo([" << address << "], " << port << ")" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	addr = Socket::StringToAddr(address);
	
	#ifdef IPV6_SUPPORT
	addr.sin6_family = AF_INET6;
	bswap(addr.sin6_port = _port);
	#else // IPv4
	addr.sin_family = AF_INET;
	bswap(addr.sin_port = _port);
	#endif // IPV6_SUPPORT
	
	const int r = bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EISCONN);
		
		switch (s_error)
		{
		case EADDRINUSE:
			cerr << "socket: address already in use" << endl;
			break;
		case EADDRNOTAVAIL:
			cerr << "socket: address not available" << endl;
			break;
		case ENOBUFS:
			cerr << "socket: out of network buffers" << endl;
			break;
		case EACCES:
			cerr << "socket: can't bind to super-user sockets" << endl;
			break;
		}
	}
	
	return r;
}

#ifdef IPV6_SUPPORT
int Socket::connect(const sockaddr_in6& rhost) throw()
{
	// TODO
	assert(1);
	return 0;
}
#endif

int Socket::connect(const sockaddr_in& rhost) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "Socket.connect()" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	r_addr = rhost;
	
	#ifdef WIN32
	const int r = WSAConnect(sock, reinterpret_cast<sockaddr*>(&r_addr), sizeof(r_addr), 0, 0, 0, 0);
	#else // POSIX
	const int r = ::connect(sock, reinterpret_cast<sockaddr*>(&r_addr), sizeof(r_addr));
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != ENOTSOCK);
		assert(s_error != EISCONN);
		assert(s_error != EADDRINUSE);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EALREADY);
		
		switch (s_error)
		{
		case EINPROGRESS:
			break;
		case EACCES:
		#ifdef EPERM
		case EPERM:
			cerr << "socket: firewall denied connection" << endl;
			break;
		#endif
		case ECONNREFUSED:
		case ETIMEDOUT:
		case ENETUNREACH:
			break;
		case EAGAIN:
			// retry
			return 2;
		}
	}
	
	return r;
}

int Socket::listen() throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "Socket.listen()" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	const int r = ::listen(sock, 4);
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		
		#ifndef NDEBUG
		cerr << "Socket.listen() - unknown error: " << s_error << endl;
		#endif // NDEBUG
		exit(1);
	}
	else
		return r;
}

int Socket::send(char* buffer, const size_t len) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "Socket.send(*buffer, " << len << ")" << endl;
	#endif
	
	assert(buffer != 0);
	assert(len > 0);
	assert(sock != INVALID_SOCKET);
	
	#ifdef WIN32
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	DWORD sb;
	const int r = ::WSASend(sock, &wbuf, 1, &sb, 0, 0, 0);
	#else // POSIX
	const int r = ::send(sock, buffer, len, MSG_NOSIGNAL);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EFAULT);
		assert(s_error != EINVAL);
		assert(s_error != EBADF);
		assert(s_error != ENOTCONN);
		assert(s_error != ENOTSOCK);
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#else
		assert(s_error != EOPNOTSUPP);
		#endif
		
		switch (s_error)
		{
		case EAGAIN:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			break;
		case ENOMEM:
		case ENOBUFS:
			break;
		#ifdef WIN32
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_IO_PENDING: // Operation will be completed later
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
			break;
		#endif
		default:
			#ifndef NDEBUG
			cerr << "Socket.send() - unknown error: " << s_error << endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WIN32
		return r;
		#endif
	}
	
	#ifdef WIN32
	return sb;
	#else
	return r;
	#endif
}

int Socket::recv(char* buffer, const size_t len) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "Socket.recv(*buffer, " << len << ")" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	assert(buffer != 0);
	assert(len > 0);
	
	#ifdef WIN32
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	DWORD flags=0;
	DWORD rb;
	const int r = ::WSARecv(sock, &wbuf, 1, &rb, &flags, 0, 0);
	#else // POSIX
	const int r = ::recv(sock, buffer, len, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != EINVAL);
		assert(s_error != ENOTCONN);
		assert(s_error != ENOTSOCK);
		
		switch (s_error)
		{
		case EAGAIN:
		case EINTR:
			break;
		case ENOBUFS: // Out of buffers
		case ECONNRESET:
		case ECONNREFUSED:
			break;
		case ENOMEM:
			break;
		#ifdef WIN32
		case WSAEDISCON:
		case WSAESHUTDOWN:
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
		case WSA_IO_PENDING: // Operation will be completed later
			break;
		#endif
		default:
			#ifndef NDEBUG
			cerr << "Socket.recv() - unknown error: " << s_error << endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WIN32
		return r;
		#endif
	}
	
	#ifdef WIN32
	return rb;
	#else
	return r;
	#endif
}

#if defined(WITH_SENDFILE) or defined(HAVE_XPWSA)
int Socket::sendfile(fd_t fd, off_t offset, size_t nbytes, off_t *sbytes) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "Socket.sendfile()" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	assert(offset >= 0);
	
	// call the real sendfile()
	#ifdef HAVE_XPWSA
	const int r = TransmitFile(sock, fd, nbytes, 0, 0, 0, TF_WRITE_BEHIND);
	#else // non-windows
	const int r = ::sendfile(fd, sock, offset, nbytes, 0, sbytes, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != ENOTSOCK);
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTCONN);
		
		switch (s_error)
		{
		case EAGAIN:
			// retry
			break;
		#ifndef WIN32 // POSIX
		case EPIPE:
		case EIO: // should be handled by the caller
			break;
		#endif
		default:
			cerr << "Socket.sendfile() - unknown error: " << s_error << endl;
			exit(1);
		}
	}
	
	return 0;
}
#endif // WITH_SENDFILE

std::string Socket::address() const throw()
{
	return AddrToString(addr);
}

uint16_t Socket::port() const throw()
{
	#ifdef IPV6_SUPPORT
	uint16_t _port =  addr.sin6_port;
	#else // IPv4
	uint16_t _port =  addr.sin_port;
	#endif
	
	return bswap(_port);
}

bool Socket::matchAddress(const Socket& tsock) const throw()
{
	#ifdef IPV6_SUPPORT
	// TODO: Similar checking for IPv6 addresses
	assert(IPV6_SUPPORT_INCOMPLETE);
	return false;
	#else // IPv4
	return (addr.sin_addr.s_addr == tsock.addr.sin_addr.s_addr);
	#endif
}

bool Socket::matchPort(const Socket& tsock) const throw()
{
	return (port() == tsock.port());
}

/* string functions */

#ifdef IPV6_SUPPORT
//static
std::string Socket::AddrToString(const sockaddr_in6& raddr) throw()
{
	char straddr[INET_ADDRSTRLEN+1];
	straddr[INET_ADDRSTRLEN] = '\0';
	
	// convert address to string
	
	#ifdef WIN32
	DWORD len = INET6_ADDRSTRLEN;
	
	sockaddr sa;
	memcpy(&sa, &raddr, sizeof(raddr));
	WSAAddressToString(&sa, sizeof(raddr), 0, straddr, &len);
	
	return std::string(straddr);
	
	#else // POSIX
	
	inet_ntop(AF_INET6, &raddr.sin6_addr, straddr, sizeof(straddr));
	
	uint16_t _port = raddr.sin6_port;
	bswap(_port);
	
	std::ostringstream stream;
	stream << "[" << straddr << "]:" << _port; // ?
	return stream.str();
	#endif
}
#endif

//static
std::string Socket::AddrToString(const sockaddr_in& raddr) throw()
{
	char straddr[INET_ADDRSTRLEN+1];
	straddr[INET_ADDRSTRLEN] = '\0';
	
	#ifdef WIN32
	DWORD len = INET_ADDRSTRLEN;
	
	sockaddr sa;
	memcpy(&sa, &raddr, sizeof(raddr));
	WSAAddressToString(&sa, sizeof(raddr), 0, straddr, &len);
	
	return std::string(straddr);
	
	#else // POSIX
	
	inet_ntop(AF_INET, &raddr.sin_addr, straddr, sizeof(straddr));
	
	uint16_t _port = raddr.sin_port;
	bswap(_port);
	
	std::ostringstream stream;
	stream << straddr << ":" << _port;
	return stream.str();
	#endif
}


#ifdef IPV6_SUPPORT
//static
sockaddr_in6 Socket::StringToAddr(const std::string& address) throw()
{
	sockaddr_in6 naddr;
	naddr.sin6_family = AF_INET6;
	
	#ifdef WIN32
	char straddr[128];
	memcpy(straddr, address.c_str(), address.length());
	
	int size = sizeof(naddr);
	WSAStringToAddress(straddr, AF_INET6, 0, reinterpret_cast<sockaddr*>(&naddr), &size);
	#else // POSIX
	inet_pton(AF_INET6, address.c_str(), &(naddr.sin6_addr));
	#endif
	
	return naddr;
}
#endif

sockaddr_in Socket::StringToAddr(const std::string& address) throw()
{
	sockaddr_in naddr;
	naddr.sin_family = AF_INET;
	
	#ifdef WIN32
	char straddr[128];
	memcpy(straddr, address.c_str(), address.length());
	
	int size = sizeof(naddr);
	WSAStringToAddress(straddr, AF_INET, 0, reinterpret_cast<sockaddr*>(&naddr), &size);
	#else // POSIX
	inet_pton(AF_INET, address.c_str(), &(naddr.sin_addr));
	#endif
	
	return naddr;
}
