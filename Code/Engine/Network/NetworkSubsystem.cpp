#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "NetworkSubsystem.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

#include "Engine/Core/ErrorWarningAssert.hpp"

namespace
{
    inline SOCKET   ToSOCKET(uint64_t h) { return static_cast<SOCKET>(h); }
    inline uint64_t FromSOCKET(SOCKET s) { return static_cast<uint64_t>(s); }
}

NetworkSubsystem::NetworkSubsystem(NetworkConfig& config) : m_config(config)
{
    if (!m_config.IsValid())
    {
        ERROR_RECOVERABLE("Invalid NetworkConfig provided!")
        m_config = NetworkConfig{}; // Use default configuration
    }
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
void NetworkSubsystem::Startup()
{
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
 * Starts the server by binding to the specified serverPort and entering the listening state.
 * This prepares the server to accept incoming connections. The server must be in the
 * IDLE state before this method can be called. The function creates a non-blocking
 * TCP socket, binds it to the specified serverPort, and begins listening on it.
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

    // Bind to the specified serverPort
    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to bind to serverPort " << port << ", error: " << WSAGetLastError() << "\n";
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

    std::cout << "Server started listening on serverPort " << port << "\n";
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
 * Initiates a client connection to the specified server IP and serverPort. The method
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
 * Updates the state of the network subsystem, including handling server and
 * client operations. This method processes incoming and outgoing data,
 * manages client connections for both server and client modes, and updates
 * frame-level statistics.
 *
 * Server Operations:
 * - Accepts and processes new client connections when in a listening state.
 * - Handles data transmission (sending and receiving) for each connected client.
 * - Removes disconnected clients from the server's active connections list.
 *
 * Client Operations:
 * - In the connecting state, checks the connection status and transitions to
 *   the connected state upon successful connection.
 * - When connected, processes outgoing and incoming data.
 * - Disconnects the client and cleans up resources upon errors or disconnection.
 *
 * Updates internal statistics for frame processing at the end of the method.
 */
void NetworkSubsystem::Update()
{
    m_frameStartTime = std::chrono::high_resolution_clock::now();

    /// Server logic
    if (m_serverState == ServerState::LISTENING)
    {
        // Accept new connections (this is fast, no need to limit)
        SOCKET listenSocket    = ToSOCKET(m_serverListenSocket);
        SOCKET newClientSocket = accept(listenSocket, nullptr, nullptr);

        if (newClientSocket != INVALID_SOCKET)
        {
            u_long mode = 1;
            if (ioctlsocket(newClientSocket, FIONBIO, &mode) == 0)
            {
                NetworkConnection conn;
                conn.socketHandle = FromSOCKET(newClientSocket);
                conn.state        = ClientState::CONNECTED;
                m_connections.push_back(std::move(conn));
                std::cout << "Server accepted new client connection. Total clients: " << m_connections.size() << "\n";
            }
            else
            {
                closesocket(newClientSocket);
            }
        }

        // Process each client
        for (auto it = m_connections.begin(); it != m_connections.end();)
        {
            SOCKET clientSocket = ToSOCKET(it->socketHandle);
            bool   shouldRemove = false;

            // Send data (select strategy according to mode)
            if (!ProcessOutgoingData(clientSocket, it->outgoing))
            {
                shouldRemove = true;
            }

            // Receive data
            if (!shouldRemove && !ProcessIncomingData(clientSocket, it->incoming))
            {
                shouldRemove = true;
            }

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
        // Check the connection status (keep the original logic)
        SOCKET clientSocket = ToSOCKET(m_clientSocket);
        fd_set writeSet, exceptSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(clientSocket, &writeSet);
        FD_SET(clientSocket, &exceptSet);

        timeval timeout      = {0, 0};
        int     selectResult = select(0, nullptr, &writeSet, &exceptSet, &timeout);

        if (selectResult > 0)
        {
            if (FD_ISSET(clientSocket, &exceptSet))
            {
                std::cerr << "Client connection failed\n";
                DisconnectClient();
            }
            else if (FD_ISSET(clientSocket, &writeSet))
            {
                m_clientState = ClientState::CONNECTED;
                std::cout << "Client connected successfully\n";
            }
        }
        else if (selectResult == SOCKET_ERROR)
        {
            std::cerr << "Select error: " << WSAGetLastError() << "\n";
            DisconnectClient();
        }
    }
    else if (m_clientState == ClientState::CONNECTED)
    {
        SOCKET clientSocket = ToSOCKET(m_clientSocket);

        // Send data (select strategy according to mode)
        if (!ProcessOutgoingData(clientSocket, m_outgoingDataForMe))
        {
            DisconnectClient();
            return;
        }

        // Receive data
        if (!ProcessIncomingData(clientSocket, m_incomingDataForMe))
        {
            DisconnectClient();
        }
    }

    // Update statistics
    UpdateFrameStatistics();
}

/**
 * Processes outgoing data for the given socket using the configured send mode.
 * Depending on the send mode, the method uses either a blocking, non-blocking,
 * or adaptive strategy to transmit data from the outgoing queue.
 *
 * @param socket The socket handle corresponding to the connection for which
 *               outgoing data needs to be transmitted.
 * @param outgoingQueue A reference to a deque containing the outgoing data
 *                      to be sent over the network.
 * @return Returns true if the data transmission is successful; otherwise,
 *         returns false if there is an error during the transmission process.
 */
bool NetworkSubsystem::ProcessOutgoingData(uint64_t socket, std::deque<uint8_t>& outgoingQueue)
{
    switch (m_config.sendMode)
    {
    case SendMode::BLOCKING:
        return SendAllDataBlocking(socket, outgoingQueue);

    case SendMode::NON_BLOCKING:
        return SendDataNonBlocking(socket, outgoingQueue);

    case SendMode::ADAPTIVE:
        // TODO: Automatically select based on network conditions
        // Currently fall back to non-blocking mode
        return SendDataNonBlocking(socket, outgoingQueue);

    default:
        return SendDataNonBlocking(socket, outgoingQueue);
    }
}

/**
 * Sends all data from the specified outgoing queue to the given socket in blocking mode.
 * This method attempts to send data until the entire outgoing queue is empty or an
 * error occurs. If the socket's send buffer is full, the method exits instead of blocking indefinitely.
 *
 * @param socket The socket to which data will be sent.
 * @param outgoingQueue Reference to a deque containing the data to be sent.
 *                      Data will be removed from this queue as it is sent successfully.
 * @return True if all data in the queue is successfully sent or partially sent
 *         without encountering unrecoverable errors. False if an unrecoverable
 *         error occurs during the sending process.
 */
bool NetworkSubsystem::SendAllDataBlocking(uint64_t socket, std::deque<uint8_t>& outgoingQueue)
{
#undef min
    // Blocking mode: try to send all data
    while (!outgoingQueue.empty())
    {
        size_t            bytesToSend = std::min(outgoingQueue.size(), m_config.cachedBufferSize);
        std::vector<char> sendBuffer(outgoingQueue.begin(), outgoingQueue.begin() + (unsigned long long)bytesToSend);

        int sent = send(socket, sendBuffer.data(), static_cast<int>(bytesToSend), 0);
        m_stats.sendAttemptsThisFrame++;

        if (sent > 0)
        {
            outgoingQueue.erase(outgoingQueue.begin(), outgoingQueue.begin() + sent);
            m_stats.bytesSentThisFrame += sent;
            m_stats.totalBytesSent += sent;

            // If it is not completely sent, it means the buffer is full
            if (sent < static_cast<int>(bytesToSend))
            {
                break; // Even in blocking mode, the game cannot be blocked
            }
        }
        else if (sent == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                // The send buffer is full, we also need to exit in blocking mode to avoid real blocking
                break;
            }
            else
            {
                std::cerr << "Send error: " << error << "\n";
                return false;
            }
        }
    }

    return true;
}

/**
 * Sends data in a non-blocking mode over the specified socket. This function attempts
 * to send queued data in small batches while adhering to configured performance limits.
 * The function will send as much data as possible within these limits without blocking.
 *
 * @param socket The socket identifier to which the data will be sent.
 * @param outgoingQueue The queue of bytes representing the data to be sent.
 *                      Bytes will be removed from this queue as they are successfully sent.
 *
 * @return True if the sending process completes successfully or no critical errors occur,
 *         false if a critical error is encountered during sending.
 */
bool NetworkSubsystem::SendDataNonBlocking(uint64_t socket, std::deque<uint8_t>& outgoingQueue)
{
    // Non-blocking mode: limit the amount of frames sent per frame
    size_t sendAttempts = 0;

    while (!outgoingQueue.empty() &&
        sendAttempts < m_config.performanceLimits.maxSendAttemptsPerFrame &&
        m_stats.bytesSentThisFrame < m_config.performanceLimits.maxSendBytesPerFrame &&
        !ShouldStopNetworkProcessing())
    {
        size_t bytesToSend = std::min({
            outgoingQueue.size(),
            m_config.performanceLimits.sendBatchSize,
            m_config.performanceLimits.maxSendBytesPerFrame - m_stats.bytesSentThisFrame
        });

        std::vector<char> sendBuffer(outgoingQueue.begin(), outgoingQueue.begin() + bytesToSend);

        int sent = send(socket, sendBuffer.data(), static_cast<int>(bytesToSend), 0);
        sendAttempts++;
        m_stats.sendAttemptsThisFrame++;

        if (sent > 0)
        {
            outgoingQueue.erase(outgoingQueue.begin(), outgoingQueue.begin() + sent);
            m_stats.bytesSentThisFrame += sent;
            m_stats.totalBytesSent += sent;

            if (sent < static_cast<int>(bytesToSend))
            {
                break; // Send buffer full
            }
        }
        else if (sent == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                break; // Send buffer full
            }
            else
            {
                std::cerr << "Send error: " << error << "\n";
                return false;
            }
        }
    }

    return true;
}

/**
 * Processes incoming data from a specific socket and appends it to the provided
 * incoming data queue. The method handles scenarios such as successfully
 * received data, socket closure, and errors during the receiving process.
 *
 * @param socket The identifier for the socket from which to receive incoming data.
 * @param incomingQueue Reference to a queue where the received data will be appended.
 *                      Each byte of the received data is added to the queue in the
 *                      order it was received.
 * @return Returns true if the data was processed successfully or the operation
 *         should continue (e.g., would-block errors). Returns false if the connection
 *         was closed or a fatal error occurred.
 */
bool NetworkSubsystem::ProcessIncomingData(uint64_t socket, std::deque<uint8_t>& incomingQueue)
{
    char recvBuffer[2048];
    int  received = recv(socket, recvBuffer, sizeof(recvBuffer), 0);

    if (received > 0)
    {
        for (int i = 0; i < received; ++i)
        {
            incomingQueue.push_back(static_cast<uint8_t>(recvBuffer[i]));
        }
        m_stats.bytesReceivedThisFrame += received;
        m_stats.totalBytesReceived += received;
    }
    else if (received == 0)
    {
        return false; // Connection closed
    }
    else // SOCKET_ERROR
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            std::cerr << "Recv error: " << error << "\n";
            return false;
        }
    }

    return true;
}

