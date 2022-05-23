/*
    main: WSAPoll()
    WsaPollfd: WSAPOLLFD[] に関する処理、OnEvent()
*/

#include <ws2tcpip.h> // struct sockaddr_in6 (ws2ipdef.h), inet_ntop
#include <iostream>
#include <vector>

// ----------------------------------------------------------------------------------------------------------------
#define NUM_listening_Port 12345
#define NUM_backlog_for_listen 10

#define PCS_client_on_init 2
#define PCS_client_in_extd 2

// ----------------------------------------------------------------------------------------------------------------
static inline void show_event_info(const char* events, const short flag)
{
    /*
    // Event flag definitions for WSAPoll().
    #define POLLRDNORM  0x0100
    #define POLLRDBAND  0x0200
    #define POLLIN      (POLLRDNORM | POLLRDBAND)
    #define POLLPRI     0x0400

    #define POLLWRNORM  0x0010
    #define POLLOUT     (POLLWRNORM)
    #define POLLWRBAND  0x0020

    #define POLLERR     0x0001
    #define POLLHUP     0x0002
    #define POLLNVAL    0x0004
    */
    switch (flag)
    {
    case 0x0:
        std::cout << "    " << "->" << events << ": " << "not in queried state" << "(" << "0x" << std::hex << flag << ")" << std::endl; break;
    case 0x0002:
        std::cout << "    " << "->" << events << ": " << "POLLHUP" << "(" << "0x" << std::hex << flag << ")" << std::endl; break;
    case 0x0100:
        std::cout << "    " << "->" << events << ": " "POLLRDNORM" << "(" << "0x" << std::hex << flag << ")" << std::endl; break;
    default:
        std::cout << "    " << "->" << events << ": " << "未対応のフラグ" << "(" << "0x" << std::hex << flag << ")" << std::endl; break;
    }
}
void DBG_Show_pollfd(const WSAPOLLFD* const _ary_pollfd, const ULONG _pcs_valid_pollfd_in_ary)
{
    std::cout << "ary_pollfd" << "  " << "pcs: " << _pcs_valid_pollfd_in_ary << std::endl;
    for (ULONG idx = 0; idx < _pcs_valid_pollfd_in_ary; ++idx)
    {
        std::cout << "  " << "[" << idx << "]" << std::endl;
        std::cout << "    " << "->fd: " << std::dec << _ary_pollfd[idx].fd << std::endl;
        show_event_info("events", _ary_pollfd[idx].events);
        show_event_info("revents", _ary_pollfd[idx].revents);
    }
}

// ----------------------------------------------------------------------------------------------------------------
struct I_Socket_Type
{
    virtual ~I_Socket_Type() {}
    virtual void OnEvent([[maybe_unused]] const short revents) const {}
};

// ----------------------------------------------------------------------------------------------------------------
class WsaPollfd
{
    const UINT size_on_init;
    const UINT unit_in_extend;

    WSAPOLLFD* m_ary_pollfd = new WSAPOLLFD[size_on_init];
    UINT m_pcs_valid_pollfd = 0;
    UINT capacity_ary_pollfd;
    
    std::vector<I_Socket_Type*> m_vec_Socket;

    WSAPOLLFD ** p_p_pos_in_ary_pollfd_in_main = nullptr;

public:
    WsaPollfd(const UINT size_on_init, const UINT unit_in_extend)
        : size_on_init{ size_on_init }, unit_in_extend{ unit_in_extend }, capacity_ary_pollfd { size_on_init }
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
        for (UINT idx = 0; idx < m_pcs_valid_pollfd; ++idx)
        {
            delete *itr_Socket;
            ++itr_Socket;
        }
    }

    void Extend()
    {
        const int idx_pos_in_ary_pollfd = int(*p_p_pos_in_ary_pollfd_in_main - m_ary_pollfd);

        const UINT capacity_ary_pollfd_extd = capacity_ary_pollfd + unit_in_extend;
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

        m_ary_pollfd = ary_pollfd_extd;
        capacity_ary_pollfd = capacity_ary_pollfd_extd;

        *p_p_pos_in_ary_pollfd_in_main = &ary_pollfd_extd[idx_pos_in_ary_pollfd];

        // Debug
        {
            std::cout << "  " << "/// Extended" << "  " << capacity_ary_pollfd - unit_in_extend << " -> " << capacity_ary_pollfd << std::endl;
            // DBG_Show_pollfd(m_ary_pollfd, m_pcs_valid_pollfd);
        }
    }

    void Set_pollfd_and_Socket(WSAPOLLFD* p_pollfd, I_Socket_Type* p_Socket)
    {
        // 自動拡張
        if (m_pcs_valid_pollfd >= capacity_ary_pollfd)
        {
            Extend();
        }

        m_ary_pollfd[m_pcs_valid_pollfd] = *p_pollfd;
        m_vec_Socket.push_back(p_Socket);
        ++m_pcs_valid_pollfd;

        // Debug
        {
            std::cout << "  " << "fd: " << std::dec << p_pollfd->fd << std::endl;
        }
    }

    void Delete()
    {
        // Debug
        {
            std::cout << "  " << "fd: " << std::dec << (*p_p_pos_in_ary_pollfd_in_main)->fd << std::endl;
        }

        const UINT idx = (UINT)(*p_p_pos_in_ary_pollfd_in_main - m_ary_pollfd);
        {
            std::memcpy(
                &m_ary_pollfd[idx],
                &m_ary_pollfd[idx + 1],
                ((m_pcs_valid_pollfd - 1) - (idx + 1) + 1) * sizeof WSAPOLLFD
            );
            m_ary_pollfd[m_pcs_valid_pollfd - 1].fd = INVALID_SOCKET;
        }
        {
            delete m_vec_Socket[idx];
            (void)m_vec_Socket.erase(m_vec_Socket.begin() + (int)idx);
        }
        --m_pcs_valid_pollfd;
        --*p_p_pos_in_ary_pollfd_in_main;
    }

    I_Socket_Type* GetEventSocket(WSAPOLLFD ** _p_p_pos_in_ary_pollfd_in_main)
    {
        p_p_pos_in_ary_pollfd_in_main = _p_p_pos_in_ary_pollfd_in_main;
        const UINT idx = (UINT)(*p_p_pos_in_ary_pollfd_in_main - m_ary_pollfd);
        return m_vec_Socket[idx];
    }

    std::pair<WSAPOLLFD*, const ULONG> Get_params_WSAPoll()
    {
        return { m_ary_pollfd, m_pcs_valid_pollfd }; // RVO
    }
};

