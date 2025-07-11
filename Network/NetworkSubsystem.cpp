#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "NetworkSubsystem.hpp"
#include <iostream>
#include <algorithm>

#include "Engine/Core/ErrorWarningAssert.hpp"

namespace
{
    // 辅助：uint64_t 和 SOCKET 互转
    inline SOCKET   ToSOCKET(uint64_t h) { return static_cast<SOCKET>(h); }
    inline uint64_t FromSOCKET(SOCKET s) { return static_cast<uint64_t>(s); }
}

NetworkSubsystem::NetworkSubsystem()
{
}

NetworkSubsystem::~NetworkSubsystem()
{
}

/**
 * Initializes the network subsystem with the specified configuration and prepares
 * the system for network communication. This method initializes the Windows
 * Sockets API (WinSock) version 2.2 and sets the initial states for client
 * and server as IDLE.
 *
 * @param config Reference to a NetworkConfig object containing the configuration
 *               parameters for the network subsystem.
 */
void NetworkSubsystem::Startup(NetworkConfig& config)
{
    m_config = config;

    // Initialize WinSock 2.2
    // WSAStartup initializes Windows Sockets API, parameter MAKEWORD(2,2) indicates using version 2.2
    WSADATA wsaData;
    int     res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0)
    {
        std::cerr << "WSAStartup failed with error: " << res << "\n";
        ERROR_AND_DIE("WSAStartup failed")
    }

    // Set the initial state
    m_clientState = ClientState::IDLE;
    m_serverState = ServerState::IDLE;
}

/**
 * Shuts down the network subsystem and cleans up resources used for network communication.
 * This method closes all active network connections, including the client socket, server
 * connections, and the listening socket. It also clears the internal connection list,
 * releases the Windows Sockets API (WinSock) resources, and resets the server/client states
 * to UNINITIALIZED.
 *
 * This function ensures proper cleanup of resources allocated during the operation of the
 * network subsystem, preparing the system for either application termination or re-initialization.
 */
void NetworkSubsystem::Shutdown()
{
    // Close the client socket
    if (m_clientSocket)
    {
        closesocket(ToSOCKET(m_clientSocket));
        m_clientSocket = 0;
    }

    // Close all client connections on the server
    for (auto& conn : m_connections)
    {
        if (conn.socketHandle)
        {
            closesocket(ToSOCKET(conn.socketHandle));
            conn.socketHandle = 0;
        }
    }
    m_connections.clear();

    // Close the listening socket
    if (m_serverListenSocket)
    {
        closesocket(ToSOCKET(m_serverListenSocket));
        m_serverListenSocket = 0;
    }

    //Clean up WinSock
    WSACleanup();

    // Reset state
    m_serverState = ServerState::UNINITIALIZED;
    m_clientState = ClientState::UNINITIALIZED;
}

/**
 * Starts the server by binding to the specified port and entering the listening state.
 * This prepares the server to accept incoming connections. The server must be in the
 * IDLE state before this method can be called. The function creates a non-blocking
 * TCP socket, binds it to the specified port, and begins listening on it.
 *
 * @param port The port number on which the server will listen for incoming connections.
 *             Should be provided in host byte order.
 * @return True if the server successfully enters the listening state, otherwise false.
 *         Failure may occur if the server is not in the IDLE state, socket creation fails,
 *         binding to the port fails, or the server fails to enter the listening state.
 */
bool NetworkSubsystem::StartServer(uint16_t port)
{
    if (m_serverState != ServerState::IDLE)
    {
        std::cerr << "Server is not in IDLE state\n";
        return false;
    }

    // Create a TCP socket
    // AF_INET: IPv4, SOCK_STREAM: TCP, IPPROTO_TCP: TCP protocol
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        std::cerr << "Failed to create server socket, error: " << WSAGetLastError() << "\n";
        return false;
    }

    // Set to non-blocking mode
    // FIONBIO: Set blocking/non-blocking mode, mode=1 means non-blocking
    u_long mode = 1;
    if (ioctlsocket(listenSock, FIONBIO, &mode) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set non-blocking mode, error: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        return false;
    }

    // Prepare address structure
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all network interfaces
    addr.sin_port        = htons(port); // Host byte order to network byte order

    // Bind to the specified port
    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to bind to port " << port << ", error: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        return false;
    }

    // Start listening, SOMAXCONN indicates the maximum length of the waiting connection queue
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Failed to listen, error: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        return false;
    }

    m_serverListenSocket = (unsigned int)FromSOCKET(listenSock);
    m_serverState        = ServerState::LISTENING;

    std::cout << "Server started listening on port " << port << "\n";
    return true;
}

