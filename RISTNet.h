//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-14.
//

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#ifndef CPPRISTWRAPPER__RISTNET_H
#define CPPRISTWRAPPER__RISTNET_H

#define CPP_WRAPPER_VERSION 21

#include "librist.h"
#include "version.h"
#include <string.h>
#include <any>
#include <tuple>
#include <vector>
#include <sstream>
#include <memory>
#include <atomic>
#include <map>
#include <functional>
#include <mutex>

#ifdef WIN32
#include <Winsock2.h>
#define _WINSOCKAPI_
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#endif



/**
 * \class RISTNetTools
 *
 * \brief
 *
 * A static helper class for the RIST C++ wrapper
 *
 */
class RISTNetTools {
public:
    /// Build the librist url based on name/ip, port and if it's a listen or not peer
    static bool buildRISTURL(const std::string &lIP, const std::string &lPort, std::string &rURL, bool lListen);
private:

    /// This class cannot be instantiated
    RISTNetTools() = default;

    static bool isIPv4(const std::string &rStr);
    static bool isIPv6(const std::string &rStr);
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetReceiver  --  RECEIVER
//
//
//---------------------------------------------------------------------------------------------------------------------

 /**
  * \class RISTNetReceiver
  *
  * \brief
  *
  * A RISTNetReceiver receives data from a sender. The reciever can listen for a sender to connect
  * or connect to a sender that listens.
  *
  */
class RISTNetReceiver {
public:

    /**
     * \class NetworkConnection
     *
     * \brief
     *
     * A NetworkConnection class is the maintainer and carrier of the user class passed to the connection.
     *
     */
    class NetworkConnection {
    public:
        std::any mObject = nullptr; //Contains your object
    };


    struct RISTNetReceiverSettings {
      RISTNetReceiverSettings() {
          mPeerConfig.version = RIST_PEER_CONFIG_VERSION;
          mPeerConfig.virt_dst_port = RIST_DEFAULT_VIRT_DST_PORT;
          mPeerConfig.recovery_mode = RIST_DEFAULT_RECOVERY_MODE;
          mPeerConfig.recovery_maxbitrate = RIST_DEFAULT_RECOVERY_MAXBITRATE;
          mPeerConfig.recovery_maxbitrate_return = RIST_DEFAULT_RECOVERY_MAXBITRATE_RETURN;
          mPeerConfig.recovery_length_min = RIST_DEFAULT_RECOVERY_LENGTH_MIN;
          mPeerConfig.recovery_length_max = RIST_DEFAULT_RECOVERY_LENGTH_MAX;
          mPeerConfig.recovery_reorder_buffer = RIST_DEFAULT_RECOVERY_REORDER_BUFFER;
          mPeerConfig.recovery_rtt_min = RIST_DEFAULT_RECOVERY_RTT_MIN;
          mPeerConfig.recovery_rtt_max = RIST_DEFAULT_RECOVERY_RTT_MAX;
          mPeerConfig.congestion_control_mode = RIST_DEFAULT_CONGESTION_CONTROL_MODE;
          mPeerConfig.min_retries = RIST_DEFAULT_MIN_RETRIES;
          mPeerConfig.max_retries = RIST_DEFAULT_MAX_RETRIES;
          mPeerConfig.weight = 5;
          mPeerConfig.session_timeout = RIST_DEFAULT_SESSION_TIMEOUT;
      }
    rist_profile mProfile = RIST_PROFILE_MAIN;
    rist_peer_config mPeerConfig;

    rist_log_level mLogLevel = RIST_LOG_ERROR;
    std::unique_ptr<rist_logging_settings> mLogSetting;
    std::string mPSK;
    std::string mCNAME;
    int mSessionTimeout = 5000;
    int mKeepAliveInterval = 10000;
    int mMaxjitter = 0;

  };

  /// Constructor
  RISTNetReceiver();

  /// Destructor
  virtual ~RISTNetReceiver();

  /**
   * @brief Initialize receiver
   *
   * Initialize the receiver using the provided settings and parameters.
   *
   * @param rURLList is a vector of RIST formated URL's
   * @param The receiver settings
   * @return true on success
   */
  bool initReceiver(std::vector<std::string> &rURLList,
                    RISTNetReceiverSettings &rSettings);

