﻿#ifndef __TSTREAM_H__
#define __TSTREAM_H__

#include <streambuf>
#include <iostream>

#if (defined _WIN32) || (defined _WIN64)
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#define __INIT_SOCK_LIB ((tstream *)nullptr)->initsocklib()
#define __CLOSE_SOCKET(S) ::closesocket(S), S = INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
typedef int SOCKET;
//#pragma region define win32 const variable in linux
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define __INIT_SOCK_LIB
#define __CLOSE_SOCKET(S) ::close(S), S = INVALID_SOCKET
//#pragma endregion
#endif

using namespace std;

struct tstream : public iostream {

#if (defined _WIN32) || (defined _WIN64)
    void initsocklib() {
        static bool inited = false;
        WSADATA wsaData;
        if (!inited) WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
#endif

    class tcpbuf : public streambuf {
    public:
        tcpbuf() {
            __INIT_SOCK_LIB;
            _sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        tcpbuf(SOCKET s) { _sock = s; }
        tcpbuf(tcpbuf&& other) {
            _sock = other._sock;
            other._sock = INVALID_SOCKET;
        }
        ~tcpbuf() override { close(); }

        bool connect(char *ip, unsigned short port) {
            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_addr.s_addr = inet_addr(ip);
            addr.sin_family = AF_INET;
            addr.sin_port = ::htons(port);
            int ret = ::connect(_sock, (sockaddr *)&addr, sizeof(addr));
            return ret != SOCKET_ERROR;
        }
        int close() { return __CLOSE_SOCKET(_sock); }

        int recv(char *buf, size_t len) { return ::recv(_sock, buf, len, 0); }
        int send(const char *buf, size_t len) { return ::send(_sock, buf, len, 0); }

    protected:
        // Unbuffered get
        int underflow() override {
            return 0;
        }
        int uflow() override {
            uint8_t c;
            return recv((char *)&c, sizeof(c)) < 0 ? EOF : c;
        }
        streamsize xsgetn(char *s, streamsize size) override {
            auto need = size;
            do {
                auto n = recv(s, need);
                if (n < 0)
                    return size - need;
                need -= n;
            } while (need);
            return size;
        }
        // Unbuffered put
        int overflow(int c) override {
            if (c == EOF)
                return close(), 0;
            char b = c;
            return send(&b, 1) > 0 ? c : EOF;
        }
        streamsize xsputn(const char *s, streamsize size) override {
            auto need = size;
            do {
                auto n = send(s, need);
                if (n < 0)
                    return size - need;
                need -= n;
            } while (need);
            return size;
        }
        // flush
        int sync() override {
#if (defined _WIN32) || (defined _WIN64)
            return 0;
#else
            return flush(_sock);
#endif
        }

    private:
        SOCKET _sock;

    };

    struct server {
        server() {
            __INIT_SOCK_LIB;
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        // Bind and listen
        server(const char *addr, unsigned short port)
            : server() {
            bind(addr, port); listen();
        }
        // Bind and listen
        server(unsigned short port)
            : server() {
            bind(port); listen();
        }
        ~server() { close(); }

        bool bind(const char *addr, unsigned short port) {
            return bind(inet_addr(addr), port);
        }
        bool bind(int port) { return bind(ADDR_ANY, port); }
        bool bind(unsigned long addr, unsigned short port) {
            sockaddr_in svraddr;
            svraddr.sin_family = AF_INET;
            svraddr.sin_addr.s_addr = addr;
            svraddr.sin_port = htons(port);
            int ret = ::bind(sock, (struct sockaddr*)&svraddr, sizeof(svraddr));
            return ret != SOCKET_ERROR;
        }

        bool listen(int backlog = 5) {
            int ret = ::listen(sock, backlog);
            return ret != SOCKET_ERROR;
        }

        tstream accept() {
            sockaddr_in cliaddr;
            int addrlen = sizeof(cliaddr);
            return ::accept(sock,
                (struct sockaddr*)&cliaddr, &addrlen);
        }
        int close() { return __CLOSE_SOCKET(sock); }

        SOCKET sock = INVALID_SOCKET;
    };

    tstream() : iostream(&_buf) {}
    tstream(tstream&& s)
        : _buf(std::move(s._buf)), iostream(&_buf) {}
    tstream(SOCKET sock)
        : _buf(sock), iostream(&_buf) {
        if (sock == INVALID_SOCKET)
            setstate(ios_base::failbit);
    }
    tstream(char *ip, unsigned short port)
        : tstream() { connect(ip, port); }

    bool connect(char *ip, unsigned short port) {
        if (_buf.connect(ip, port))
            return clear(ios_base::goodbit), true;
        return clear(ios_base::failbit), true;
    }

    int close() { return _buf.close(); }
    // Raw 'send' function
    int send(const char *buf, size_t len) { return _buf.send(buf, len); }
    // Raw 'recv' function
    int recv(char *buf, size_t len) { return _buf.recv(buf, len); }

    template <typename T>
    inline int recv2(T& t) { return recv((char *)&t, sizeof(T)); }
    template <typename T>
    inline tstream& read2(T& t) { read((char *)&t, sizeof(T)); return *this; }
    template <typename T>
    inline tstream& write2(T& t) { write((const char *)&t, sizeof(T)); return *this; }

private:
    tcpbuf _buf;
};

#endif /* __TSTREAM_H__ */