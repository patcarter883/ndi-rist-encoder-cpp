// ndi-rist-encoder.cpp : Defines the entry point for the application.

#include <chrono>
#include <thread>

#include <FL/Fl.H>
#include <RISTNet.h>

#include "UserInterface.cxx"
#include "UserInterface.h"
using std::thread;
#include <algorithm>

#include <fmt/core.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include "Url.h"
#include "rpc/client.h"
using homer6::Url;

#include <string>
using std::string;

struct BufferListFuncData
{
  RISTNetSender* sender;
  int streamId;
};

struct App
{
  UserInterface* ui = new UserInterface;
  Fl_Text_Buffer* log_buff = new Fl_Text_Buffer;
  Fl_Text_Buffer* rist_log_buff = new Fl_Text_Buffer;

  GstElement* datasrc_pipeline;
  GstElement *video_sink, *video_encoder;
  GstDeviceMonitor* monitor;

  gboolean is_eos = false;
  gboolean is_running = false;
  GMainLoop* loop;

  gboolean is_playing = false;

  std::shared_ptr<std::thread> encode_thread, preview_thread;

  uint16_t current_bitrate;
  double previous_quality;
  uint16_t rpc_port = 5999;
};

struct Config
{
  string ndi_input_name;
  string codec = "h264";
  string encoder = "software";
  string transport = "m2ts";
  string bitrate = "4300";
  string rist_output_address = "127.0.0.1:5000";
  string rist_output_buffer_min = "245";
  string rist_output_buffer_max = "5000";
  string rist_output_rtt_min = "40";
  string rist_output_rtt_max = "500";
  string rist_output_reorder_buffer = "120";
  string rist_output_bandwidth = "6000";
  string rtmp_address = "sydney.restream.io/live";
  string rtmp_key = "";
  uint16_t use_rpc_control = 1;
};

/* Globals */
Config config;
App app;

/* Function Declaration */

void startStream();
void stopStream();
void* runEncodeThread();
void* runRistVideo(RISTNetSender* sender);

gboolean sendBufferToRist(GstBuffer* buffer,
                          RISTNetSender* sender,
                          uint16_t streamId = NULL);
gboolean sendBufferListToRist(GstBuffer** buffer,
                              guint idx,
                              gpointer userDataPtr);
gboolean handle_gstreamer_bus_message(GstBus* bus,
                                      GstMessage* message,
                                      App* app);
void handle_gst_message_error(GstMessage* message);
void handle_gst_message_eos(GstMessage* message);
void parsePipeline(string pipelineString);
void playPipeline(std::shared_ptr<std::thread> pipelineThread);
void stopPipeline();

void* previewNDISource();
void* findNdiDevices(void* p);
void addNdiDevice(gpointer devicePtr, gpointer p);

void logAppend(string logMessage);
void ristLogAppend(const char* logMessage);
int ristLog(void* arg, enum rist_log_level, const char* msg);
void gotRistStatistics(const rist_stats& statistics);

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
  app.ui->show(NULL, NULL);
}

int main(int argc, char** argv)
{

  /* init GStreamer */
  gst_init(&argc, &argv);

  Fl::lock();
  initUi();

  thread findNdiThread(findNdiDevices, app.ui);

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
  *logMessageCpy = strdup(logMessage.c_str());
  Fl::awake(logAppend_cb, logMessageCpy);
  return;
}

void ristLogAppend(const char* logMessage)
{
  auto* logMessageCpy = new string;
  *logMessageCpy = strdup(logMessage);
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
  if (config.use_rpc_control) {
    try {
      Url url {fmt::format("rist://{}", config.rist_output_address)};

      rpc::client client(url.getHost(), app.rpc_port);
      auto result = client.call(
          "start",
          fmt::format(
              "rtmp://{}/{} live=true", config.rtmp_address, config.rtmp_key));
    } catch (const std::exception& e) {
      logAppend(e.what());
    }
  }

  app.is_running = true;

  std::thread thisEncodeThread(runEncodeThread);
  thisEncodeThread.detach();

  Fl::lock();
  app.ui->btnStartStream->deactivate();
  app.ui->btnStopStream->activate();
  Fl::unlock();
  Fl::awake();
}