  /**
   * @brief Map of all active connections
   *
   * Get a map of all connected clients
   *
   * @param function getting the map of active clients (normally a lambda).
   */
  void getActiveClients(std::function<void(std::map<rist_peer *, std::shared_ptr<NetworkConnection>> &)> function);

  /**
   * @brief Close a client connection
   *
   * Closes a client connection.
   *
   */
  bool closeClientConnection(rist_peer *);


  /**
   * @brief Close all active connections
   *
   * Closes all active connections.
   *
   */
  void closeAllClientConnections();

  /**
   * @brief Send OOB data (Currently not working in librist)
   *
   * Sends OOB data to the specified peer
   * OOB data is encrypted (if used) but not protected for network loss
   *
   * @param target peer
   * @param pointer to the data
   * @param length of the data
   *
   */
  bool sendOOBData(rist_peer *pPeer, const uint8_t *pData, size_t lSize);

  /**
   * @brief Destroys the receiver
   *
   * Destroys the receiver and garbage collects all underlying assets.
   *
   */
  bool destroyReceiver();

  /**
   * @brief Gets the version
   *
   * Gets the version of the C++ librist wrapper sender and librist
   *
   * @return The cpp wrapper version, rist major and minor version.
   */
  static void getVersion(uint32_t &rCppWrapper, uint32_t &rRistMajor, uint32_t &rRistMinor);

  /**
   * @brief Data receive callback
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there. The sender can also put a optional uint16_t value (not 0) associated with the data
   *
   * @param function getting data from the sender.
   * @return 0 to keep the connection else -1.
   */
  std::function<int(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection, rist_peer *pPeer, uint16_t lConnectionID)>
      networkDataCallback = nullptr;

  /**
   * @brief OOB Data receive callback (__NULLABLE)
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there.
   *
   * @param function getting data from the sender.
   */
  std::function<void(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection, rist_peer *pPeer)>
      networkOOBDataCallback = nullptr;

  /**
   * @brief Validate connection callback
   *
   * If the reciever is in listen mode and a sender connects this method is called
   * Return a NetworkConnection if you want to accept this connection
   * You can attach any object to the NetworkConnection and the NetworkConnection object
   * will manage your objects lifecycle. Meaning it will release it when the connection
   * is terminated.
   *
   * @param function validating the connection.
   * @return a NetworkConnection object or nullptr for rejecting.
   */
  std::function<std::shared_ptr<NetworkConnection>(std::string lIPAddress, uint16_t lPort)>
      validateConnectionCallback = nullptr;

  /// Callback handling disconnecting clients
  std::function<void(const std::shared_ptr<NetworkConnection>&, const rist_peer&)> clientDisconnectedCallback = nullptr;

  /// Callback for statistics, called once every second
  std::function<void(const rist_stats& statistics)> statisticsCallback = nullptr;

  // Delete copy and move constructors and assign operators
  RISTNetReceiver(RISTNetReceiver const &) = delete;             // Copy construct
  RISTNetReceiver(RISTNetReceiver &&) = delete;                  // Move construct
  RISTNetReceiver &operator=(RISTNetReceiver const &) = delete;  // Copy assign
  RISTNetReceiver &operator=(RISTNetReceiver &&) = delete;       // Move assign

private:

  std::shared_ptr<NetworkConnection> validateConnectionStub(std::string lIPAddress, uint16_t lPort);
  int dataFromClientStub(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection);

  // Private method receiving the data from librist C-API
  static int receiveData(void *pArg, rist_data_block *data_block);

  // Private method receiving OOB data from librist C-API
  static int receiveOOBData(void *pArg, const rist_oob_block *pOOB_block);

  // Private method called when a client connects
  static int clientConnect(void *pArg, const char* pConnectingIP, uint16_t lConnectingPort, const char* pIP, uint16_t lPort, rist_peer *pPeer);

  // Private method called when a client disconnects
  static int clientDisconnect(void *pArg, rist_peer *pPeer);

  // Private method called when a statistics are delivered
  static int gotStatistics(void *pArg, const rist_stats *stats);

  // The context of a RIST receiver
  rist_ctx *mRistContext = nullptr;

