// ndi-rist-encoder->cpp : Defines the entry point for the application.

#include <algorithm>
#include <chrono>
#include <future>
#include <thread>

#include <FL/Fl.H>
#include <fmt/core.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <atomic>
#include <ristsender.h>
#include "Url.h"
#include "UserInterface.cxx"
#include "UserInterface.h"
using homer6::Url;
#include "rpc/client.h"
#include <string>
using std::string;

#include "encode.h"

void startStream();
void stopStream();

void* previewNDISource();
void* findNdiDevices(void* p);
void addNdiDevice(gpointer devicePtr, gpointer p);

void gotRistStatistics(const rist_stats& statistics);
void logAppend(std::string logMessage);
void ristLogAppend(std::string logMessage);
int ristLog(void* arg, enum rist_log_level, const char* msg);

struct RpcData
{
  std::string bitrate;
  std::string rist_output_address;
  std::string rist_output_buffer_min;
  std::string rist_output_buffer_max;
  std::string rist_output_rtt_min;
  std::string rist_output_rtt_max;
  std::string rist_output_reorder_buffer;
  std::string rist_output_bandwidth;
  std::string rtmp_address;
  std::string rtmp_key;
  std::string reencode_bitrate;
  int codec;
  bool upscale;
  MSGPACK_DEFINE_ARRAY(bitrate,
                       rist_output_address,
                       rist_output_buffer_min,
                       rist_output_buffer_max,
                       rist_output_rtt_min,
                       rist_output_rtt_max,
                       rist_output_reorder_buffer,
                       rist_output_bandwidth,
                       rtmp_address,
                       rtmp_key,
                       reencode_bitrate,
                       codec,
                       upscale);
};
struct App
{
  UserInterface* ui = new UserInterface;
  Fl_Text_Buffer* log_buff = new Fl_Text_Buffer;
  Fl_Text_Buffer* rist_log_buff = new Fl_Text_Buffer;

  GstDeviceMonitor* monitor;

  std::atomic_bool is_eos = false;
  std::atomic_bool is_running = false;
  GMainLoop* loop;

  std::atomic_bool is_playing = false;

  uint16_t current_bitrate;
  double previous_quality;
  uint16_t rpc_port = 5999;

  std::future<int> transport_thread_future;
  std::future<void> gstreamer_sink_future;
};

namespace nre
{
/* Globals */
Config config;
App app;
Encode* encoder = nullptr;

}  // namespace nre

using namespace nre;

void initUi()
{
  app.ui->logDisplay->buffer(app.log_buff);
  app.ui->ristLogDisplay->buffer(app.rist_log_buff);
  app.ui->ristAddressInput->value(config.rist_output_address.c_str());
  app.ui->ristBandwidthInput->value(config.rist_output_bandwidth.c_str());
  app.ui->ristBufferMaxInput->value(config.rist_output_buffer_max.c_str());
  app.ui->ristBufferMinInput->value(config.rist_output_buffer_min.c_str());
  app.ui->ristRttMaxInput->value(config.rist_output_rtt_max.c_str());
  app.ui->ristRttMinInput->value(config.rist_output_rtt_min.c_str());
  app.ui->ristReorderBufferInput->value(
      config.rist_output_reorder_buffer.c_str());
  app.ui->encoderBitrateInput->value(config.bitrate.c_str());
  app.ui->useRpcInput->value(config.use_rpc_control);
  app.ui->upscaleInput->value(config.upscale);
  app.ui->reencodeBitrateInput->value(config.reencode_bitrate.c_str());
  app.ui->show(NULL, NULL);
}



int main(int argc, char** argv)
{

  /* init GStreamer */
  gst_init(&argc, &argv);

  Fl::lock();
  initUi();

  std::thread findNdiThread(findNdiDevices, app.ui);

  const int result = Fl::run();

  if (findNdiThread.joinable()) {
    findNdiThread.join();
  }

  return result;
}

void logAppend_cb(void* msgPtr)
{
  string* msgStr = static_cast<string*>(msgPtr);
  app.ui->logDisplay->insert(msgStr->c_str());
  app.ui->logDisplay->insert("\n");
  delete msgPtr;
  return;
}

void ristLogAppend_cb(void* msgPtr)
{
  string* msgStr = static_cast<string*>(msgPtr);
  app.ui->ristLogDisplay->insert(msgStr->c_str());
  delete msgPtr;
  return;
}

void logAppend(string logMessage)
{
  auto* logMessageCpy = new string;
  *logMessageCpy = _strdup(logMessage.c_str());
  Fl::awake(logAppend_cb, logMessageCpy);
  return;
}

void ristLogAppend(string logMessage)
{
  auto* logMessageCpy = new string;
  *logMessageCpy = _strdup(logMessage.c_str());
  Fl::awake(ristLogAppend_cb, logMessageCpy);
  return;
}

int ristLog(void* arg, enum rist_log_level, const char* msg)
{
  ristLogAppend(msg);
  return 1;
}