void stopStream()
{
  app.is_running = false;
  if (g_main_loop_is_running(app.loop)) {
    g_main_loop_quit(app.loop);
  }

  Fl::lock();
  app.ui->btnStopStream->deactivate();
  Fl::unlock();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  Fl::lock();
  app.ui->btnStartStream->activate();
  Fl::unlock();
  Fl::awake();

  if (config.use_rpc_control) {
    try {
      Url url {fmt::format("rist://{}", config.rist_output_address)};

      rpc::client client(url.getHost(), app.rpc_port);
      auto result = client.call("stop");
    } catch (const std::exception& e) {
      logAppend(e.what());
    }
  }
}

gboolean sendBufferToRist(GstBuffer* buffer,
                          RISTNetSender* sender,
                          uint16_t streamId)
{
  GstMapInfo info;

  gst_buffer_map(buffer, &info, GST_MAP_READ);
  gsize& buf_size = info.size;
  guint8*& buf_data = info.data;

  sender->sendData(buf_data, buf_size, 0, streamId);

  return true;
}

gboolean sendBufferListToRist(GstBuffer** buffer,
                              guint idx,
                              gpointer userDataPtr)
{
  BufferListFuncData* userData = static_cast<BufferListFuncData*>(userDataPtr);
  GstMapInfo info;

  gst_buffer_map(*buffer, &info, GST_MAP_READ);
  gsize& buf_size = info.size;
  guint8*& buf_data = info.data;

  userData->sender->sendData(buf_data, buf_size, 0, userData->streamId);

  return true;
}

void handle_gst_message_error(GstMessage* message)
{
  GError* err;
  gchar* debug_info;
  gst_message_parse_error(message, &err, &debug_info);
  logAppend("\nReceived error from datasrc_pipeline...\n");
  logAppend(fmt::format("Error received from element {}: {}\n",
                        GST_OBJECT_NAME(message->src),
                        err->message));
  logAppend(fmt::format("Debugging information: {}\n",
                        debug_info ? debug_info : "none"));
  g_clear_error(&err);
  g_free(debug_info);
  stopStream();
}

void handle_gst_message_eos(GstMessage* message)
{
  logAppend("\nReceived EOS from pipeline...\n");
  stopStream();
}

gboolean handle_gstreamer_bus_message(GstBus* bus,
                                      GstMessage* message,
                                      App* app)
{
  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
      handle_gst_message_error(message);
      break;
    case GST_MESSAGE_EOS:
      handle_gst_message_eos(message);
      break;
    default:
      break;
  }
  return TRUE;
}

void* runRistVideo(RISTNetSender* sender)
{
  GstSample* sample;
  GstBuffer* buffer;
  GstBufferList* bufferlist;

  while (app.is_running) {
    sample = gst_app_sink_pull_sample(GST_APP_SINK(app.video_sink));
    bufferlist = gst_sample_get_buffer_list(sample);
    if (bufferlist) {
      BufferListFuncData user_data = {.sender = sender, .streamId = 1000};

      gst_buffer_list_foreach(bufferlist, sendBufferListToRist, &user_data);
    } else {
      buffer = gst_sample_get_buffer(sample);
      if (buffer) {
        sendBufferToRist(buffer, sender);
      }
    }

    gst_sample_unref(sample);
  }

  return 0L;
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
        std::min(app.current_bitrate += bitrateDelta / 2, (uint16_t)maxBitrate);
    app.current_bitrate = std::max(newBitrate, 1000);

    g_object_set(G_OBJECT(app.video_encoder), "bitrate", newBitrate, NULL);
  }

  app.previous_quality = statistics.stats.sender_peer.quality;

  rist_stats* p_statistics = new rist_stats;
  *p_statistics = statistics;

  Fl::awake(ristStatistics_cb, p_statistics);
}