  // The configuration of the RIST receiver
  rist_peer_config mRistPeerConfig{};

  // The mutex protecting the list. since the list can be accessed from both librist and the C++ layer
  std::mutex mClientListMtx;

  // The list of connected clients
  std::map<rist_peer *, std::shared_ptr<NetworkConnection>> mClientListReceiver;

  std::unique_ptr<rist_logging_settings, decltype(&free)> mLoggingScope{nullptr, &free};

};

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetSender  --  SENDER
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class RISTNetSender
 *
 * \brief
 *
 * A RISTNetSender sends data to a receiver. The sender can listen for a receiver to connect
 * or connect to a receiver that listens.
 *
 */
class RISTNetSender {
public:

    /**
     * \class NetworkConnection
     *
     * \brief
     *
     * A NetworkConnection class is the maintainer and carrier of the user class passed to the connection.
     *
     */
    class NetworkConnection {
    public:
        std::any mObject = nullptr; //Contains your object
    };

  struct RISTNetSenderSettings {
      RISTNetSenderSettings() {
          mPeerConfig.version = RIST_PEER_CONFIG_VERSION;
          mPeerConfig.virt_dst_port = RIST_DEFAULT_VIRT_DST_PORT;
          mPeerConfig.recovery_mode = RIST_DEFAULT_RECOVERY_MODE;
          mPeerConfig.recovery_maxbitrate = RIST_DEFAULT_RECOVERY_MAXBITRATE;
          mPeerConfig.recovery_maxbitrate_return = RIST_DEFAULT_RECOVERY_MAXBITRATE_RETURN;
          mPeerConfig.recovery_length_min = RIST_DEFAULT_RECOVERY_LENGTH_MIN;
          mPeerConfig.recovery_length_max = RIST_DEFAULT_RECOVERY_LENGTH_MAX;
          mPeerConfig.recovery_reorder_buffer = RIST_DEFAULT_RECOVERY_REORDER_BUFFER;
          mPeerConfig.recovery_rtt_min = RIST_DEFAULT_RECOVERY_RTT_MIN;
          mPeerConfig.recovery_rtt_max = RIST_DEFAULT_RECOVERY_RTT_MAX;
          mPeerConfig.congestion_control_mode = RIST_DEFAULT_CONGESTION_CONTROL_MODE;
          mPeerConfig.min_retries = RIST_DEFAULT_MIN_RETRIES;
          mPeerConfig.max_retries = RIST_DEFAULT_MAX_RETRIES;
          mPeerConfig.weight = 5;
          mPeerConfig.session_timeout = RIST_DEFAULT_SESSION_TIMEOUT;
      };
    rist_profile mProfile = RIST_PROFILE_MAIN;
    rist_peer_config mPeerConfig;

    rist_log_level mLogLevel = RIST_LOG_ERROR;
    std::unique_ptr<rist_logging_settings> mLogSetting;
    std::string mPSK;
    std::string mCNAME;
    uint32_t mSessionTimeout = 5000;
    uint32_t mKeepAliveInterval = 10000;
    int mMaxJitter = 0;
   };

  /// Constructor
  RISTNetSender();

  /// Destructor
  virtual ~RISTNetSender();

  /**
   * @brief Initialize sender
   *
   * Initialize the sender using the provided settings and parameters.
   *
   * @param rPeerList is a vector of RIST formated URLs and weights.
   *        The weight is used for load balancing when using several interfaces. Transport is divided
   *        between the interfaces in proportion to its weight. Use 0 to to duplicate all transport on
   *        all interfaces.
   *        Note that sending is skipping the first packet if weight=0,
   *        see https://code.videolan.org/rist/librist/-/issues/135
   * @param The sender settings
   * @return true on success
   */
  bool initSender(std::vector<std::tuple<std::string, int>> &rPeerList,
                  RISTNetSenderSettings &rSettings);

  /**
   * @brief Map of all active connections
   *
   * Get a map of all connected clients
   *
   * @param function getting the map of active clients (normally a lambda).
   */
  void getActiveClients(const std::function<void(std::map<rist_peer *, std::shared_ptr<NetworkConnection>> &)> function);

  /**
   * @brief Close a client connection
   *
   * Closes a client connection.
   *
   */
  bool closeClientConnection(rist_peer *);

