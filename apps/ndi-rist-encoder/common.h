#pragma once

#include <string>
#include <gst/gst.h>

enum class Codec {
  h264,
  h265,
  av1
};

enum class Encoder {
  amd,
  qsv,
  nvenc,
  software
};
struct Config
{
  std::string ndi_input_name;
  Codec codec = Codec::h264;
  Encoder encoder = Encoder::software;
  std::string transport = "m2ts";
  std::string bitrate = "4300";
  std::string rist_output_address = "127.0.0.1:5000";
  std::string rist_output_buffer_min = "245";
  std::string rist_output_buffer_max = "5000";
  std::string rist_output_rtt_min = "40";
  std::string rist_output_rtt_max = "500";
  std::string rist_output_reorder_buffer = "240";
  std::string rist_output_bandwidth = "6000";
  std::string rtmp_address = "sydney.restream.io/live";
  std::string rtmp_key = "";
  std::string reencode_bitrate = "18000";
  uint16_t use_rpc_control = 1;
  bool upscale = true;
};

struct BufferDataStruct
    {
        gsize buf_size {0};
        uint8_t* buf_data;
    };
