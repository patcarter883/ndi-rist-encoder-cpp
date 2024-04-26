#include "transport.h"
#include "Url.h"

using std::string;
using homer6::Url;

void Transport::setup_rist_sender() {
 
  RISTNetSender::RISTNetSenderSettings mySendConfiguration;

  std::vector<std::tuple<string, int>> interfaceListSender;

  Url url{ fmt::format("rist://{}", this->config.rist_output_address) };

  for (int i = 0; i < this->config.rist_output_streams; i = i + 1)
  {
      string rist_output_url = fmt::format(
          "rist://"
          "{}:{}?bandwidth={}buffer-min={}&buffer-max={}&rtt-min={}&rtt-max={}&"
          "reorder-buffer={}",
          url.getHost(),
          url.getPort() + (2 * i),
          this->config.rist_output_bandwidth,
          this->config.rist_output_buffer_min,
          this->config.rist_output_buffer_max,
          this->config.rist_output_rtt_min,
          this->config.rist_output_rtt_max,
          this->config.rist_output_reorder_buffer);

      interfaceListSender.push_back(std::tuple<string, int>(rist_output_url, 0));
  }

  mySendConfiguration.mLogLevel = RIST_LOG_DEBUG;
  mySendConfiguration.mProfile = RIST_PROFILE_MAIN;
  mySendConfiguration.mLogSetting.get()->log_cb = this->log_callback;
  this->ristSender.initSender(interfaceListSender, mySendConfiguration);
}

void Transport::send_buffer(uint8_t* buf_data, gsize buf_size)
{
    this->ristSender.sendData(buf_data, buf_size);
}