/**
 * Appends a message to a given queue with a message boundary, based on the
 * currently configured boundary mode. The message can be appended using
 * NULL-terminated boundaries, raw bytes, or a length-prefixed approach
 * (if implemented).
 *
 * @param queue A reference to a deque of uint8_t where the message and its
 *              boundary will be appended.
 * @param message The message to be appended to the queue, represented as a
 *                standard string.
 */
void NetworkSubsystem::AppendMessageWithBoundary(std::deque<uint8_t>& queue, const std::string& message)
{
    switch (m_config.boundaryMode)
    {
    case MessageBoundaryMode::NULL_TERMINATED:
        // Add message content
        for (char c : message)
        {
            queue.push_back(static_cast<uint8_t>(c));
        }
        // Add separator
        queue.push_back(static_cast<uint8_t>(m_config.messageDelimiter));
        break;

    case MessageBoundaryMode::RAW_BYTES:
        // Add directly without processing the border
        for (char c : message)
        {
            queue.push_back(static_cast<uint8_t>(c));
        }
        break;

    case MessageBoundaryMode::LENGTH_PREFIXED:
        // TODO: length prefix mode
        // Currently falls back to NULL_TERMINATED
        AppendMessageWithBoundary(queue, message);
        break;
    }
}

/**
 * Determines if the provided message complies with the configured safety limits
 * to ensure it is safe for transmission. This includes checks for maximum
 * message size and the presence of invalid delimiters that may disrupt message
 * boundaries.
 *
 * @param message The message string to be validated for safety.
 * @return True if the message passes all safety checks, false otherwise.
 */
