/*
    main: WSAPoll()
    WsaPollfd: WSAPOLLFD[] に関する処理、OnEvent()
*/

#include <ws2tcpip.h> // struct sockaddr_in6 (ws2ipdef.h), inet_ntop
#include <iostream>
#include <vector>
#include <unordered_map> // Debug

// ----------------------------------------------------------------------------------------------------------------
#define NUM_listening_Port 12345
#define NUM_backlog_for_listen 10

#define PCS_client_on_init 2
#define PCS_client_in_extd 2

// ----------------------------------------------------------------------------------------------------------------
std::unordered_map<SHORT, const char*> d_event_code
{
    std::pair{0x0000, "-"}, 
    std::pair{0x0100, "POLLRDNORM"},
    std::pair{0x0200, "POLLRDBAND"},
    std::pair{0x0300, "POLLIN"},
    std::pair{0x0400, "POLLPRI"},
    std::pair{0x0010, "POLLWRNORM"},
    std::pair{0x0010, "POLLOUT"},
    std::pair{0x0020, "POLLWRBAND"},
    std::pair{0x0001, "POLLERR"},
    std::pair{0x0002, "POLLHUP"},
    std::pair{0x0004, "POLLNVAL"}
};
void DBG_Show_pollfd(const WSAPOLLFD* const ary_pollfd, const ULONG pcs_valid_pollfd_in_ary)
{
    std::cout << "ary_pollfd" << "  " << "pcs: " << pcs_valid_pollfd_in_ary << std::endl;
    for (ULONG idx = 0; idx < pcs_valid_pollfd_in_ary; ++idx)
    {
        std::cout << "  " << "[" << idx << "]" << std::endl;
        std::cout << "    " << "->fd: " << std::dec << ary_pollfd[idx].fd << std::endl;
        std::cout << "    " << "->events: " << d_event_code[ary_pollfd[idx].events] << "(" << "0x" << std::hex <<ary_pollfd[idx].events << ")" << std::endl;
        std::cout << "    " << "->revents: " << d_event_code[ary_pollfd[idx].revents] << "(" << "0x" << std::hex << ary_pollfd[idx].revents << ")" << std::endl;;
    }
}

// ----------------------------------------------------------------------------------------------------------------
struct I_Socket_Type
{
    virtual ~I_Socket_Type() {}
    virtual WSAPOLLFD const* OnEvent(const short revents, WSAPOLLFD const* p_in_ary_pollfd) const = 0;
};

// ----------------------------------------------------------------------------------------------------------------
class WsaPollfd
{
private:
    const UINT size_on_init;
    const UINT unit_in_extd;

    WSAPOLLFD* m_ary_pollfd = new WSAPOLLFD[size_on_init];
    UINT pcs_valid_pollfd = 0;
    UINT capacity_ary_pollfd;
    
    std::vector<I_Socket_Type*> m_vec_Socket;

public:
    WsaPollfd(const UINT size_on_init, const UINT unit_in_extd)
        : size_on_init{ size_on_init }, unit_in_extd{ unit_in_extd }, capacity_ary_pollfd { size_on_init }
    {
        if (m_ary_pollfd == nullptr)
        {
            throw "!!! WsaPollfd:: new WSAPOLLFD[size_on_init]";
        }

        for (UINT idx = 0; idx < size_on_init; ++idx)
        {
            m_ary_pollfd[idx].fd = INVALID_SOCKET;
            // events, revents をクリアする必要はない
        }
    }

    ~WsaPollfd()
    {
        delete[] m_ary_pollfd;

        std::vector<I_Socket_Type*>::iterator itr_Socket = m_vec_Socket.begin();
        for (UINT idx = 0; idx < pcs_valid_pollfd; ++idx)
        {
            delete *itr_Socket;
            ++itr_Socket;
        }
    }