// ----------------------------------------------------------------------------------------------------------------
class Socket_client : public I_Socket_Type
{
    const sockaddr_in6 mc_addr_client = { 0 };
    const int len_addr_client = sizeof mc_addr_client;
    const SOCKET mc_fd_sock_client;
    WsaPollfd* const m_p_WsaPollfd;

public:
    Socket_client(const SOCKET fd_sock_listener, WsaPollfd* const p_WsaPollfd)
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
    void Rgst_to_WsaPollfd()
    {
        WSAPOLLFD pollfd_client = { mc_fd_sock_client, POLLRDNORM, 0 };
        m_p_WsaPollfd->Set_pollfd_and_Socket(&pollfd_client, this);
    }

    ~Socket_client()
    {
        closesocket(mc_fd_sock_client);
    }

    virtual void OnEvent(const short revents) const override
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
            
            m_p_WsaPollfd->Delete();
        }
    }
};

// ----------------------------------------------------------------------------------------------------------------
class Socket_server : public I_Socket_Type
{
    const sockaddr_in6 mc_addr_sock_listener = { AF_INET6, htons(NUM_listening_Port), 0, in6addr_loopback, { 0 } };
    const SOCKET mc_fd_sock_listener = socket(AF_INET6, SOCK_STREAM, 0);
    WsaPollfd* const m_p_WsaPollfd;

public:
    Socket_server(WsaPollfd* const p_WsaPollfd)
        :  m_p_WsaPollfd { p_WsaPollfd }
    {
        if (mc_fd_sock_listener == INVALID_SOCKET)
        {
            throw "!!! Socket_server:: socket()";
        }
        try
        {
            {
                BOOL mode_socket_reuse = 1;
                if (setsockopt(mc_fd_sock_listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&mode_socket_reuse, sizeof mode_socket_reuse) == SOCKET_ERROR)
                {
                    throw "!!! Socket_server:: setsockopt()";
                }
            }

            {
                ULONG mode_socket_nonblocking = 1;
                if (ioctlsocket(mc_fd_sock_listener, FIONBIO, &mode_socket_nonblocking) == SOCKET_ERROR)
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
            closesocket(mc_fd_sock_listener);
            throw;
        }
    }
    void Rgst_to_WsaPollfd()
    {
        WSAPOLLFD pollfd_listener = { mc_fd_sock_listener, POLLRDNORM, 0 };
        m_p_WsaPollfd->Set_pollfd_and_Socket(&pollfd_listener, this);
    }

    ~Socket_server()
    {
        closesocket(mc_fd_sock_listener);
    }

    virtual void OnEvent(const short revents) const override
    {
        if (revents & POLLRDNORM)
        {
            // Debug
            {
                std::cout << "+++ Server:: new Socket_client()" << std::endl;
            }

            if (new Socket_client(mc_fd_sock_listener, m_p_WsaPollfd) == nullptr)
            {
                throw "Socket_server:: OnEvent(): new Socket_client(mc_fd_sock_listener, m_p_WsaPollfd))";
            }
        }
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

        for (;;)
        {
            if (cnt_Event_WSAPoll == 0)
            {
                break;
            }
            const short revents = ary_pollfd->revents;
            if (revents)
            {
                const I_Socket_Type* const EventSocket = wsapollfd.GetEventSocket(&ary_pollfd);
                EventSocket->OnEvent(revents);
                --cnt_Event_WSAPoll;
            }
            ++ary_pollfd;
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