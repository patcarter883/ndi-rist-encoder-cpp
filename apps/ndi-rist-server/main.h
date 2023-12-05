#pragma once

#include <atomic>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>


#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <gst/video/video.h>

#include <ristreceiver.h>
#include "gst/gstmessage.h"
#include "rpc/server.h"

namespace nrs
{
/* Structure to contain all our information, so we can pass it to callbacks */
struct App
{
  std::string pipeline_str;
  GstElement *datasrc_pipeline;
  GstElement *video_src;
  GstBus* bus;

  std::atomic_bool is_playing = false;
  gboolean debug = false;

  std::future<void> rist_receive_future;
  std::future<void> gstreamer_src_future;
  std::future<void> gstreamer_bus_future;
};

struct Config
{
  std::string rist_input_address = "";
  std::string rtmp_output_address = "";
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
  MSGPACK_DEFINE_ARRAY(bitrate,   rist_output_address,   rist_output_buffer_min,   rist_output_buffer_max,   rist_output_rtt_min,   rist_output_rtt_max,   rist_output_reorder_buffer,   rist_output_bandwidth,   rtmp_address,   rtmp_key);
};

void log();

void run_rpc_server();
void rpc_call_start(RpcData data);
void rpc_call_stop();

void start_gstreamer(RpcData &data);
void start_rist(RpcData &data);

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
}  // namespace nrs