void* runEncodeThread()
{
  string datasrc_pipeline_str = "";

  
  app.current_bitrate = std::stoi(config.bitrate);

  RISTNetSender thisRistSender;
  RISTNetSender::RISTNetSenderSettings mySendConfiguration;

  // Generate a vector of RIST URL's,  ip(name), ports, RIST URL output,
  // listen(true) or send mode (false)
  string lURL = fmt::format(
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
  std::vector<std::tuple<string, int>> interfaceListSender;

  interfaceListSender.push_back(std::tuple<string, int>(lURL, 0));

  thisRistSender.statisticsCallback = gotRistStatistics;

  if (config.rist_output_bandwidth != "") {
    int recovery_maxbitrate = std::stoi(config.rist_output_bandwidth);
    if (recovery_maxbitrate > 0) {
      mySendConfiguration.mPeerConfig.recovery_maxbitrate = recovery_maxbitrate;
    }
  }

  mySendConfiguration.mLogLevel = RIST_LOG_INFO;
  mySendConfiguration.mProfile = RIST_PROFILE_MAIN;
  mySendConfiguration.mLogSetting.get()->log_cb = *ristLog;
  thisRistSender.initSender(interfaceListSender, mySendConfiguration);

  datasrc_pipeline_str += fmt::format(
      "ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux ",
      config.ndi_input_name);

  datasrc_pipeline_str +=
      " appsink buffer-list=true wait-on-eos=false sync=false name=video_sink  "
      "mpegtsmux name=tsmux ! tsparse alignment=7 ! video_sink. ";

  string audioDemux = " demux.audio ! queue ! audioresample ! audioconvert ";
  string videoDemux = " demux.video ! queue ! videoconvert ";

  string audioEncoder = " avenc_aac ! aacparse ";

  string video_encoder;

  string audioPayloader;

  string videoPayloader;

  if (config.encoder == "amd") {
    if (config.codec == "h264") {
      video_encoder = fmt::format(
          "amfh264enc name=vidEncoder  bitrate={} rate-control=cbr "
          "usage=low-latency ! video/x-h264,framerate=60/1,profile=high ! "
          "h264parse ",
          config.bitrate);
    } else if (config.codec == "h265") {
      video_encoder = fmt::format(
          "amfh265enc name=vidEncoder bitrate={} rate-control=cbr "
          "usage=low-latency ! video/x-h265,framerate=60/1 ! h265parse ",
          config.bitrate);
    } else if (config.codec == "av1") {
      video_encoder = fmt::format(
          "amfav1enc name=vidEncoder bitrate={} rate-control=cbr "
          "usage=low-latency preset=high-quality ! video/x-av1,framerate=60/1 "
          "! av1parse ",
          config.bitrate);
    }
  } else if (config.encoder == "qsv") {
    if (config.codec == "h264") {
      video_encoder = fmt::format(
          "qsvh264enc name=vidEncoder  bitrate={} rate-control=cbr "
          "target-usage=1 ! h264parse ",
          config.bitrate);
    } else if (config.codec == "h265") {
      video_encoder = fmt::format(
          "qsvh265enc name=vidEncoder bitrate={} rate-control=cbr "
          "target-usage=1 ! h265parse ",
          config.bitrate);
    } else if (config.codec == "av1") {
      video_encoder = fmt::format(
          "qsvav1enc name=vidEncoder bitrate={} rate-control=cbr "
          "target-usage=1 ! av1parse ",
          config.bitrate);
    }
  } else if (config.encoder == "nvenc") {
    if (config.codec == "h264") {
      video_encoder = fmt::format(
          "nvh264enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! h264parse ",
          config.bitrate);
    } else if (config.codec == "h265") {
      video_encoder = fmt::format(
          "nvh265enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! h265parse ",
          config.bitrate);
    } else if (config.codec == "av1") {
      video_encoder = fmt::format(
          "nvav1enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! av1parse ",
          config.bitrate);
    }
  } else {
    if (config.codec == "h264") {
      video_encoder = fmt::format(
          "x264enc name=vidEncoder speed-preset=fast tune=zerolatency "
          "bitrate={} ! h264parse ",
          config.bitrate);
    } else if (config.codec == "h265") {
      video_encoder = fmt::format(
          "x265enc name=vidEncoder bitrate={} "
          "speed-preset=fast tune=zerolatency ! h265parse ",
          config.bitrate);
    } else if (config.codec == "av1") {
      video_encoder = fmt::format(
          "rav1enc name=vidEncoder bitrate={} speed-preset=8 tile-cols=2 "
          "tile-rows=2 ! av1parse ",
          config.bitrate);
    }
  }

  audioPayloader = "queue ! tsmux. ";
  videoPayloader = "queue ! tsmux. ";

  datasrc_pipeline_str += fmt::format("{} ! {} ! {}  {} ! {} ! {}",
                                      audioDemux,
                                      audioEncoder,
                                      audioPayloader,
                                      videoDemux,
                                      video_encoder,
                                      videoPayloader);

  logAppend(datasrc_pipeline_str.c_str());

  parsePipeline(datasrc_pipeline_str);

  app.video_encoder =
      gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "vidEncoder");

  // /* set up appsink */
  app.video_sink =
      gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "video_sink");

  thread rist_video_thread(runRistVideo, &thisRistSender);

  playPipeline(app.encode_thread);
  stopPipeline();

  if (rist_video_thread.joinable()) {
    rist_video_thread.join();
  }

  thisRistSender.destroySender();

  return 0L;
}