/**
 * Shuts down the server and releases all associated resources. This method
 * closes the server listening socket as well as all active client connections,
 * ensuring that no lingering socket handles remain. The server's internal state
 * is then set to IDLE, indicating that it is no longer operational.
 */
void NetworkSubsystem::StopServer()
{
    if (m_serverListenSocket)
    {
        closesocket(ToSOCKET(m_serverListenSocket));
        m_serverListenSocket = 0;
    }

    // Close all client connections
    for (auto& conn : m_connections)
    {
        if (conn.socketHandle)
        {
            closesocket(ToSOCKET(conn.socketHandle));
            conn.socketHandle = 0;
        }
    }
    m_connections.clear();

    m_serverState = ServerState::IDLE;
}

/**
 * Initiates a client connection to the specified server IP and port. The method
 * creates a non-blocking socket and attempts to connect to the server. If the client
 * is not in the IDLE state or if any step fails, the connection will not proceed.
 *
 * @param serverIp The IP address of the server to connect to, as a string.
 * @param port The port number of the server to connect to.
 * @return True if the connection is successfully initiated; otherwise, false.
 */
bool NetworkSubsystem::StartClient(const std::string& serverIp, uint16_t port)
{
    if (m_clientState != ClientState::IDLE)
    {
        std::cerr << "Client is not in IDLE state\n";
        return false;
    }

    // Create a client socket
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET)
    {
        std::cerr << "Failed to create client socket, error: " << WSAGetLastError() << "\n";
        return false;
    }

    // Set non-blocking mode
    u_long mode = 1;
    if (ioctlsocket(clientSock, FIONBIO, &mode) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set non-blocking mode, error: " << WSAGetLastError() << "\n";
        closesocket(clientSock);
        return false;
    }

    // Prepare the server address
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // inet_pton: Converts a textual IP address to binary form in network byte order
    if (inet_pton(AF_INET, serverIp.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid IP address: " << serverIp << "\n";
        closesocket(clientSock);
        return false;
    }
    addr.sin_port = htons(port);

    // Start connecting (will return immediately in non-blocking mode)
    int result = connect(clientSock, (sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        // WSAEWOULDBLOCK indicates a connection is in progress in non-blocking mode
        if (error != WSAEWOULDBLOCK)
        {
            std::cerr << "Connect failed with error: " << error << "\n";
            closesocket(clientSock);
            return false;
        }
    }

    m_clientSocket = FromSOCKET(clientSock);
    m_clientState  = ClientState::CONNECTING;

    std::cout << "Client starting connection to " << serverIp << ":" << port << "\n";
    return true;
}

/**
 * Disconnects the client by closing its socket connection, clearing any
 * inbound or outbound data buffers, and resetting the client state to idle.
 *
 * This method ensures that any pending data associated with the client is discarded
 * and the client's current state is updated to reflect the disconnection. The client
 * socket is closed using the platform-specific Winsock API, and internal buffers
 * are cleared to avoid residual data processing.
 */
void NetworkSubsystem::DisconnectClient()
{
    if (m_clientSocket)
    {
        closesocket(ToSOCKET(m_clientSocket));
        m_clientSocket = 0;
    }

    // Clear the buffer
    m_incomingDataForMe.clear();
    m_outgoingDataForMe.clear();

    m_clientState = ClientState::IDLE;
}

/**
 * Updates the network subsystem by processing server and client logic. This method
 * manages server-side operations, such as accepting new client connections and
 * handling data transmission or disconnection from connected clients. Additionally,
 * it handles client-side operations, including connection attempts, data sending,
 * and receiving from the server.
 *
 * Server Logic:
 * - Accepts incoming client connections in non-blocking mode.
 * - Processes connected client data, handling outgoing and incoming message queues.
 * - Manages connection removals on client disconnection or errors.
 *
 * Client Logic:
 * - Manages the state of the connection to a server, handling connection success
 *   or failure.
 * - Sends queued outgoing client data to the server.
 * - Processes incoming data from the server and manages disconnection scenarios.
 *
 * Any errors during send or receive operations are logged, along with graceful
 * disconnections or failures. The function depends on non-blocking socket I/O,
 * selective I/O operations (`select`), and asynchronous processing mechanisms.
 */
void NetworkSubsystem::Update()
{
    /// Server logic
    if (m_serverState == ServerState::LISTENING)
    {
        // Try to accept new client connections
        SOCKET listenSocket = ToSOCKET(m_serverListenSocket);

        // accept returns immediately in non-blocking mode
        // If there is no pending connection, return INVALID_SOCKET and set error to WSAEWOULDBLOCK
        SOCKET newClientSocket = accept(listenSocket, nullptr, nullptr);

        if (newClientSocket != INVALID_SOCKET)
        {
            // Set non-blocking mode for new clients
            u_long mode = 1;
            if (ioctlsocket(newClientSocket, FIONBIO, &mode) == 0)
            {
                // Create a new connection object
                NetworkConnection conn;
                conn.socketHandle = FromSOCKET(newClientSocket);
                conn.state        = ClientState::CONNECTED;
                m_connections.push_back(std::move(conn));

                std::cout << "Server accepted new client connection. Total clients: " << m_connections.size() << "\n";
            }
            else
            {
                // Setting non-blocking failed, close this connection
                closesocket(newClientSocket);
            }
        }
#undef min
        // Process each connected client
        for (auto it = m_connections.begin(); it != m_connections.end();)
        {
            SOCKET clientSocket = ToSOCKET(it->socketHandle);
            bool   shouldRemove = false;

            //Send the data to be sent
            while (!it->outgoing.empty())
            {
                // Try to send data
                int               bytesToSend = static_cast<int>(std::min(it->outgoing.size(), size_t(1024)));
                std::vector<char> sendBuffer(it->outgoing.begin(), it->outgoing.begin() + bytesToSend);

                int sent = send(clientSocket, sendBuffer.data(), bytesToSend, 0);
                if (sent > 0)
                {
                    // Successfully sent, remove the sent data from the queue
                    it->outgoing.erase(it->outgoing.begin(), it->outgoing.begin() + sent);
                }
                else if (sent == SOCKET_ERROR)
                {
                    int error = WSAGetLastError();
                    if (error == WSAEWOULDBLOCK)
                    {
                        // The send buffer is full, try again later
                        break;
                    }
                    else
                    {
                        // An error occurred, marking for removal
                        std::cerr << "Server send error: " << error << "\n";
                        shouldRemove = true;
                        break;
                    }
                }
            }

            // Receive data
            if (!shouldRemove)
            {
                char recvBuffer[1024];
                int  received = recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);

                if (received > 0)
                {
                    // Add the received data to the queue
                    for (int i = 0; i < received; ++i)
                    {
                        it->incoming.push_back(static_cast<uint8_t>(recvBuffer[i]));
                    }
                }
                else if (received == 0)
                {
                    // The client closes the connection normally
                    std::cout << "Client disconnected gracefully\n";
                    shouldRemove = true;
                }
                else // received == SOCKET_ERROR
                {
                    int error = WSAGetLastError();
                    if (error != WSAEWOULDBLOCK)
                    {
                        // An error occurred (not WOULDBLOCK)
                        std::cerr << "Server recv error: " << error << "\n";
                        shouldRemove = true;
                    }
                    // WSAEWOULDBLOCK means no data is available to read, this is normal
                }
            }

            // If you need to remove the connection
            if (shouldRemove)
            {
                closesocket(clientSocket);
                it = m_connections.erase(it);
                std::cout << "Removed client connection. Remaining clients: " << m_connections.size() << "\n";
            }
            else
            {
                ++it;
            }
        }
    }

    /// Client Logic
    if (m_clientState == ClientState::CONNECTING)
    {
        // Use select to check the connection status
        SOCKET clientSocket = ToSOCKET(m_clientSocket);

        fd_set writeSet, exceptSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(clientSocket, &writeSet);
        FD_SET(clientSocket, &exceptSet);

        // select (timeout = 0) that returns immediately
        timeval timeout      = {0, 0};
        int     selectResult = select(0, nullptr, &writeSet, &exceptSet, &timeout);

        if (selectResult > 0)
        {
            if (FD_ISSET(clientSocket, &exceptSet))
            {
                // Connection failed
                std::cerr << "Client connection failed\n";
                DisconnectClient();
            }
            else if (FD_ISSET(clientSocket, &writeSet))
            {
                // Connection successful
                m_clientState = ClientState::CONNECTED;
                std::cout << "Client connected successfully\n";
            }
        }
        else if (selectResult == SOCKET_ERROR)
        {
            std::cerr << "Select error: " << WSAGetLastError() << "\n";
            DisconnectClient();
        }
        // selectResult == 0 means timeout, the connection is still in progress
    }
    else if (m_clientState == ClientState::CONNECTED)
    {
        SOCKET clientSocket = ToSOCKET(m_clientSocket);
#undef min
        //Send the data to be sent
        while (!m_outgoingDataForMe.empty())
        {
            int               bytesToSend = static_cast<int>(std::min(m_outgoingDataForMe.size(), size_t(1024)));
            std::vector<char> sendBuffer(m_outgoingDataForMe.begin(), m_outgoingDataForMe.begin() + bytesToSend);

            int sent = send(clientSocket, sendBuffer.data(), bytesToSend, 0);
            if (sent > 0)
            {
                // Successfully sent, removed from the queue
                m_outgoingDataForMe.erase(m_outgoingDataForMe.begin(), m_outgoingDataForMe.begin() + sent);
            }
            else if (sent == SOCKET_ERROR)
            {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                {
                    // The send buffer is full, try again later
                    break;
                }
                else
                {
                    // An error occurred, disconnecting
                    std::cerr << "Client send error: " << error << "\n";
                    DisconnectClient();
                    return;
                }
            }
        }

        // Receive data
        char recvBuffer[1024];
        int  received = recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);

        if (received > 0)
        {
            // Add the received data to the queue
            for (int i = 0; i < received; ++i)
            {
                m_incomingDataForMe.push_back(static_cast<uint8_t>(recvBuffer[i]));
            }
        }
        else if (received == 0)
        {
            // Server closes the connection
            std::cout << "Server closed connection\n";
            DisconnectClient();
        }
        else // received == SOCKET_ERROR
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                // An error occurred
                std::cerr << "Client recv error: " << error << "\n";
                DisconnectClient();
            }
            // WSAEWOULDBLOCK is normal, indicating that no data is available to read
        }
    }
}