    void Extend()
    {
        const UINT capacity_ary_pollfd_extd = capacity_ary_pollfd + unit_in_extd;
        WSAPOLLFD* const ary_pollfd_extd = new WSAPOLLFD[capacity_ary_pollfd_extd];
        if (ary_pollfd_extd == nullptr)
        {
            throw "WsaPollfd:: Extend(): new WSAPOLLFD[capacity_ary_pollfd_extd]";
        }

        std::memcpy(ary_pollfd_extd, m_ary_pollfd, capacity_ary_pollfd * sizeof WSAPOLLFD);
        for (UINT idx = capacity_ary_pollfd; idx < capacity_ary_pollfd_extd; ++idx)
        {
            ary_pollfd_extd[idx].fd = INVALID_SOCKET;
            // events, revents をクリアする必要はない
        }
        delete[] m_ary_pollfd;

        capacity_ary_pollfd = capacity_ary_pollfd_extd;
        m_ary_pollfd = ary_pollfd_extd;

        // Debug
        {
            std::cout << "  " << "/// Extended" << "  " << capacity_ary_pollfd - unit_in_extd << " -> " << capacity_ary_pollfd << std::endl;
            // DBG_Show_pollfd(m_ary_pollfd, pcs_valid_pollfd);
        }
    }

    void Set_pollfd_and_Socket(const WSAPOLLFD* const p_pollfd, const I_Socket_Type* const p_Socket)
    {
        // 自動拡張
        if (pcs_valid_pollfd >= capacity_ary_pollfd)
        {
            Extend();
        }

        m_ary_pollfd[pcs_valid_pollfd] = *p_pollfd;
        m_vec_Socket.push_back((I_Socket_Type* const)p_Socket);
        ++pcs_valid_pollfd;

        // Debug
        {
            std::cout << "  " << "fd: " << std::dec << p_pollfd->fd << std::endl;
        }
    }

   void Delete(WSAPOLLFD const* p_in_ary_pollfd)
    {
        // Debug
        {
            std::cout << "  " << "fd: " << std::dec << p_in_ary_pollfd->fd << std::endl;
        }

        const UINT idx = (UINT)(p_in_ary_pollfd - m_ary_pollfd);
        {
            std::memcpy(
                &m_ary_pollfd[idx],
                &m_ary_pollfd[idx + 1],
                ((pcs_valid_pollfd - 1) - (idx + 1) + 1) * sizeof WSAPOLLFD
            );
            m_ary_pollfd[pcs_valid_pollfd - 1].fd = INVALID_SOCKET;
        }
        {
            delete m_vec_Socket[idx];
            (void)m_vec_Socket.erase(m_vec_Socket.begin() + (int)idx);
        }
        --pcs_valid_pollfd;
    }

    I_Socket_Type const* GetEventSocket(WSAPOLLFD const* p_in_ary_pollfd)
    {
        //p_in_ary_pollfd = _p_in_ary_pollfd;
        if (p_in_ary_pollfd == nullptr)
        {
            throw "WsaPollfd:: GetEventSocket(): p_in_ary_pollfd が設定されていません";
        }
        const UINT idx = (UINT)(p_in_ary_pollfd - m_ary_pollfd);
        return m_vec_Socket[idx];
    }

    std::pair<WSAPOLLFD*, const ULONG> Get_params_WSAPoll()
    {
        return { m_ary_pollfd, pcs_valid_pollfd }; // RVO
    }

    WSAPOLLFD const* Get_ary_pollfd()
    {
        return m_ary_pollfd;
    }
};

// ----------------------------------------------------------------------------------------------------------------
class Socket_client : public I_Socket_Type
{
private:
    const sockaddr_in6 mc_addr_client = { 0 };
    const int len_addr_client = sizeof mc_addr_client;
    const SOCKET mc_fd_sock_client;
    WsaPollfd* const m_p_WsaPollfd;

public:
    Socket_client(const SOCKET fd_sock_listener, WsaPollfd* const p_WsaPollfd)
    try
        : mc_fd_sock_client{ accept(fd_sock_listener, (sockaddr*)&mc_addr_client, (int*)&len_addr_client) }
        , m_p_WsaPollfd { p_WsaPollfd }
    {
        if (mc_fd_sock_client == INVALID_SOCKET)
        {
            throw "!!! Socket_client:: accept()";
        }

        {
            ULONG mode_socket_nonblocking = 1;
            if (ioctlsocket(mc_fd_sock_client, FIONBIO, &mode_socket_nonblocking) == SOCKET_ERROR)
            {
                throw "!!! Socket_client:: ioctlsocket()";
            }
        }

        Rgst_to_WsaPollfd();
    }
    catch ([[maybe_unused]] const char* const)
    {
        if (mc_fd_sock_client != INVALID_SOCKET)
        {
            closesocket(mc_fd_sock_client);
        }
        throw;
    }
    void Rgst_to_WsaPollfd()
    {
        const WSAPOLLFD pollfd_client = { mc_fd_sock_client, POLLRDNORM, 0 };
        m_p_WsaPollfd->Set_pollfd_and_Socket(&pollfd_client, this);
    }

