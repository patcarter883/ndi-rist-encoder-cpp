#include "main.h"

#include <boost/circular_buffer.hpp>
#include <fmt/core.h>
namespace nrs
{
using std::cerr;
using std::cout;
using std::string;
using std::vector;

Config config;
App app;

boost::circular_buffer<std::pair<uint8_t, size_t>> receive_buffer;

void log(string message)
{
  std::cout << message << "\n";
}

void pipeline_build_sink()
{
  app.pipeline_str =
      "flvmux streamable=true name=mux ! queue ! rtmpsink location='"
      + config.rtmp_output_address + "' name=rtmpSink multiqueue name=outq ";
}

void pipeline_build_source()
{
  app.pipeline_str +=
      " appsrc emit-signals=false block=true is-live=true name=videosrc ! "
      "queue2 ! tsparse set-timestamps=true alignment=7 ! tsdemux name=demux ";
}

void pipeline_build_audio_remux()
{
  app.pipeline_str +=
      " demux. ! aacparse ! queue max-size-time=5000000000 ! outq.sink_1 "
      "outq.src_1 ! mux.";
}

void pipeline_build_video_decode()
{
  app.pipeline_str += " demux. ! av1parse ! queue ! nvav1dec !";
}

void pipeline_build_video_encoder()
{
  app.pipeline_str +=
      " queue ! cudascale ! "
      "video/x-raw(memory:CUDAMemory),width=2560,height=1440 ! queue ! "
      "nvh264enc rc-mode=cbr-hq bitrate=16000 gop-size=120 preset=hq ! "
      "video/x-h264,framerate=60/1,profile=high ! h264parse ! outq.sink_0 "
      "outq.src_0 ! mux.";
}

void build_pipeline()
{
  pipeline_build_source();
  pipeline_build_sink();
  pipeline_build_audio_remux();
  pipeline_build_video_decode();
  pipeline_build_video_encoder();
}

void parse_pipeline()
{
  app.datasrc_pipeline = NULL;
  GError* error = NULL;

  log(app.pipeline_str);

  app.datasrc_pipeline = gst_parse_launch(app.pipeline_str.c_str(), &error);
  if (error) {
    log(fmt::format("Parse Error: {}", error->message));
    g_clear_error(&error);
  }
  if (app.datasrc_pipeline == NULL) {
    log("*** Bad pipeline ***\n");
  }

  app.video_src =
      gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "video_src");

  auto caps = gst_caps_new_simple("video/mpegts",
                                  "systemstream",
                                  G_TYPE_BOOLEAN,
                                  true,
                                  "packetsize",
                                  G_TYPE_INT,
                                  188,
                                  NULL);
  gst_app_src_set_caps(GST_APP_SRC(app.video_src), caps);

  app.bus = gst_element_get_bus(app.datasrc_pipeline);
}

void start_gstreamer(RpcData& data)
{
  config.rtmp_output_address =
      fmt::format("rtmp://{}/{} live=true", data.rtmp_address, data.rtmp_key);
  build_pipeline();
  parse_pipeline();
  app.is_playing = true;
  log("Playing pipeline.\n");
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);

  app.gstreamer_src_future = std::async(std::launch::async, gstreamer_src_loop);
  app.gstreamer_bus_future = std::async(std::launch::async, gstreamer_bus_loop);
}

void stop_gstreamer()
{
  app.is_playing = false;
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);
  gst_object_unref(GST_OBJECT(app.datasrc_pipeline));
  gst_object_unref(app.bus);
  log("Stopping pipeline.\n");

  std::future_status status;

  switch (status = app.gstreamer_src_future.wait_for(std::chrono::seconds(1));
          status)
  {
    case std::future_status::timeout:
      log("Waiting for encoder stop has timed out.");
      break;
    case std::future_status::ready:
      log("Encoder stopped.");
      break;
  }
}

void start_rist(RpcData& data)
{
  config.rist_input_address = fmt::format(
      "rist://@0.0.0.0:5000"
      "?bandwidth={}buffer-min={}&buffer-max={}&rtt-min={}&rtt-max={}&"
      "reorder-buffer={}",
      data.rist_output_bandwidth,
      data.rist_output_buffer_min,
      data.rist_output_buffer_max,
      data.rist_output_rtt_min,
      data.rist_output_rtt_max,
      data.rist_output_reorder_buffer);
}