/**
 * Queues the specified data to be sent to the server. The data will be added
 * to the outgoing queue and transmitted during the system's update cycle.
 *
 * @param data A vector of bytes containing the data to be sent to the server.
 */
void NetworkSubsystem::SendToServer(const std::vector<uint8_t>& data)
{
    // Add data to the send queue, and it will be automatically sent in Update
    m_outgoingDataForMe.insert(m_outgoingDataForMe.end(), data.begin(), data.end());
}

/**
 * Broadcasts the given data to all connected clients by appending it to their
 * respective outgoing buffers.
 *
 * @param data Reference to a vector containing the data to be broadcasted
 *             to all connected clients.
 */
void NetworkSubsystem::BroadcastToClients(const std::vector<uint8_t>& data)
{
    // Send data to all connected clients
    for (auto& conn : m_connections)
    {
        conn.outgoing.insert(conn.outgoing.end(), data.begin(), data.end());
    }
}

/**
 * Sends a block of data to a specific client identified by the given index.
 * The data is appended to the client's outgoing message queue, where it
 * will be processed and transmitted.
 *
 * @param clientIndex The index of the client in the connection list to
 *                    which the data will be sent.
 * @param data A reference to a vector of bytes containing the data to be
 *             sent to the specified client.
 */
void NetworkSubsystem::SendToClient(size_t clientIndex, const std::vector<uint8_t>& data)
{
    if (clientIndex < m_connections.size())
    {
        m_connections[clientIndex].outgoing.insert(
            m_connections[clientIndex].outgoing.end(),
            data.begin(),
            data.end()
        );
    }
}

