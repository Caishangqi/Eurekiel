#pragma once
#include "NetworkCommon.hpp"    // Common Header


/**
 * @class NetworkSubsystem
 *
 * @brief Manages network operations for both client and server configurations.
 *
 * This class provides the functionality to handle network communication,
 * including initializing and cleaning up network-related resources, starting
 * and stopping servers and clients, sending and receiving data, and
 * maintaining client and server states.
 */
class NetworkSubsystem
{
public:
    NetworkSubsystem(NetworkConfig& config);
    ~NetworkSubsystem();

public:
    void Startup(); // Initialize WinSock
    void Shutdown(); // Close all connections and clean up WinSock
    void Update();

public: /// Server side API
    bool StartServer(uint16_t port); // Start the server to listen on the specified serverPort
    void StopServer(); // Stop the server

    // Server States
    ServerState GetServerState() const { return m_serverState; }
    size_t      GetConnectedClientCount() const { return m_connections.size(); }

    // Server Data handling
    void                 BroadcastToClients(const std::vector<uint8_t>& data); // The server broadcasts to all clients
    void                 SendToClient(size_t clientIndex, const std::vector<uint8_t>& data); // The server sends a message to the specified client
    bool                 HasDataFromClient(size_t clientIndex) const; // Is there any new data from the specified client?
    std::vector<uint8_t> ReceiveFromClient(size_t clientIndex); // Get all data from the specified client
    void                 BroadcastStringToClients(const std::string& message);
    void                 SendStringToClient(size_t clientIndex, const std::string& message);

public: /// Client side API
    bool StartClient(const std::string& serverIp, uint16_t port); // Connect to the remote server
    void DisconnectClient(); // Disconnect the client

    // Client Status
    ClientState GetClientState() const { return m_clientState; }

    // Client Data handling
    void                 SendToServer(const std::vector<uint8_t>& data); // The client sends data to the server
    bool                 HasDataFromServer() const;
    std::vector<uint8_t> ReceiveFromServer();
    bool                 ClearReceivedData(); // Refresh the client data
    void                 SendStringToServer(const std::string& message);

public:
    // Configuration
    NetworkConfig&      GetConfig() { return m_config; }
    void                SetSendMode(SendMode mode) { m_config.sendMode = mode; }
    SendMode            GetSendMode() const { return m_config.sendMode; }
    void                SetMessageBoundaryMode(MessageBoundaryMode mode) { m_config.boundaryMode = mode; }
    MessageBoundaryMode GetMessageBoundaryMode() const { return m_config.boundaryMode; }

    // Performance Matrices
    NetworkStats GetNetworkStatistics() const;
    void         ResetStatistics();

    // Queue States
    size_t GetOutgoingQueueSize() const { return m_outgoingDataForMe.size(); }
    size_t GetIncomingQueueSize() const { return m_incomingDataForMe.size(); }
    void   ClearAllQueues();

private:
    NetworkConfig m_config;

    // Statistic Information
    mutable NetworkStats                           m_stats;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;

    // Client variable
    [[maybe_unused]] uint64_t m_clientSocket = 0;
    ClientState               m_clientState  = ClientState::UNINITIALIZED;
    std::deque<unsigned char> m_incomingDataForMe; // game code does pop front to receive
    std::deque<unsigned char> m_outgoingDataForMe; // game code does push back to send

    // Sever variable
    [[maybe_unused]] unsigned int  m_serverListenSocket = 0;
    ServerState                    m_serverState        = ServerState::UNINITIALIZED;
    std::vector<NetworkConnection> m_connections;

private:
    // Choose sending strategy based on send mode
    bool ProcessOutgoingData(uint64_t socket, std::deque<uint8_t>& outgoingQueue);

    // Blocking send: attempt to send all data
    bool SendAllDataBlocking(uint64_t socket, std::deque<uint8_t>& outgoingQueue);

    // Non-blocking send: limit send amount per frame
    bool SendDataNonBlocking(uint64_t socket, std::deque<uint8_t>& outgoingQueue);

    // Receive data
    bool ProcessIncomingData(uint64_t socket, std::deque<uint8_t>& incomingQueue);

    // Performance control
    bool ShouldStopNetworkProcessing() const;
    void UpdateFrameStatistics();

    // Message boundary handling
    void AppendMessageWithBoundary(std::deque<uint8_t>& queue, const std::string& message);

    // Safety checks
    bool IsMessageSafe(const std::string& message) const;
    bool IsQueueSizeOk(const std::deque<uint8_t>& queue) const;
};

