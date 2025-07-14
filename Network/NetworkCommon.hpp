#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <vector>
#include <string>


/**
 * @enum SendMode
 * @brief Network sending mode configuration
 */
enum class SendMode
{
    BLOCKING, // Blocking send: attempts to send all pending data in one Update
    NON_BLOCKING, // Non-blocking send: sends partial data each Update to avoid framerate impact
    ADAPTIVE // Adaptive send: dynamically adjusts based on network conditions (future extension)
};

/**
 * @enum MessageBoundaryMode
 * @brief Message boundary handling mode
 */
enum class MessageBoundaryMode
{
    NULL_TERMINATED, // Uses \0 as message delimiter
    RAW_BYTES, // Raw byte stream, no message boundary processing
    LENGTH_PREFIXED // Length prefix mode (future extension)
};

enum class ServerState
{
    // Startup
    UNINITIALIZED,
    // Win socket startup
    IDLE, // Ready to do something, but not actively processing any requests.
    // Start Server
    // Create the socket, listen socket, Set to non-blocking
    // Bind the socket to a serverPort (3100) for listening
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
 * server or client connections. This includes the serverPort number and the
 * server IP address for communication.
 */
/**
 * @struct NetworkConfig
 * @brief Network subsystem configuration structure
 */
struct NetworkConfig
{
    // Basic network configuration
    uint16_t    serverPort       = 3100;
    std::string serverIp         = "127.0.0.1";
    int         maxPlayers       = 2;
    std::string motd             = "default Game Server";
    size_t      cachedBufferSize = 2048;

    // Send mode configuration
    SendMode sendMode = SendMode::NON_BLOCKING; // Default non-blocking mode

    // Message boundary configuration
    MessageBoundaryMode boundaryMode     = MessageBoundaryMode::NULL_TERMINATED;
    char                messageDelimiter = '\0';

    // Performance limits configuration (effective only in NON_BLOCKING mode)
    struct PerformanceLimits
    {
        size_t maxSendAttemptsPerFrame = 10; // Maximum send attempts per frame
        size_t maxSendBytesPerFrame    = 4096; // Maximum bytes to send per frame
        double maxNetworkTimePerFrame  = 0.002; // Maximum network processing time per frame (seconds)
        size_t sendBatchSize           = 1024; // Batch send size
    } performanceLimits;

    // Safety limits configuration
    struct SafetyLimits
    {
        size_t maxMessageSize     = 64 * 1024; // 64KB maximum message size
        size_t maxQueueSize       = 1024 * 1024; // 1MB maximum queue size
        bool   enableSafetyChecks = true; // Enable safety checks
    } safetyLimits;

    // 验证配置的有效性
    bool IsValid() const
    {
        return serverPort > 0 &&
            maxPlayers > 0 &&
            cachedBufferSize > 0 &&
            performanceLimits.maxSendAttemptsPerFrame > 0 &&
            performanceLimits.maxSendBytesPerFrame > 0 &&
            performanceLimits.sendBatchSize > 0 &&
            safetyLimits.maxMessageSize > 0 &&
            safetyLimits.maxQueueSize > 0;
    }
};

/**
 * @struct NetworkStats
 * @brief Represents statistical data for network operations.
 *
 * This structure is used for tracking various metrics related to
 * network communication, including data transmission, queue statuses,
 * connection statuses, and current mode settings.
 *
 * The statistics are primarily divided into the following categories:
 * - Basic statistics: Tracks the total data and message counts for transmissions.
 * - Current frame statistics: Tracks data sent/received within the current frame.
 * - Queue status: Monitors the size and state of message queues.
 * - Connection status: Indicates information about active connections and restrictions.
 * - Mode status: Records the current sending and message boundary modes being used.
 */
struct NetworkStats
{
    // Basic statistics
    size_t totalBytesSent        = 0;
    size_t totalBytesReceived    = 0;
    size_t totalMessagesSent     = 0;
    size_t totalMessagesReceived = 0;

    // Current frame statistics
    size_t sendAttemptsThisFrame  = 0;
    size_t bytesSentThisFrame     = 0;
    size_t bytesReceivedThisFrame = 0;
    double networkTimeThisFrame   = 0.0;

    // Queue status
    size_t outgoingQueueSize  = 0;
    size_t incomingQueueSize  = 0;
    size_t incompleteMessages = 0;

    // Connection status
    size_t activeConnections = 0;
    bool   isNetworkLimited  = false;

    // Mode status
    SendMode            currentSendMode     = SendMode::NON_BLOCKING;
    MessageBoundaryMode currentBoundaryMode = MessageBoundaryMode::NULL_TERMINATED;
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

/// Common Functions
std::vector<uint8_t> StringToByte(std::string& str);