bool NetworkSubsystem::IsMessageSafe(const std::string& message) const
{
    if (!m_config.safetyLimits.enableSafetyChecks)
        return true;

    // Check message size
    if (message.size() > m_config.safetyLimits.maxMessageSize)
    {
        return false;
    }

    // Check if it contains delimiters (will break message boundaries)
    if (m_config.boundaryMode == MessageBoundaryMode::NULL_TERMINATED &&
        message.find(m_config.messageDelimiter) != std::string::npos)
    {
        return false;
    }

    return true;
}

/**
 * Checks if the size of the specified queue is within the permissible limits,
 * based on the safety checks and configuration.
 *
 * @param queue Reference to a deque of uint8_t representing the queue whose size
 *              needs to be validated.
 *
 * @return True if the queue size is within the permissible limit or if safety
 *         checks are disabled. False otherwise.
 */
bool NetworkSubsystem::IsQueueSizeOk(const std::deque<uint8_t>& queue) const
{
    if (!m_config.safetyLimits.enableSafetyChecks)
        return true;

    return queue.size() < m_config.safetyLimits.maxQueueSize;
}

/**
 * Determines whether network processing should stop in the current frame based
 * on the elapsed time and the current network configuration. This check is only
 * relevant in non-blocking send mode.
 *
 * @return true if the elapsed time since the start of the frame exceeds the
 *         maximum allowed network time per frame as specified in the configuration;
 *         false otherwise.
 */
