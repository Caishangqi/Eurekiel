#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include <string>

enum class ServerState
{
    // Startup
    UNINITIALIZED,
    // Win socket startup
    IDLE, // Ready to do something, but not actively processing any requests.
    // Start Server
    // Create the socket, listen socket, Set to non-blocking
    // Bind the socket to a port (3100) for listening
    // Start listening for incoming connections
    // Wait until a connection is established (Blocking and Non-Blocking version)
    LISTENING, // 0  clients connected
    // Begin Frame()
    // Accept the connection (m_Listener), if we accept a connection, we return a new socket, Set to non-blocking
    // We push back the new client to the vector
    STOP_LISTENING
    /// When we disconnect the client
    /// We first Disconnect the client by Disconnect(m_client)
    /// We turn the state to @see ServerState::IDLE
    /// Shutdown the subsystem
    /// We then close the socket
    /// Win socket cleanup
    /// turn the state to @see ServerState::UNINITIALIZED
};

enum class ClientState
{
    UNINITIALIZED,
    IDLE, // Ready to do something, but not actively processing any requests.
    // Start Client function
    // We create the client socket, Set to non-blocking
    // Connect(IP, Port) this function have block and unblock version
    /// Try to connect, and put into new state @see ClientState::CONNECTING
    CONNECTING,
    // Begin frame
    /// Select(m_client), if successful, we turn to new state @see ClientState::CONNECTED
    CONNECTED,
};

/**
 * @struct NetworkConfig
 *
 * @brief Holds the configuration settings for the network subsystem.
 *
 * The NetworkConfig structure defines the parameters used to configure
 * server or client connections. This includes the port number and the
 * server IP address for communication.
 */
struct NetworkConfig
{
    uint16_t    port     = 25565; // default port
    std::string serverIp = "127.0.0.1"; // default server IP, current computer
};

/**
 * @struct NetworkConnection
 *
 * @brief Represents a single network connection in the system.
 *
 * The NetworkConnection structure is used to maintain the state and data
 * associated with a specific client or server connection. It stores the raw
 * socket handle for communication, the current connection state, and the data
 * queues for incoming and outgoing messages.
 */
struct NetworkConnection
{
    uint64_t            socketHandle = 0; // raw socket handle
    ClientState         state        = ClientState::UNINITIALIZED;
    std::deque<uint8_t> incoming;
    std::deque<uint8_t> outgoing;
};

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
    NetworkSubsystem();
    ~NetworkSubsystem();

public:
    void Startup(NetworkConfig& config); // Initialize WinSock
    void Shutdown(); // Close all connections and clean up WinSock

    bool StartServer(uint16_t port); // Start the server to listen on the specified port
    void StopServer(); // Stop the server

    bool StartClient(const std::string& serverIp, uint16_t port); // Connect to the remote server
    void DisconnectClient(); // Disconnect the client

    void Update();

    void                 SendToServer(const std::vector<uint8_t>& data); // The client sends data to the server
    void                 BroadcastToClients(const std::vector<uint8_t>& data); // The server broadcasts to all clients
    void                 SendToClient(size_t clientIndex, const std::vector<uint8_t>& data); // The server sends a message to the specified client
    bool                 HasServerData() const;
    std::vector<uint8_t> ReceiveFromServer();

    bool                 HasClientData(size_t clientIndex) const; // Is there any new data from the specified client?
    std::vector<uint8_t> ReceiveFromClient(size_t clientIndex); // Get all data from the specified client

    ClientState GetClientState() const { return m_clientState; }
    ServerState GetServerState() const { return m_serverState; }

private:
    NetworkConfig m_config;

    // Client variable
    [[maybe_unused]] uint64_t m_clientSocket = 0;
    ClientState               m_clientState  = ClientState::UNINITIALIZED;
    std::deque<unsigned char> m_incomingDataForMe; // game code does pop front to receive
    std::deque<unsigned char> m_outgoingDataForMe; // game code does push back to send

    // Sever variable
    [[maybe_unused]] unsigned int  m_serverListenSocket = 0;
    ServerState                    m_serverState        = ServerState::UNINITIALIZED;
    std::vector<NetworkConnection> m_connections;
};
