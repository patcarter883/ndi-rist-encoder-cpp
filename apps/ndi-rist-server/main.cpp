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
#include "Url.h"
using homer6::Url;

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

  bool hasTS = false;
  bool hasFLV = false;
  bool hasSDP = false;
  bool hasST2110 = false;
};

struct Config
{
  std::string rist_input_address = "";
  std::string streamid = "";
  Codec codec = Codec::h264;
  bool upscale = true;
  std::string reencode_bitrate = "";
  std::vector<std::array<std::string, 2>> stream_destinations;
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
    int rist_output_streams;
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
    std::vector<std::array<std::string, 2>> stream_destinations;
MSGPACK_DEFINE_ARRAY(bitrate,
                     rist_output_address,
                     rist_output_streams,
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
                     upscale,
                     stream_destinations);
};

void log();

void run_rpc_server();
void rpc_call_start(RpcData data);
void rpc_call_stop();

void start_gstreamer();
void start_rist(string rist_input_url, string rist_output_url);
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

  log("Building pipeline sinks.");

  app.pipeline_str += " multiqueue name=outq multiqueue name=inq tee name=vtee tee name=atee ";
  int SDPCount = 0;
  int ST2110Count = 0;

  for (auto& element : config.stream_destinations) {
    log(element[0]);
    Url url{ element[0] };
    auto streamKey = element[1];

    if (url.getScheme() == "srt")
    {
      app.hasTS = true;

      app.pipeline_str += fmt::format(
        "tstee. ! queue silent=true ! tsparse alignment=7 ! srtsink uri={} mode=caller wait-for-connection=false  ", url.toString());
    }
    else if (url.getScheme() == "rtmp")
    {
      app.hasFLV = true;

      app.pipeline_str += fmt::format(
        "flvtee. ! queue ! rtmpsink location='{}/{} live=true'  ", url.toString(), streamKey);
    } else if (url.getScheme() == "sdp")
    {

      if (url.getPath() == "/raw")
      {
        app.hasST2110 = true;
        app.pipeline_str += fmt::format("rtpst2110vtee. ! st2110rtpbin.send_rtp_sink_{} ", SDPCount);
        app.pipeline_str += fmt::format("st2110rtpbin.send_rtp_src_{} ! udpsink host={} port={}  ", SDPCount, url.getHost(), url.getPort());
        app.pipeline_str += fmt::format("st2110rtpbin.send_rtcp_src_{} ! udpsink host={} port={} sync=false async=false ",SDPCount, url.getHost(), url.getPort() + 1);

        app.pipeline_str += fmt::format("rtpst2110atee. ! st2110rtpbin.send_rtp_sink_{} ", SDPCount + 1);
        app.pipeline_str += fmt::format("st2110rtpbin.send_rtp_src_{} ! udpsink host={} port={} ", SDPCount + 1, url.getHost(), url.getPort() + 2);
        app.pipeline_str += fmt::format("st2110rtpbin.send_rtcp_src_{} ! udpsink host={} port={} sync=false async=false ", SDPCount + 1, url.getHost(), url.getPort() +3 );

        SDPCount = SDPCount + 2;
      } else
      {
        app.hasSDP = true;
        
        app.pipeline_str += fmt::format("rtpvtee. ! sdprtpbin.send_rtp_sink_{} ", SDPCount);
        app.pipeline_str += fmt::format("sdprtpbin.send_rtp_src_{} ! udpsink host={} port={}  ", SDPCount, url.getHost(), url.getPort());
        app.pipeline_str += fmt::format("sdprtpbin.send_rtcp_src_{} ! udpsink host={} port={} sync=false async=false ",SDPCount, url.getHost(), url.getPort() + 1);

        app.pipeline_str += fmt::format("rtpatee. ! sdprtpbin.send_rtp_sink_{} ", SDPCount + 1);
        app.pipeline_str += fmt::format("sdprtpbin.send_rtp_src_{} ! udpsink host={} port={} ", SDPCount + 1, url.getHost(), url.getPort() + 2);
        app.pipeline_str += fmt::format("sdprtpbin.send_rtcp_src_{} ! udpsink host={} port={} sync=false async=false ", SDPCount + 1, url.getHost(), url.getPort() +3 );

        SDPCount = SDPCount + 2;
      }
      
    }

  }

  if (app.hasTS)
  {
    app.pipeline_str += " mpegtsmux name=mpegtsmux ! tee name=tstee vtee. ! queue silent=true ! mpegtsmux. atee. ! queue silent=true max-size-time=5000000000 ! mpegtsmux.  ";
  }

  if (app.hasFLV)
  {
    app.pipeline_str += " vtee. ! queue silent=true ! h264parse config-interval=-1 ! video/x-h264,framerate=60/1,profile=high,stream-format=avc ! flvmux streamable=true name=flvmux ! tee name=flvtee  demuxatee. ! queue silent=true ! flvmux.audio  ";
  }

  if (app.hasSDP)
  {
    app.pipeline_str +=
  " rtpbin name=sdprtpbin buffer-mode=synced do-retransmission=true do-lost=true ntp-sync=true ntp-time-source=ntp rfc7273-sync=true update-ntp64-header-ext=true "
  " vtee. ! queue silent=true ! h264parse ! rtph264pay pt=102 config-interval=-1 ! tee name=rtpvtee "
	" atee. ! queue silent=true ! audioconvert ! rtpL24pay ! application/x-rtp, pt=103, payload=103, clock-rate=48000, channels=2 ! tee name=rtpatee ";
  }

  if (app.hasST2110)
  {
    app.pipeline_str +=
  " rtpbin name=st2110rtpbin buffer-mode=synced do-retransmission=true do-lost=true ntp-sync=true ntp-time-source=ntp rfc7273-sync=true update-ntp64-header-ext=true "
  " rawvtee. ! queue silent=true ! cudadownload ! videoconvert ! video/x-raw,format=UYVY ! rtpvrawpay pt=102 mtu=64000 ! tee name=rtpst2110vtee "
	" atee. ! queue silent=true ! audioconvert ! rtpL24pay mtu=64000 ! application/x-rtp, pt=103, payload=103, clock-rate=48000, channels=2 ! tee name=rtpst2110atee ";
  }
  
}
  