bool NetworkSubsystem::ShouldStopNetworkProcessing() const
{
    if (m_config.sendMode != SendMode::NON_BLOCKING)
        return false; // Only check the time limit in non-blocking mode

    auto now     = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_frameStartTime).count();
    return elapsed >= m_config.performanceLimits.maxNetworkTimePerFrame;
}

/**
 * Updates the current frame's network statistics, including timings, queue sizes,
 * active connections, and performance limits. This method calculates the elapsed
 * network time for the current frame and updates various counters that track the
 * state and limitations of the network subsystem.
 *
 * The method checks for potential performance limitations based on pre-configured
 * thresholds, such as the maximum number of send attempts, maximum bytes sent,
 * and the maximum allowable network time per frame.
 *
 * It is typically invoked at the end of a frame processing cycle to capture accurate
 * metrics for the ongoing frame.
 */
void NetworkSubsystem::UpdateFrameStatistics()
{
    auto now                     = std::chrono::high_resolution_clock::now();
    m_stats.networkTimeThisFrame = std::chrono::duration<double>(now - m_frameStartTime).count();

    // Update queue status
    m_stats.outgoingQueueSize = m_outgoingDataForMe.size();
    m_stats.incomingQueueSize = m_incomingDataForMe.size();
    m_stats.activeConnections = m_connections.size();

    // Check if performance is limited
    m_stats.isNetworkLimited = (
        m_stats.sendAttemptsThisFrame >= m_config.performanceLimits.maxSendAttemptsPerFrame ||
        m_stats.bytesSentThisFrame >= m_config.performanceLimits.maxSendBytesPerFrame ||
        m_stats.networkTimeThisFrame >= m_config.performanceLimits.maxNetworkTimePerFrame
    );
}

/**
 * Retrieves and returns the current network statistics for the subsystem. This
 * includes details such as the number of active connections, the size of the
 * outgoing and incoming data queues, and other performance metrics.
 *
 * @return A NetworkStats object containing the current metrics and states of
 *         the network subsystem.
 */