    ~Socket_client()
    {
        if (mc_fd_sock_client != INVALID_SOCKET)
        {
            closesocket(mc_fd_sock_client);
        }
    }

    virtual WSAPOLLFD const* OnEvent(const short revents, WSAPOLLFD const* p_in_ary_pollfd) const override
    {
        if (revents & POLLRDNORM)
        {
            // Debug
            {
                std::cout << "=== Client:: Read()" << std::endl;
                std::cout << "  " << "fd: " << std::dec << mc_fd_sock_client << std::endl;
            }

            constexpr int EN_bytes_buf_recv = 2000;
            char buf_recv[EN_bytes_buf_recv];
            const int bytes_recved = recv(mc_fd_sock_client, buf_recv, sizeof buf_recv, 0);
            if (bytes_recved < sizeof buf_recv)
            {
                buf_recv[bytes_recved] = '\0';
            }
            else
            {
                buf_recv[EN_bytes_buf_recv - 1] = '\0';
            }

            // Debug
            {
                std::cout << buf_recv << std::endl;
            }
        }
        else if (revents & POLLHUP)
        {
            // Debug
            {
                std::cout << "--- Client:: Delete()" << std::endl;
            }
            
            m_p_WsaPollfd->Delete(p_in_ary_pollfd);
            --p_in_ary_pollfd;
        }
        
        return p_in_ary_pollfd;
    }
};

// ----------------------------------------------------------------------------------------------------------------
class Socket_server : public I_Socket_Type
{
private:
    const sockaddr_in6 mc_addr_sock_listener = { AF_INET6, htons(NUM_listening_Port), 0, in6addr_loopback, { 0 } };
    const SOCKET mc_fd_sock_listener = socket(AF_INET6, SOCK_STREAM, 0);
    WsaPollfd* const m_p_WsaPollfd;

public:
    Socket_server(WsaPollfd* const p_WsaPollfd)
    try
        :  m_p_WsaPollfd { p_WsaPollfd }
    {
        if (mc_fd_sock_listener == INVALID_SOCKET)
        {
            throw "!!! Socket_server:: socket()";
        }

        {
            const BOOL mode_socket_reuse = 1;
            if (setsockopt(mc_fd_sock_listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&mode_socket_reuse, sizeof mode_socket_reuse) == SOCKET_ERROR)
            {
                throw "!!! Socket_server:: setsockopt()";
            }
        }

        {
            const ULONG mode_socket_nonblocking = 1;
            if (ioctlsocket(mc_fd_sock_listener, FIONBIO, (ULONG*)&mode_socket_nonblocking) == SOCKET_ERROR)
            {
                throw "!!! Socket_server:: socket()";
            }
        }

        if (bind(mc_fd_sock_listener, (sockaddr*)&mc_addr_sock_listener, sizeof mc_addr_sock_listener) == SOCKET_ERROR)
        {
            throw "!!! Socket_server:: bind()";
        }

        if (listen(mc_fd_sock_listener, NUM_backlog_for_listen) == SOCKET_ERROR)
        {
            throw "!!! Socket_server:: listen()";
        }

        Rgst_to_WsaPollfd();
    }
    catch([[maybe_unused]] const char* const err)
    {
        if (mc_fd_sock_listener != INVALID_SOCKET)
        {
            closesocket(mc_fd_sock_listener);
        }
        throw;
    }
    void Rgst_to_WsaPollfd()
    {
        const WSAPOLLFD pollfd_listener = { mc_fd_sock_listener, POLLRDNORM, 0 };
        m_p_WsaPollfd->Set_pollfd_and_Socket(&pollfd_listener, this);
    }

