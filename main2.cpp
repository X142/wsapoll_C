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
    virtual WSAPOLLFD const* OnEvent(const short revents) const = 0;
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

    WSAPOLLFD const* p_in_ary_pollfd = nullptr;
    // main 関数に返すポインタ
    // この位置に const が入っているのは必然。
    // 管理しているオブジェクトの内容を別の管理主体に書き換えられてはならない。

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
        const int idx_pos_in_ary_pollfd = int(p_in_ary_pollfd - m_ary_pollfd);

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
        p_in_ary_pollfd = &ary_pollfd_extd[idx_pos_in_ary_pollfd];

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

    WSAPOLLFD const* Delete()
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
        --p_in_ary_pollfd;

        return p_in_ary_pollfd;
    }

    I_Socket_Type const* GetEventSocket(WSAPOLLFD const* const _p_in_ary_pollfd)
    // 引数：
    // 修正前の &_p_in_ary_pollfd の場合、
    // main 関数の管理オブジェクトを変更することになり、OOP の考え方に馴染まない。
    // 
    // 1 つ目の const：
    // 今回は WSAPOLLFD const* を受け取ることが前提になっているわけだけど、、、
    // 一般論として、
    // 関数内で変更しないものを const 引数で受け取れば、最適化を促すという話がある。
    // 関数呼び出し前、
    // main 関数側が *p_in_ary_pollfd をアクセスしていた場合には、
    // *p_in_ary_pollfd の内容がどこかのレジスタに保存されている。
    // この const が付いていれば、関数呼び出し時、そのレジスタを破棄することはなく
    // 関数呼び出し後も、そのレジスタを利用できる。
    // 結果的に、不要なメモリアクセスを発生させないことをコンパイラに指示しているということであり、
    // const は変更しないという意味論だけではないということ。
    // 
    // メンバ関数間でエイリアスをもつメンバ変数群についても同様の話がいえる。
    // メンバ関数の const {} は、先述のレジスタ破棄を防止するというコンパイラへの指示でもある。
    {
        p_in_ary_pollfd = _p_in_ary_pollfd;
        const UINT idx = (UINT)(p_in_ary_pollfd - m_ary_pollfd);
        return m_vec_Socket[idx];
    }

    std::pair<WSAPOLLFD const*, const ULONG> Get_params_WSAPoll()
    // 変更前：std::pair<WSAPOLLFD*, const ULONG>
    // 変更後：std::pair<WSAPOLLFD const*, const ULONG>
    // 管理しているオブジェクト（m_ary_pollfd）を他の管理主体に書き換えられてはならない
    {
        return { m_ary_pollfd, pcs_valid_pollfd }; // RVO
    }

    WSAPOLLFD const* Get_p_in_ary_pollfd()
    {
        return p_in_ary_pollfd;
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

    virtual WSAPOLLFD const* OnEvent(const short revents) const override
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
            
            return m_p_WsaPollfd->Delete();
        }
        
        return m_p_WsaPollfd->Get_p_in_ary_pollfd();
    
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

    virtual WSAPOLLFD const* OnEvent(const short revents) const override
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

        return m_p_WsaPollfd->Get_p_in_ary_pollfd();
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

        // API に合わせてキャストを行うのは仕方がない
        int cnt_Event_WSAPoll = WSAPoll((WSAPOLLFD*)ary_pollfd, pcs_valid_pollfd, -1);

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
        // この位置に const が入っているのは必然。
        // ary_pollfd の内容は WsaPollfd class が管理しているものだから、こちらから書き換えることはできない。
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
                // 1 つ目の const は必然。
                // EventSocket の内容は WsaPollfd class が管理しているものだから、こちらから書き換えることはできない。
                p_in_ary_pollfd = EventSocket->OnEvent(revents);
                // p_in_ary_pollfd は main 関数の管理オブジェクトなので、main 関数が書き換える。
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