#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <gst/video/video.h>
#include "rpc/server.h"
#include <ristreceiver.h>
#include "gst/gstmessage.h"
#include <atomic>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fmt/core.h>
namespace nrs
{
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

enum class Codec {
  h264,
  h265,
  av1
};

struct App
{
  std::string pipeline_str;
  GstElement *datasrc_pipeline;
  GstElement *video_src;
  GstBus* bus;

  std::atomic_bool is_playing = false;
  gboolean debug = false;

  std::future<int> rist_receive_future;
  std::future<void> gstreamer_bus_future;

  uint16_t udp_internal_port = 7000;
};

struct Config
{
  std::string rist_input_address = "";
  std::string rtmp_output_address = "";
  Codec codec = Codec::h264;
  bool upscale = true;
};

struct BufferDataStruct
{
  gsize buf_size {0};
  uint8_t* buf_data;
};

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
  int codec;
  bool upscale;
  MSGPACK_DEFINE_ARRAY(bitrate,   rist_output_address,   rist_output_buffer_min,   rist_output_buffer_max,   rist_output_rtt_min,   rist_output_rtt_max,   rist_output_reorder_buffer,   rist_output_bandwidth,   rtmp_address,   rtmp_key, codec, upscale);
};

void log();

void run_rpc_server();
void rpc_call_start(RpcData data);
void rpc_call_stop();

void start_gstreamer(RpcData &data);
void start_rist(RpcData &data);
int ristLog(void* arg, enum rist_log_level logLevel, const char* msg);
void stop_gstreamer();
void stop_rist();

void rist_receive_loop();
void gstreamer_src_loop();
void gstreamer_bus_loop();

void build_pipeline();
void pipeline_build_source();
void pipeline_build_sink();
void pipeline_build_video_decode();
void pipeline_build_audio_remux();
void pipeline_build_video_encoder();
void parse_pipeline();
void play_pipeline();
void handle_gst_message_error(GstMessage* message);
void handle_gst_message_eos(GstMessage* message);
void handle_gstreamer_message(GstMessage* message);

Config config;
App app;

void log(string message)
{
  std::cout << message << endl;
}

void pipeline_build_sink()
{
  app.pipeline_str +=
      "flvmux streamable=true name=mux ! queue ! rtmpsink location='"
      + config.rtmp_output_address + "' name=rtmpSink multiqueue name=outq ";
}

void pipeline_build_source()
{
  app.pipeline_str += fmt::format(
      " udpsrc port={} do-timestamp=true name=videosrc ! "
      "rtpjitterbuffer max-ts-offset-adjustment=500 rfc7273-sync=true mode=synced sync-interval=1 add-reference-timestamp-meta=1 ! rtpmp2tdepay ! tsparse ! tsdemux name=demux ", app.udp_internal_port);
}

void pipeline_build_audio_remux()
{
  app.pipeline_str +=
      " demux. ! aacparse ! queue max-size-time=5000000000 ! outq.sink_1 "
      "outq.src_1 ! mux.";
}

void pipeline_build_video_decode()
{
  switch (config.codec)
  {
  case Codec::av1:
    app.pipeline_str += " demux. ! av1parse ! queue ! nvav1dec !";
    break;

  case Codec::h265:
    app.pipeline_str += " demux. ! h265parse ! queue ! nvh265dec !";
    break;
  
  default:
    app.pipeline_str += " demux. ! h264parse ! queue ! nvh264dec !";
    break;
  }
}

void pipeline_build_video_encoder()
{
  if (config.upscale)
  {
    app.pipeline_str +=
      " queue ! cudascale ! "
      "video/x-raw(memory:CUDAMemory),width=2560,height=1440 ! "
      "queue ! nvh264enc rc-mode=cbr-hq bitrate=24000 gop-size=120 preset=hq bframes=2 ! ";
  } else {
    app.pipeline_str +=
      "queue ! nvh264enc rc-mode=cbr-hq bitrate=12000 gop-size=120 preset=hq bframes=2 ! ";
  }
  
  app.pipeline_str +=
      "video/x-h264,framerate=60/1,profile=high ! h264parse ! outq.sink_0 "
      "outq.src_0 ! mux.";
}

void build_pipeline()
{
  app.pipeline_str = "";
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
    log("*** Bad pipeline **");
  }

  app.video_src =
      gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "videosrc");

  auto caps = gst_caps_new_simple("application/x-rtp",
                                  "media",
                                  G_TYPE_STRING,
                                  "video",
                                  "encoding-name",
                                  G_TYPE_STRING,
                                  "MP2T",
                                  "clock-rate",
                                  G_TYPE_INT,
                                  90000,
                                  NULL);
  g_object_set(app.video_src, "caps", caps, NULL);

  app.bus = gst_element_get_bus(app.datasrc_pipeline);
}

void start_gstreamer(RpcData& data)
{
  config.rtmp_output_address =
      fmt::format("rtmp://{}/{} live=true", data.rtmp_address, data.rtmp_key);
  config.codec = static_cast<Codec>(data.codec);
  config.upscale = data.upscale;
  build_pipeline();
  parse_pipeline();
  
  log("Playing pipeline.");
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);

  app.gstreamer_bus_future = std::async(std::launch::async, gstreamer_bus_loop);
}

void stop_gstreamer()
{
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);
  gst_object_unref(GST_OBJECT(app.datasrc_pipeline));
  gst_object_unref(app.bus);
  log("Stopping pipeline.");

  std::future_status status;

  switch (status = app.gstreamer_bus_future.wait_for(std::chrono::seconds(5));
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
  string rist_input_url = fmt::format(
      "rist://@[::]:5000"
      "?bandwidth={}buffer-min={}&buffer-max={}&rtt-min={}&rtt-max={}&"
      "reorder-buffer={}",
      data.rist_output_bandwidth,
      data.rist_output_buffer_min,
      data.rist_output_buffer_max,
      data.rist_output_rtt_min,
      data.rist_output_rtt_max,
      data.rist_output_reorder_buffer);
      string rist_output_url = fmt::format("rtp://@127.0.0.1:{}", app.udp_internal_port);
      app.rist_receive_future = std::async(std::launch::async, rist_receiver::run_rist_receiver,
                                           rist_input_url,
                                           rist_output_url,
                                           &ristLog,
                                           &app.is_playing);
}

void stop_rist()
{
  std::future_status status;

  switch (status = app.rist_receive_future.wait_for(std::chrono::seconds(1));
          status)
  {
    case std::future_status::timeout:
      log("Waiting for receiver stop has timed out.");
      break;
    case std::future_status::ready:
      log("Receiver stopped.");
      break;
  }
}

int ristLog(void* arg, enum rist_log_level logLevel, const char* msg)
{
  // log(msg);
  return 1;
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
  app.is_playing = true;
  start_gstreamer(data);
  start_rist(data);
}

void rpc_call_stop()
{
  app.is_playing = false;
  log("Stopping.");
  stop_gstreamer();
  stop_rist();
}

void run_rpc_server()
{
  // Create a server that listens on port 5999
  rpc::server srv("0.0.0.0", 5999);

  std::cout << "RIST Restreamer Started - Listening on port " << srv.port()
            << endl;

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
