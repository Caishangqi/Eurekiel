#pragma once
#include <cstdint>
#include <deque>
#include <vector>

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
 * @class NetworkSubsystem
 * @brief Manages networking operations and communications within the system.
 *
 * The NetworkSubsystem class is designed to handle various aspects of a networked application.
 * It provides mechanisms for establishing, maintaining, and terminating network connections,
 * as well as functionalities for data transmission and reception. This class acts as a core
 * component for enabling networking features and protocols.
 *
 * Use this class to integrate networking capabilities within the system.
 * The implementation details are specific to system requirements and depend on underlying
 * platform-specific networking libraries or APIs.
 */
class NetworkSubsystem
{
public:

private:
    // Client variable
    [[maybe_unused]] uint64_t m_clientSocket = 0;
    std::deque<unsigned char> m_incomingDataForMe;  // game code does pop front to receive
    std::deque<unsigned char> m_outgoingDataForMe;  // game code does push back to send

    // Sever variable
    [[maybe_unused]] unsigned int m_serverListenSocket = 0;
    std::vector<uint64_t>         m_externalClientSocket{0};
};
