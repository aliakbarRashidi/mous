#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <thread>
using namespace std;

#include <scx/Signal.h>
#include <scx/Socket.h>
using namespace scx;

#include "ClientPlayerHandler.h"
#include "ClientPlaylistHandler.h"

class Client
{
public:
    Client();
    ~Client();

    bool Run();
    void Stop();

    void SetConnectMaxRetry(int max);
    void SetConnectRetryInterval(int ms);

    void StopService();

    ClientPlayerHandler& PlayerHandler();
    ClientPlaylistHandler& PlaylistHandler();

    Signal<void ()>& SigTryConnect();
    Signal<void ()>& SigConnected();

    Signal<void (const std::vector<std::string>&)>& SigSuffixes()
    {
        return m_SigSuffixes;
    }

private:
    char* GetPayloadBuffer(char, int);
    inline void SendOut();

private:
    thread m_RecvThread;

    int m_ConnectMaxRetry = 25;
    int m_ConnectRetryInterval = 200;
    bool m_ConnectStopRetry = false;

    TcpSocket m_Socket;
    mutex m_SendOutBufMutex;
    vector<char> m_SendOutBuf;

    ClientPlayerHandler m_PlayerHandler;
    ClientPlaylistHandler m_PlaylistHandler;

    Signal<void ()> m_SigTryConnect;
    Signal<void ()> m_SigConnected;
    Signal<void (const std::vector<std::string>&)> m_SigSuffixes;
};