    ~Socket_server()
    {
        if (mc_fd_sock_listener != INVALID_SOCKET)
        {
            closesocket(mc_fd_sock_listener);
        }
    }

    virtual WSAPOLLFD const* OnEvent(const short revents, WSAPOLLFD const* p_in_ary_pollfd) const override
    {
        if (revents & POLLRDNORM)
        {
            // Debug
            {
                std::cout << "+++ Server:: new Socket_client()" << std::endl;
            }

            WSAPOLLFD const* const ary_pollfd_old = m_p_WsaPollfd->Get_ary_pollfd();

            if (new Socket_client(mc_fd_sock_listener, m_p_WsaPollfd) == nullptr)
            {
                throw "Socket_server:: OnEvent(): new Socket_client(mc_fd_sock_listener, m_p_WsaPollfd))";
            }

            WSAPOLLFD const* const ary_pollfd_new = m_p_WsaPollfd->Get_ary_pollfd();
            if (ary_pollfd_old != ary_pollfd_new) // リアロケーションが発生したか判定
            {
                p_in_ary_pollfd = &ary_pollfd_new[(UINT)(p_in_ary_pollfd - ary_pollfd_old)];
            }
        }

        return p_in_ary_pollfd;
    }
};

// ----------------------------------------------------------------------------------------------------------------
int DBG_cnt = 10;

// ----------------------------------------------------------------------------------------------------------------
int main_2()
{
    // Debug
    {
        std::cout << "=== Socket_server creating..." << std::endl;
    }

    WsaPollfd wsapollfd(PCS_client_on_init, PCS_client_in_extd);
    if (new Socket_server(&wsapollfd) == nullptr)
    {
        throw "new Socket_server(&wsapollfd)";
    }

    // Debug
    {
        std::cout << std::endl;
        DBG_Show_pollfd(wsapollfd.Get_params_WSAPoll().first, wsapollfd.Get_params_WSAPoll().second);
        std::cout << std::endl;
        std::cout << "=== Listening...\n" << std::endl;
    }

    for (;;)
    {
        // Debug
        {
            if (--DBG_cnt == -1)
            {
                break;
            }
        }

        auto [ary_pollfd, pcs_valid_pollfd] = wsapollfd.Get_params_WSAPoll();

        int cnt_Event_WSAPoll = WSAPoll(ary_pollfd, pcs_valid_pollfd, -1);

        if (cnt_Event_WSAPoll == SOCKET_ERROR)
        {
            throw "!!! WSAPoll()";
        }

        // valid if timeout > 0
        if (cnt_Event_WSAPoll == 0)
        {
            continue;
        }

        // Debug
        {
            std::cout << ">>> WSAPoll() = " << cnt_Event_WSAPoll << std::endl;
            DBG_Show_pollfd(ary_pollfd, pcs_valid_pollfd);
        }

        WSAPOLLFD const* p_in_ary_pollfd = ary_pollfd;
        for (;;)
        {
            if (cnt_Event_WSAPoll == 0)
            {
                break;
            }

            const short revents = p_in_ary_pollfd->revents;
            if (revents)
            {
                const I_Socket_Type* const EventSocket = wsapollfd.GetEventSocket(p_in_ary_pollfd);
                p_in_ary_pollfd = EventSocket->OnEvent(revents, p_in_ary_pollfd);
                --cnt_Event_WSAPoll;
            }

            ++p_in_ary_pollfd;
        }

        // Debug
        {
            std::cout << std::endl;
            std::cout << "<<< RESULT" << std::endl;
            DBG_Show_pollfd(wsapollfd.Get_params_WSAPoll().first, wsapollfd.Get_params_WSAPoll().second);
            std::cout << std::endl;
        }
    }

    return 0;
}
