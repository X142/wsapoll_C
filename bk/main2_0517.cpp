#include <ws2tcpip.h> // struct sockaddr_in6 (ws2ipdef.h), inet_ntop
#include <iostream>
#include <string.h>

// ---------------------------------------------------------------
#define NUM_listening_Port 12345
#define NUM_backlog_for_listen 10

#define PCS_client_on_init 10

SOCKET create_fd_sock_listener()
{
    const SOCKET fd_sock_listener = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd_sock_listener == INVALID_SOCKET)
    {
        throw "!!! Server:: socket()";
    }

    {
        ULONG set_nonblocking = 1;
        if (ioctlsocket(fd_sock_listener, FIONBIO, &set_nonblocking) == SOCKET_ERROR)
        {
            throw "!!! Server:: ioctlsocket()";
        }
    }

    sockaddr_in6 addr_listener;
    memset(&addr_listener, 0, sizeof(addr_listener));
    addr_listener.sin6_family = AF_INET6;
    addr_listener.sin6_port = htons(NUM_listening_Port);
    addr_listener.sin6_addr = in6addr_loopback;
    
    if (bind(fd_sock_listener, (sockaddr*)&addr_listener, sizeof addr_listener) != 0)
    {
        throw "!!! Server:: bind()";
    }

    if (listen(fd_sock_listener, NUM_backlog_for_listen) != 0)
    {
        throw "!!! Server:: listen()";
    }

    return fd_sock_listener;
}

SOCKET create_fd_sock_client(const SOCKET fd_sock_listener, sockaddr* addr, int* len_addr)
{
    const SOCKET fd_sock_client = accept(fd_sock_listener, addr, len_addr);
    if (fd_sock_client == INVALID_SOCKET)
    {
        throw "!!! Server:: accept()";
    }
    {
        ULONG set_nonblocking = 1;
        if (ioctlsocket(fd_sock_listener, FIONBIO, &set_nonblocking) == SOCKET_ERROR)
        {
            throw "!!! Server:: ioctlsocket()";
        }
    }
    return fd_sock_client;
}

void Read_sock_client(const SOCKET fd_sock_client)
{
    constexpr int EN_bytes_buf_recv = 2000;
    char buf[EN_bytes_buf_recv];
    int bytes_recved = recv(fd_sock_client, buf, EN_bytes_buf_recv, 0);
    if (bytes_recved < EN_bytes_buf_recv)
    {
        buf[bytes_recved] = '\0';
    }
    else
    {
        buf[EN_bytes_buf_recv - 1] = '\0';
    }
    std::cout << "  " << "fd: " << fd_sock_client << ": recved bytes: " << bytes_recved << std::endl;
    std::cout << buf << std::endl;
}

static inline void show_event_info(const char* events, short flag)
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
void DBG_Show_pollfd(WSAPOLLFD* _ary_pollfd, ULONG _pcs_valid_pollfd_in_ary)
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

void Close_sockets(WSAPOLLFD* _ary_pollfd, ULONG _pcs_valid_pollfd_in_ary)
{
    for (ULONG idx = 0; idx < _pcs_valid_pollfd_in_ary; ++idx)
    {
        closesocket(_ary_pollfd[idx].fd);
    }
    std::cout << "Close_sockets()" << std::endl;
    return;
}

// ----------------------------------------------------------------------------
int DBG_cnt = 16;

