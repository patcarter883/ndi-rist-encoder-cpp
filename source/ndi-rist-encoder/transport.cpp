#include "common.h"
#include <fmt/core.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <RISTNet.h>
using std::string;

class Transport {

    public:
        Config* config;
        RISTNetSender ristSender;
        std::function<void(const rist_stats& statistics)> statistics_callback = nullptr;
        std::function<int(void* arg, enum rist_log_level, const char* msg)> log_callback = nullptr;
        void setup_rist_sender();
        void send_buffer(BufferDataStruct buffer_data);
        Transport(Config* config) {
            this->config = config;
        }
        ~Transport() {
            this->ristSender.destroySender();
        }

    private:
        
};

void Transport::setup_rist_sender() {
 
  RISTNetSender::RISTNetSenderSettings mySendConfiguration;

  // Generate a vector of RIST URL's,  ip(name), ports, RIST URL output,
  // listen(true) or send mode (false)
  string lURL = fmt::format(
      "rist://"
      "{}?bandwidth={}buffer-min={}&buffer-max={}&rtt-min={}&rtt-max={}&"
      "reorder-buffer={}",
      this->config->rist_output_address,
      this->config->rist_output_bandwidth,
      this->config->rist_output_buffer_min,
      this->config->rist_output_buffer_max,
      this->config->rist_output_rtt_min,
      this->config->rist_output_rtt_max,
      this->config->rist_output_reorder_buffer);
  std::vector<std::tuple<string, int>> interfaceListSender;

  interfaceListSender.push_back(std::tuple<string, int>(lURL, 0));

  this->ristSender.statisticsCallback = this->statistics_callback;

  if (this->config->rist_output_bandwidth != "") {
    int recovery_maxbitrate = std::stoi(config->rist_output_bandwidth);
    if (recovery_maxbitrate > 0) {
      mySendConfiguration.mPeerconfig->recovery_maxbitrate = recovery_maxbitrate;
    }
  }

  mySendConfiguration.mLogLevel = RIST_LOG_INFO;
  mySendConfiguration.mProfile = RIST_PROFILE_MAIN;
  mySendConfiguration.mLogSetting.get()->log_cb = this->log_callback;
  this->ristSender.initSender(interfaceListSender, mySendConfiguration);
}

void Transport::send_buffer(BufferDataStruct buffer_data)
{
    this->ristSender.sendData(buffer_data.buf_data, buffer_data.buf_size);
}