  /**
   * @brief Close all active connections
   *
   * Closes all active connections.
   *
   */
  void closeAllClientConnections();

  /**
   * @brief Send data
   *
   * Sends data to the connected peers
   *
   * @param pointer to the data
   * @param length of the data
   * @param a optional uint16_t value sent to the receiver
   *
   */
  bool sendData(const uint8_t *pData, size_t lSize, uint16_t lConnectionID=0, uint16_t streamid=NULL, uint64_t ts_ntp=NULL, uint64_t seq=NULL);

  /**
  * @brief Send OOB data (Currently not working in librist)
  *
  * Sends OOB data to the specified peer
  * OOB data is encrypted (if used) but not protected for network loss
  *
  * @param target peer
  * @param pointer to the data
  * @param length of the data
  *
  */
  bool sendOOBData(rist_peer *pPeer, const uint8_t *pData, size_t lSize);

  /**
   * @brief Destroys the sender
   *
   * Destroys the sender and garbage collects all underlying assets.
   *
   */
  bool destroySender();

  /**
   * @brief Gets the version
   *
   * Gets the version of the C++ librist wrapper sender and librist
   *
   * @return The cpp wrapper version, rist major and minor version.
   */
   static void getVersion(uint32_t &rCppWrapper, uint32_t &rRistMajor, uint32_t &rRistMinor);

  /**
   * @brief OOB Data receive callback (__NULLABLE)
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there.
   *
   * @param function getting data from the sender.
   */
  std::function<void(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection, rist_peer *pPeer)>
      networkOOBDataCallback = nullptr;

  /**
   * @brief Validate connection callback
   *
   * If the sender is in listen mode and a receiver connects this method is called
   * Return a NetworkConnection if you want to accept this connection
   * You can attach any object to the NetworkConnection and the NetworkConnection object
   * will manage your objects lifecycle. Meaning it will release it when the connection
   * is terminated.
   *
   * @param function validating the connection.
   * @return a NetworkConnection object or nullptr for rejecting.
   */
  std::function<std::shared_ptr<NetworkConnection>(std::string lIPAddress, uint16_t lPort)>
      validateConnectionCallback = nullptr;

  /// Callback handling disconnecting clients
  std::function<void(const std::shared_ptr<NetworkConnection>&, const rist_peer&)> clientDisconnectedCallback = nullptr;

  /// Callback for statistics, called once every second
  std::function<void(const rist_stats& statistics)> statisticsCallback = nullptr;

  // Delete copy and move constructors and assign operators
  RISTNetSender(RISTNetSender const &) = delete;             // Copy construct
  RISTNetSender(RISTNetSender &&) = delete;                  // Move construct
  RISTNetSender &operator=(RISTNetSender const &) = delete;  // Copy assign
  RISTNetSender &operator=(RISTNetSender &&) = delete;       // Move assign

private:

  std::shared_ptr<NetworkConnection> validateConnectionStub(const std::string &ipAddress, uint16_t port);
  void dataFromClientStub(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection);

  // Private method receiving OOB data from librist C-API
  static int receiveOOBData(void *pArg, const rist_oob_block *pOOBBlock);

  // Private method called when a client connects
  static int clientConnect(void *pArg, const char* pConnectingIP, uint16_t lConnectingPort, const char* pIP, uint16_t lPort, rist_peer *pPeer);

  // Private method called when a client disconnects
  static int clientDisconnect(void *pArg, rist_peer *pPeer);

  // Private method called when statistics are delivered
  static int gotStatistics(void *pArg, const rist_stats *stats);

  // The context of a RIST sender
  rist_ctx *mRistContext = nullptr;

  // The configuration of the RIST sender
  rist_peer_config mRistPeerConfig{};

  // The mutex protecting the list. since the list can be accessed from both librist and the C++ layer
  std::mutex mClientListMtx;

  // The list of connected clients
  std::map<rist_peer *, std::shared_ptr<NetworkConnection>> mClientListSender;

  std::unique_ptr<rist_logging_settings, decltype(&free)> mLoggingScope{nullptr, &free};

};

#endif //CPPRISTWRAPPER__RISTNET_H