int ristLog(void* arg, enum rist_log_level logLevel, const char* msg)
{
  log(msg);
  return 1;
}

int on_data_from_rist(
    const uint8_t* buf,
    size_t len,
    std::shared_ptr<RISTNetReceiver::NetworkConnection>& connection,
    rist_peer* pPeer,
    uint16_t connectionID)
{
  receive_buffer.push_back(std::pair<uint8_t, size_t> {*buf, len});

  return 0;  // Keep connection
}

void rist_receive_loop()
{
  RISTNetReceiver ristReceiver;

  std::vector<std::string> interfaceListReceiver;
  interfaceListReceiver.push_back(config.rist_input_address);

  ristReceiver.networkDataCallback = std::bind(&on_data_from_rist,
                                               std::placeholders::_1,
                                               std::placeholders::_2,
                                               std::placeholders::_3,
                                               std::placeholders::_4,
                                               std::placeholders::_5);

  RISTNetReceiver::RISTNetReceiverSettings myReceiveConfiguration;
  myReceiveConfiguration.mLogLevel = app.debug ? RIST_LOG_DEBUG : RIST_LOG_INFO;
  myReceiveConfiguration.mProfile = RIST_PROFILE_MAIN;
  myReceiveConfiguration.mLogSetting.get()->log_cb = ristLog;

  if (!ristReceiver.initReceiver(interfaceListReceiver, myReceiveConfiguration))
  {
    log("Couldn't start RIST Receiver.");
    app.is_playing = false;
  }

  while (app.is_playing) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  };

  ristReceiver.destroyReceiver();
}

void gstreamer_src_loop()
{
  do {
    if (!receive_buffer.empty()) {
      std::pair<uint8_t, size_t> bufData = receive_buffer[0];
      receive_buffer.pop_front();

      GstBuffer* buffer;
      GstFlowReturn ret;

      buffer = gst_buffer_new_memdup(&bufData.first, bufData.second);

      g_signal_emit_by_name(app.video_src, "push-buffer", buffer, &ret);
      gst_buffer_unref(buffer);

      if (ret != GST_FLOW_OK) {
        /* some error, stop sending data */
        cerr << "Appsrc buffer push error\n";
        g_signal_emit_by_name(app.video_src, "end-of-stream", &ret);
        app.is_playing = false;
      }
    }

  } while (app.is_playing);
}

void gstreamer_bus_loop()
{
  do {
    GstMessage* msg = gst_bus_timed_pop(app.bus, -1);
    handle_gstreamer_message(msg);
    gst_message_unref(msg);
  } while (app.is_playing);
}

void handle_gst_message_error(GstMessage* message)
{
  GError* err;
  gchar* debug_info;
  gst_message_parse_error(message, &err, &debug_info);
  log("Received error from app.datasrc_pipeline...");
  log(fmt::format("Error received from element {}: {}",
                  GST_OBJECT_NAME(message->src),
                  err->message));
  log(fmt::format("Debugging information: {}",
                  debug_info ? debug_info : "none"));
  g_clear_error(&err);
  g_free(debug_info);
  app.is_playing = false;
}

void handle_gst_message_eos(GstMessage* message)
{
  log("Received EOS from pipeline...");
  app.is_playing = false;
}

void handle_gstreamer_message(GstMessage* message)
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
}

void rpc_call_start(RpcData data)
{
  log("Start Requested for destination " + data.rtmp_address);
  start_gstreamer(data);
  start_rist(data);
}

void rpc_call_stop()
{
  log("Stopping.");
}

void run_rpc_server()
{
  // Create a server that listens on port 5999
  rpc::server srv("0.0.0.0", 5999);

  std::cout << "RIST Restreamer Started - Listening on port " << srv.port()
            << "\n";

  srv.bind("start", &rpc_call_start);
  srv.bind("stop", &rpc_call_stop);

  // Run the server loop.
  srv.run();
}
}  // namespace nrs

int main(int argc, char** argv)
{
  gst_init(&argc, &argv);
  const std::vector<std::string> args(
      argv + 1, argv + argc);  // convert C-style to modern C++
  for (auto a : args) {
    if (a == "debug") {
      nrs::app.debug = TRUE;
    }
  }

  nrs::run_rpc_server();
}