NetworkStats NetworkSubsystem::GetNetworkStatistics() const
{
    NetworkStats stats = m_stats;

    // Update real-time queue information
    stats.outgoingQueueSize = m_outgoingDataForMe.size();
    stats.incomingQueueSize = m_incomingDataForMe.size();
    stats.activeConnections = m_connections.size();

    return stats;
}

void NetworkSubsystem::ResetStatistics()
{
    m_stats = NetworkStats{};
}

/**
 * Clears all data queues within the network subsystem to reset the state of
 * outgoing and incoming data buffers. This includes clearing the shared buffers
 * as well as all individual connection-specific queues.
 */
void NetworkSubsystem::ClearAllQueues()
{
    m_outgoingDataForMe.clear();
    m_incomingDataForMe.clear();

    for (auto& connection : m_connections)
    {
        connection.incoming.clear();
        connection.outgoing.clear();
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
bool NetworkSubsystem::HasDataFromServer() const
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
    std::vector<uint8_t> data(m_incomingDataForMe.begin(), m_incomingDataForMe.end());
    m_incomingDataForMe.clear();
    return data;
}

/**
 * Checks if the client at the specified index has incoming data in the queue.
 *
 * @param clientIndex The index of the client to check within the connection list.
 * @return True if the specified client index is within bounds and has incoming
 *         data; otherwise, false.
 */
bool NetworkSubsystem::HasDataFromClient(size_t clientIndex) const
{
    return clientIndex < m_connections.size() && !m_connections[clientIndex].incoming.empty();
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

/**
 * Clears the internal buffer of received data intended for the subsystem
 * and checks whether the buffer is empty after the operation.
 *
 * @return True if the buffer is successfully cleared and is empty, false otherwise.
 */
bool NetworkSubsystem::ClearReceivedData()
{
    m_incomingDataForMe.clear();
    return m_incomingDataForMe.empty();
}

/**
 * Sends a string message to the server after performing safety checks.
 * The message is appended with a boundary marker before being sent.
 * Updates statistics about the total number of messages sent.
 *
 * @param message The string message to be sent to the server. The method will
 *                ensure the message passes safety checks before taking any action.
 */
void NetworkSubsystem::SendStringToServer(const std::string& message)
{
    if (!IsMessageSafe(message))
    {
        std::cerr << "Message failed safety check: " << message.substr(0, 50) << "..." << std::endl;
        return;
    }

    AppendMessageWithBoundary(m_outgoingDataForMe, message);
    m_stats.totalMessagesSent++;
}

/**
 * Sends a string message to the specified client by appending it to the client's
 * outgoing message queue. The message is first validated for safety before being sent.
 * If the client index is invalid or the message fails the safety check, the operation
 * is aborted.
 *
 * @param clientIndex The index of the client in the connection list to whom the
 *                    message should be sent.
 * @param message The message string to be sent to the specified client.
 */
void NetworkSubsystem::SendStringToClient(size_t clientIndex, const std::string& message)
{
    if (clientIndex >= m_connections.size()) return;

    if (!IsMessageSafe(message))
    {
        std::cerr << "Message failed safety check for client " << clientIndex << std::endl;
        return;
    }

    AppendMessageWithBoundary(m_connections[clientIndex].outgoing, message);
    m_stats.totalMessagesSent++;
}

/**
 * Broadcasts a given string message to all connected clients in the network subsystem.
 * Before broadcasting, the message undergoes a safety check, and only safe messages
 * are sent to all clients. Increments the total message count after a successful broadcast.
 *
 * @param message The string message to be broadcasted to all connected clients.
 */
void NetworkSubsystem::BroadcastStringToClients(const std::string& message)
{
    if (!IsMessageSafe(message))
    {
        std::cerr << "Message failed safety check for broadcast" << std::endl;
        return;
    }

    for (auto& connection : m_connections)
    {
        AppendMessageWithBoundary(connection.outgoing, message);
    }
    m_stats.totalMessagesSent += m_connections.size();
}