// ----------------------------------------------------------------------------
int main_2()
{
    // +1 は、サーバーソケットを追加するため
    WSAPOLLFD ary_pollfd[PCS_client_on_init + 1] = {};
    ULONG pcs_valid_pollfd_in_ary = 0;
    try 
    {
        for (int i = 0; i < PCS_client_on_init + 1; ++i)
        {
            ary_pollfd[i].fd = INVALID_SOCKET;
            // events, revents をクリアする必要はない
            // If the fd member of the WSAPOLLFD structure is set to a negative value, 
            // the structure is ignored by the WSAPoll function call, 
            // and the revents member is cleared upon return.
        }

        const SOCKET fd_sock_listener = create_fd_sock_listener();
        ary_pollfd[0] = { fd_sock_listener, POLLRDNORM, 0 };
        pcs_valid_pollfd_in_ary++;

        UINT idx_alloc_next_in_ary_pollfd = 1;
        const UINT idx_tmnt_in_ary_pollfd = PCS_client_on_init + 1;
        int cnt_event_WSAPoll;

        std::cout << "Server:: listen start" << "\n" << std::endl;
        for (;;)
        {
            // Debug
            if (--DBG_cnt == 0) { break; }

            cnt_event_WSAPoll = WSAPoll(ary_pollfd, pcs_valid_pollfd_in_ary, -1);

            if (cnt_event_WSAPoll == SOCKET_ERROR)
            {
                throw "!!! WSAPoll()";
            }
            // timeout > 0
            if (cnt_event_WSAPoll == 0)
            {
                continue;
            }

            // Debug
            {
                std::cout << ">>> WSAPoll()" << ": " << cnt_event_WSAPoll << "\t" << "DBG_cnt: " << std::dec << DBG_cnt << std::endl;
                DBG_Show_pollfd(ary_pollfd, pcs_valid_pollfd_in_ary);
            }

            int idx_pos_in_ary_pollfd = 0;
            for (;;)
            {
                // Debug
                //{
                //     std::cout << "\t\t" << "idx_pos: " << idx_pos_in_ary_pollfd << std::endl;
                //     std::cout << "\t\t" << "cnt_event_WSAPoll = " << cnt_event_WSAPoll << std::endl;
                //}

                if (cnt_event_WSAPoll == 0)
                {
                    break;
                }
                
                const short revents = ary_pollfd[idx_pos_in_ary_pollfd].revents;
                // Server
                if (idx_pos_in_ary_pollfd == 0)
                {
                    if (revents != 0)
                    {
                        std::cout << "=== Server Event " << std::endl;
                        if (revents & POLLRDNORM)
                        {
                            if (idx_alloc_next_in_ary_pollfd == idx_tmnt_in_ary_pollfd)
                            {
                                throw "!!! Server:: idx_alloc_next_in_ary_pollfd == idx_tmnt_in_ary_pollfd";
                            }

                            const SOCKET fd_sock_client = create_fd_sock_client(fd_sock_listener, nullptr, nullptr);
                            ary_pollfd[idx_alloc_next_in_ary_pollfd] = { fd_sock_client, POLLRDNORM, 0 };
                            std::cout << "  " << "+++ new socket: " << "idx: " << idx_alloc_next_in_ary_pollfd << ", " << "fd: " << fd_sock_client << std::endl;
                            ++pcs_valid_pollfd_in_ary;
                            ++idx_alloc_next_in_ary_pollfd;
                        }
                        --cnt_event_WSAPoll;
                    }
                }
                // Client
                else 
                {
                    if (revents != 0)
                    {
                        std::cout << "=== Client Event " << std::endl;
                        if (revents & POLLRDNORM)
                        {
                            Read_sock_client(ary_pollfd[idx_pos_in_ary_pollfd].fd);
                        }
                        else if (revents & POLLHUP)
                        {
                            std::cout << "  " << "--- deleted socket: " << "idx: " << idx_pos_in_ary_pollfd << ", " << "fd: " << ary_pollfd[idx_pos_in_ary_pollfd].fd << std::endl;
                            closesocket(ary_pollfd[idx_pos_in_ary_pollfd].fd);
                            memmove(
                                &ary_pollfd[idx_pos_in_ary_pollfd],
                                &ary_pollfd[idx_pos_in_ary_pollfd + 1],
                                ((pcs_valid_pollfd_in_ary - 1) - (idx_pos_in_ary_pollfd + 1) + 1) * sizeof(WSAPOLLFD)
                            ); // 第 3 引数が 0 の場合、memmove は何もしない
                            ary_pollfd[pcs_valid_pollfd_in_ary - 1].fd = INVALID_SOCKET;
                            --pcs_valid_pollfd_in_ary;
                            --idx_alloc_next_in_ary_pollfd;
                            --idx_pos_in_ary_pollfd;
                        }
                        --cnt_event_WSAPoll;
                    }
                }

                ++idx_pos_in_ary_pollfd;
            }
            
            // Debug
            {
                std::cout << "\n";
                std::cout << "<<< RESULT" << std::endl;
                DBG_Show_pollfd(ary_pollfd, pcs_valid_pollfd_in_ary);
                std::cout << "\n";
            }
        }
    }
    catch (char const * const)
    {
        Close_sockets(ary_pollfd, pcs_valid_pollfd_in_ary);
        throw;
    }

    Close_sockets(ary_pollfd, pcs_valid_pollfd_in_ary);

    return 0;
}
