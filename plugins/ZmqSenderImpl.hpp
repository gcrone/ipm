/**
 *
 * @file ZmqSenderImpl.cpp Implementations of common routines for ZeroMQ Senders
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IPM_PLUGINS_ZMQSENDERIMPL_HPP_
#define IPM_PLUGINS_ZMQSENDERIMPL_HPP_

#include "ipm/Sender.hpp"
#include "ipm/ZmqContext.hpp"

#include "logging/Logging.hpp"
#include "zmq.hpp"

#include <string>

namespace dunedaq {
namespace ipm {
class ZmqSenderImpl : public Sender
{
public:
  enum class SenderType
  {
    Publisher,
    Push,
  };

  explicit ZmqSenderImpl(SenderType type)
    : m_socket(ZmqContext::instance().GetContext(),
               type == SenderType::Push ? zmq::socket_type::push : zmq::socket_type::pub)
  {}
  bool can_send() const noexcept override { return m_socket_connected; }
  void connect_for_sends(const nlohmann::json& connection_info)
  {
    m_connection_string = connection_info.value<std::string>("connection_string", "inproc://default");
    TLOG() << "Connection String is " << m_connection_string;
    m_socket.setsockopt(ZMQ_SNDTIMEO, 1); // 1 ms, we'll repeat until we reach timeout
    m_socket.bind(m_connection_string);
    m_socket_connected = true;
  }

protected:
  void send_(const void* message, int N, const duration_t& timeout, std::string const& topic) override
  {
    TLOG_DEBUG(0) << "Endpoint " << m_connection_string << ": Starting send of " << N << " bytes";
    auto start_time = std::chrono::steady_clock::now();
    bool res = false;
    do {

      zmq::message_t topic_msg(topic.c_str(), topic.size());
      res = m_socket.send(topic_msg, ZMQ_SNDMORE);

      if (!res) {
        TLOG_DEBUG(2) << "Endpoint " << m_connection_string << ": Unable to send message";
        continue;
      }

      zmq::message_t msg(message, N);
      res = m_socket.send(msg);
    } while (std::chrono::steady_clock::now() - start_time < timeout && !res);

    if (!res) {
      throw SendTimeoutExpired(ERS_HERE, timeout.count());
    }

    TLOG_DEBUG(0) << "Endpoint " << m_connection_string << ": Completed send of " << N << " bytes";
  }

private:
  zmq::socket_t m_socket;
  std::string m_connection_string;
  bool m_socket_connected;
};

} // namespace ipm
} // namespace dunedaq

#endif // IPM_PLUGINS_ZMQSENDERIMPL_HPP_