void pipeline_build_source()
{
  app.pipeline_str += fmt::format(
      " rtpbin buffer-mode=synced do-retransmission=true do-lost=true ntp-sync=true ntp-time-source=ntp rfc7273-sync=true update-ntp64-header-ext=true name=recvrtpbin udpsrc port={} ! application/x-rtp,media=video,encoding-name=MP2T,clock-rate=90000, payload=33 ! recvrtpbin.recv_rtp_sink_0 recvrtpbin. ! rtpmp2tdepay ! "
      " tsparse ! tsdemux name=demux ", app.udp_internal_port, app.udp_internal_port);
}

void pipeline_build_audio_demux()
{
  app.pipeline_str +=
      " demux. ! aacparse ! inq.sink_1 "
      " inq.src_1 ! tee name=demuxatee ";
}

void pipeline_build_audio_decode()
{
  app.pipeline_str +=
      " demuxatee. ! queue silent=true ! aacparse ! fdkaacdec ! outq.sink_1 outq.src_1 ! atee.";
}

void pipeline_build_video_decode()
{
  switch (config.codec)
  {
  case Codec::av1:
    app.pipeline_str += " demux. ! av1parse ! inq.sink_0 inq.src_0 ! nvav1dec ! ";
    break;

  case Codec::h265:
    app.pipeline_str += " demux. ! h265parse ! inq.sink_0 inq.src_0 ! nvh265dec ! ";
    break;
  
  default:
    app.pipeline_str += " demux. ! h264parse ! inq.sink_0 inq.src_0 ! nvh264dec ! ";
    break;
  }

  if (config.upscale)
  {
    app.pipeline_str +=
      " cudascale ! "
      "video/x-raw(memory:CUDAMemory),width=2560,height=1440 ! ";
  } else
  {
    app.pipeline_str +=
      "  cudascale ! "
      "video/x-raw(memory:CUDAMemory),width=1920,height=1080 ! ";
  }

  app.pipeline_str +=
      " tee name=rawvtee ";
}

void pipeline_build_video_encode()
{ 

    app.pipeline_str +=
      fmt::format("  nvcudah264enc name=encoder rate-control=cbr tune=low-latency bitrate={} gop-size=120 preset=7 ! video/x-h264,profile=high ! ", config.reencode_bitrate);
  
  app.pipeline_str +=
      "outq.sink_0 outq.src_0 ! vtee.";
}

void build_pipeline()
{
  app.pipeline_str = "";
  pipeline_build_source();
  pipeline_build_sink();
  pipeline_build_audio_demux();
  pipeline_build_audio_decode();
  pipeline_build_video_decode();
  if (app.hasFLV || app.hasSDP || app.hasTS)
  {
    pipeline_build_video_encode();
  }
  
  
  
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

  // auto caps = gst_caps_new_simple("application/x-rtp",
  //                                 "media",
  //                                 G_TYPE_STRING,
  //                                 "video",
  //                                 "encoding-name",
  //                                 G_TYPE_STRING,
  //                                 "MP2T",
  //                                 "clock-rate",
  //                                 G_TYPE_INT,
  //                                 90000,
  //                                 NULL);
  // g_object_set(app.video_src, "caps", caps, NULL);

  app.bus = gst_element_get_bus(app.datasrc_pipeline);
}

void start_gstreamer()
{

  build_pipeline();
  parse_pipeline();
  
  log("Playing pipeline.");
  gst_debug_bin_to_dot_file(GST_BIN(app.datasrc_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "preplay");
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);
  gst_debug_bin_to_dot_file(GST_BIN(app.datasrc_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "play");
  app.gstreamer_bus_future = std::async(std::launch::async, gstreamer_bus_loop);
}

void stop_gstreamer()
{
  gst_debug_bin_to_dot_file(GST_BIN(app.datasrc_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "prestop");
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

void start_rist(string rist_input_url, string rist_output_url)
{

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
  log("Start Requested.");

  config.streamid = data.rtmp_key;
  config.codec = static_cast<Codec>(data.codec);
  config.upscale = data.upscale;
  config.reencode_bitrate = data.reencode_bitrate;
  config.stream_destinations = data.stream_destinations;

  Url url{ fmt::format("rist://{}", data.rist_output_address) };

  string rist_input_url;

  for (int i = 0; i < data.rist_output_streams; i = i + 1)
  {
      rist_input_url.append(fmt::format(
          "rist://@[::]:{}"
          "?bandwidth={}buffer-min={}&buffer-max={}&rtt-min={}&rtt-max={}&"
          "reorder-buffer={}",
          url.getPort() + (2 * i),
          data.rist_output_bandwidth,
          data.rist_output_buffer_min,
          data.rist_output_buffer_max,
          data.rist_output_rtt_min,
          data.rist_output_rtt_max,
          data.rist_output_reorder_buffer));

      if (data.rist_output_streams > 1 && i < (data.rist_output_streams - 1))
      {
          rist_input_url.append(",");
      }
  }

  string rist_output_url = fmt::format("udp://@127.0.0.1:{}", app.udp_internal_port);

  app.is_playing = true;
  start_gstreamer();
  start_rist(rist_input_url, rist_output_url);
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