/**
 * Checks if there is incoming data available for the server. This method evaluates
 * whether the data queue intended for the server is non-empty.
 *
 * @return True if there is incoming data for the server, otherwise false.
 */
bool NetworkSubsystem::HasServerData() const
{
    return !m_incomingDataForMe.empty();
}

/**
 * Retrieves and clears the incoming data buffer containing data
 * received from the server. The data is provided as a vector of
 * bytes representing the received information.
 *
 * @return A vector of bytes containing the data received from the server.
 *         If no data is available, the returned vector will be empty.
 */
std::vector<uint8_t> NetworkSubsystem::ReceiveFromServer()
{
    std::vector<uint8_t> out(m_incomingDataForMe.begin(), m_incomingDataForMe.end());
    m_incomingDataForMe.clear();
    return out;
}

/**
 * Checks if the client at the specified index has incoming data in the queue.
 *
 * @param idx The index of the client to check within the connection list.
 * @return True if the specified client index is within bounds and has incoming
 *         data; otherwise, false.
 */
bool NetworkSubsystem::HasClientData(size_t idx) const
{
    return idx < m_connections.size() && !m_connections[idx].incoming.empty();
}

/**
 * Retrieves the incoming data from a specific client connection identified by the index.
 * This method fetches all the received data for the given connection, clears the internal
 * buffer for that connection, and returns the data as a vector of bytes.
 *
 * @param idx The index of the client connection from which to receive data. This index
 *            corresponds to the position of the client in the internal connections list.
 * @return A vector of bytes containing the data received from the specified client connection.
 *         If the index is invalid or out of range, an empty vector is returned.
 */
std::vector<uint8_t> NetworkSubsystem::ReceiveFromClient(size_t idx)
{
    std::vector<uint8_t> result;
    if (idx < m_connections.size())
    {
        auto& incoming = m_connections[idx].incoming;
        result.assign(incoming.begin(), incoming.end());
        incoming.clear();
    }
    return result;
}