void startStream()
{
 encoder = new Encode(&config);
 encoder->log_func = logAppend;
  app.current_bitrate = std::stoi(config.bitrate);
  if (config.use_rpc_control) {

    RpcData rpcData;
      rpcData.bitrate = config.bitrate;
      rpcData.rist_output_address = config.rist_output_address;
      rpcData.rist_output_buffer_min = config.rist_output_buffer_min;
      rpcData.rist_output_buffer_max = config.rist_output_buffer_max;
      rpcData.rist_output_rtt_min = config.rist_output_rtt_min;
      rpcData.rist_output_rtt_max = config.rist_output_rtt_max;
      rpcData.rist_output_reorder_buffer = config.rist_output_reorder_buffer;
      rpcData.rist_output_bandwidth = config.rist_output_bandwidth;
      rpcData.rtmp_address = config.rtmp_address;
      rpcData.rtmp_key = config.rtmp_key;
      rpcData.codec = static_cast<int>(config.codec);
      rpcData.upscale = config.upscale;
      rpcData.reencode_bitrate = config.reencode_bitrate;
    

    try {
        std::future<void> future = std::async(
            std::launch::async,
          [rpcData]()
            {
              Url url {fmt::format("rist://{}", config.rist_output_address)};
              rpc::client client(url.getHost(), app.rpc_port);
              client.call("start", rpcData);
            });

        std::future_status status;

        using namespace std::chrono_literals;
        switch (status = future.wait_for(5s); status) {
          case std::future_status::timeout:
            logAppend("Server start timed out.");
            break;
          case std::future_status::ready:
            logAppend("Server pipeline started.");
            break;
        }
    } catch (const std::exception& e) {
      logAppend(e.what());
    }
  }
  app.is_running = true;
  encoder->run_encode_thread();
  string rist_output_url = fmt::format(
      "rist://"
      "{}?bandwidth={}buffer-min={}&buffer-max={}&rtt-min={}&rtt-max={}&"
      "reorder-buffer={}",
      config.rist_output_address,
      config.rist_output_bandwidth,
      config.rist_output_buffer_min,
      config.rist_output_buffer_max,
      config.rist_output_rtt_min,
      config.rist_output_rtt_max,
      config.rist_output_reorder_buffer);
      string rist_input_url = "rtp://@127.0.0.1:6000";
  app.transport_thread_future = std::async(std::launch::async,
                                           rist_sender::run_rist_sender,
                                           rist_input_url,
                                           rist_output_url,
                                           &ristLog,
                                           &gotRistStatistics,
                                           &app.is_running);

  Fl::lock();
  app.ui->btnStartStream->deactivate();
  app.ui->btnStopStream->activate();
  Fl::unlock();
  Fl::awake();
}

void stopStream()
{
  app.is_running = false;
  encoder->stop_encode_thread();
  app.transport_thread_future.wait();
  Fl::lock();
  app.ui->btnStopStream->deactivate();
  Fl::unlock();
  delete encoder;

  if (config.use_rpc_control) {
    try {
      std::future<void> future = std::async(
          std::launch::async,
          []()
          {
            Url url {fmt::format("rist://{}", config.rist_output_address)};
            rpc::client client(url.getHost(), app.rpc_port);
            client.call("stop");
          });

      std::future_status status;

      using namespace std::chrono_literals;
      switch (status = future.wait_for(5s); status) {
        case std::future_status::timeout:
          logAppend("Server stop timed out.");
          break;
        case std::future_status::ready:
          logAppend("Server pipeline stopped.");
          break;
      }

    } catch (const std::exception& e) {
      logAppend(e.what());
    }
  }

  Fl::lock();
  app.ui->btnStartStream->activate();
  Fl::unlock();
  Fl::awake();
}

void ristStatistics_cb(void* msgPtr)
{
  rist_stats statistics = *(rist_stats*)msgPtr;
  app.ui->bandwidthOutput->value(
      std::to_string(statistics.stats.sender_peer.bandwidth / 1000).c_str());
  app.ui->linkQualityOutput->value(
      std::to_string(statistics.stats.sender_peer.quality).c_str());
  app.ui->totalPacketsOutput->value(
      std::to_string(statistics.stats.sender_peer.sent).c_str());
  app.ui->retransmittedPacketsOutput->value(
      std::to_string(statistics.stats.sender_peer.retransmitted).c_str());
  app.ui->rttOutput->value(
      std::to_string(statistics.stats.sender_peer.rtt).c_str());
  app.ui->encodeBitrateOutput->value(
      std::to_string(app.current_bitrate).c_str());
  delete msgPtr;
  return;
}