void* previewNDISource()
{
  string pipelineString = fmt::format(
      "ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux   "
      "demux.video ! queue ! videoconvert ! autovideosink  demux.audio ! queue "
      "! audioconvert ! autoaudiosink",
      config.ndi_input_name);
  parsePipeline(pipelineString);
  playPipeline(app.preview_thread);
  stopPipeline();

  return 0L;
}

void ndi_source_select_cb(Fl_Widget* o, void* v)
{
  config.ndi_input_name = (char*)v;
}

void preview_cb(Fl_Button* o, void* v)
{
   std::thread thisPreviewThread(previewNDISource);
   thisPreviewThread.detach();
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

void select_codec_cb(Fl_Menu_* o, void* v)
{
  config.codec = (char*)v;
}

void select_encoder_cb(Fl_Menu_* o, void* v)
{
  config.encoder = (char*)v;
}

void select_transport_cb(Fl_Menu_* o, void* v)
{
  config.transport = (char*)v;
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
}

void use_rpc_cb(Fl_Check_Button* o, void* v)
{
  config.use_rpc_control = o->value();
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

void parsePipeline(string pipelineString)
{
  app.datasrc_pipeline = NULL;
  GError* error = NULL;
  GstBus* datasrc_bus;

  app.datasrc_pipeline = gst_parse_launch(pipelineString.c_str(), &error);
  if (error) {
    logAppend(fmt::format("Parse Error: {}", error->message));
    g_clear_error(&error);
  }
  if (app.datasrc_pipeline == NULL) {
    logAppend("*** Bad datasrc_pipeline ***\n");
  }

  datasrc_bus = gst_element_get_bus(app.datasrc_pipeline);

  /* add watch for messages */
  gst_bus_add_watch(
      datasrc_bus, (GstBusFunc)handle_gstreamer_bus_message, &app);
  gst_object_unref(datasrc_bus);
}

void playPipeline(std::shared_ptr<std::thread> pipelineThread)
{
  app.is_eos = FALSE;
  app.loop = g_main_loop_new(NULL, FALSE);
  app.is_playing = true;
  logAppend("Setting pipeline to PLAYING.\n");
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);
  pipelineThread = std::make_shared<std::thread>(g_main_loop_run, app.loop);
  if (pipelineThread->joinable())
  {
    pipelineThread->join();
  }
  app.is_playing = false;
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);

  gst_object_unref(GST_OBJECT(app.datasrc_pipeline));
  g_main_loop_unref(app.loop);
}

void stopPipeline()
{

  
}