void gotRistStatistics(const rist_stats& statistics)
{
  int bitrateDelta = 0;
  double qualDiffPct;
  int adjBitrate;
  double maxBitrate = std::stoi(config.bitrate);

  if (app.previous_quality > 0
      && (int)statistics.stats.sender_peer.quality != (int)app.previous_quality)
  {
    qualDiffPct = statistics.stats.sender_peer.quality / app.previous_quality;
    adjBitrate = (int)(app.current_bitrate * qualDiffPct);
    bitrateDelta = adjBitrate - app.current_bitrate;
  }

  if (static_cast<int>(app.previous_quality) == 100
      && static_cast<int>(statistics.stats.sender_peer.quality) == 100
      && app.current_bitrate < maxBitrate)
  {
    qualDiffPct = (app.current_bitrate / maxBitrate);
    adjBitrate = (int)(app.current_bitrate * (1 + qualDiffPct));
    bitrateDelta = adjBitrate - app.current_bitrate;
  }

  if (bitrateDelta != 0) {
    int newBitrate =
        std::max(std::min(app.current_bitrate += bitrateDelta / 2, (uint16_t)maxBitrate), static_cast<const uint16_t>(1000));
    app.current_bitrate = newBitrate;
    encoder->set_encode_bitrate(newBitrate);
  }

  app.previous_quality = statistics.stats.sender_peer.quality;

  rist_stats* p_statistics = new rist_stats;
  *p_statistics = statistics;

  Fl::awake(ristStatistics_cb, p_statistics);
}

void ndi_source_select_cb(Fl_Widget* o, void* v)
{
  config.ndi_input_name = (char*)v;
}

void startStream_cb(Fl_Button* o, void* v)
{
  startStream();
}

void stopStream_cb(Fl_Button* o, void* v)
{
  stopStream();
}

void refreshSources_cb(Fl_Button* o, void* v)
{
  GList* devices = gst_device_monitor_get_devices(app.monitor);

  app.ui->ndiSourceSelect->clear();
  app.ui->btnRefreshSources->deactivate();

  g_list_foreach(devices, addNdiDevice, NULL);

  app.ui->btnRefreshSources->activate();
}

void select_codec_cb(Fl_Menu_* o, Codec v)
{
  config.codec = v;
}

void select_encoder_cb(Fl_Menu_* o, Encoder v)
{
  config.encoder = v;
}

void rist_address_cb(Fl_Input* o, void* v)
{
  config.rist_output_address = o->value();
}

void rtmp_address_cb(Fl_Input* o, void* v)
{
  config.rtmp_address = o->value();
}

void rtmp_key_cb(Fl_Input* o, void* v)
{
  config.rtmp_key = o->value();
}

void rist_bandwidth_cb(Fl_Input* o, void* v)
{
  config.rist_output_bandwidth = o->value();
}

void rist_buffer_min_cb(Fl_Input* o, void* v)
{
  config.rist_output_buffer_min = o->value();
}

void rist_buffer_max_cb(Fl_Input* o, void* v)
{
  config.rist_output_buffer_max = o->value();
}
void rist_rtt_min_cb(Fl_Input* o, void* v)
{
  config.rist_output_rtt_min = o->value();
}
void rist_rtt_max_cb(Fl_Input* o, void* v)
{
  config.rist_output_rtt_max = o->value();
}
void rist_reorder_buffer_cb(Fl_Input* o, void* v)
{
  config.rist_output_reorder_buffer = o->value();
}

void encoder_bitrate_cb(Fl_Input* o, void* v)
{
  config.bitrate = o->value();

  if (app.is_playing) {
    encoder->set_encode_bitrate(std::stoi(config.bitrate));
  }
}

void use_rpc_cb(Fl_Check_Button* o, void* v)
{
  config.use_rpc_control = o->value();
}

void upscale_cb(Fl_Check_Button* o, void* v)
{
  config.upscale = o->value();
}

void reencodeBitrate_cb(Fl_Input* o, void* v)
{
  config.reencode_bitrate = o->value();
}

void* findNdiDevices(void* p)
{
  UserInterface* ui = (UserInterface*)p;
  GstBus* bus;
  GstCaps* caps;

  app.monitor = gst_device_monitor_new();
  bus = gst_device_monitor_get_bus(app.monitor);
  caps = gst_caps_new_empty_simple("application/x-ndi");
  gst_device_monitor_add_filter(app.monitor, "Video/Source", caps);
  gst_caps_unref(caps);

  gst_device_monitor_start(app.monitor);

  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  while ((std::chrono::duration_cast<std::chrono::seconds>(end - start).count()
          != 2))
  {
    GstMessage* msg =
        gst_bus_timed_pop_filtered(bus, 0, GST_MESSAGE_DEVICE_ADDED);
    if (msg) {
      GstDevice* newDevice;
      gst_message_parse_device_added(msg, &newDevice);
      char* newDeviceDisplayName = gst_device_get_display_name(newDevice);

      Fl::lock();
      app.ui->ndiSourceSelect->add(newDeviceDisplayName,
                                   0,
                                   ndi_source_select_cb,
                                   newDeviceDisplayName,
                                   0);
      Fl::unlock();
      Fl::awake();
      gst_object_unref(newDevice);
    }
    end = std::chrono::system_clock::now();
  }

  return 0L;
}

void addNdiDevice(gpointer devicePtr, gpointer p)
{
  GstDevice* device = static_cast<GstDevice*>(devicePtr);

  char* newDeviceDisplayName = gst_device_get_display_name(device);

  app.ui->ndiSourceSelect->add(
      newDeviceDisplayName, 0, ndi_source_select_cb, newDeviceDisplayName, 0);
  gst_object_unref(device);
}