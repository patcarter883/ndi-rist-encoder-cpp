#pragma once

#include <atomic>
#include <future>
#include <thread>
#include <utility>

#include <fmt/core.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include "common.h"

class Encode
{
public:
  const Config& config;
  std::atomic_bool& is_running;
  std::atomic_bool& is_eos;
  std::future<void> encode_thread_future;
  void (*log_func)(std::string logMessage);
  GstElement* video_sink = nullptr;
  void run_encode_thread();
  void stop_encode_thread();
  BufferDataStruct pull_buffer();
  void set_encode_bitrate(int newBitrate);
  Encode(Config& config, std::atomic_bool& is_running, std::atomic_bool& is_eos, void (*log_func)(std::string logMessage)) :
	  config{ config },
	  is_running{ is_running },
	  is_eos{ is_eos },
	  log_func{ log_func }
  {}

private:
  std::string pipeline_str;
  GstElement* datasrc_pipeline = nullptr;
  GstElement* video_encoder = nullptr;
  GstBus* bus = nullptr;
  void build_pipeline();
  void pipeline_build_source();
  void pipeline_build_sink();
  void pipeline_build_video_demux();
  void pipeline_build_audio_demux();
  void pipeline_build_audio_encoder();
  void pipeline_build_video_encoder();
  void pipeline_build_amd_encoder();
  void pipeline_build_qsv_encoder();
  void pipeline_build_nvenc_encoder();
  void pipeline_build_software_encoder();
  void pipeline_build_amd_h264_encoder();
  void pipeline_build_amd_h265_encoder();
  void pipeline_build_amd_av1_encoder();
  void pipeline_build_qsv_h264_encoder();
  void pipeline_build_qsv_h265_encoder();
  void pipeline_build_qsv_av1_encoder();
  void pipeline_build_nvenc_h264_encoder();
  void pipeline_build_nvenc_h265_encoder();
  void pipeline_build_nvenc_av1_encoder();
  void pipeline_build_software_h264_encoder();
  void pipeline_build_software_h265_encoder();
  void pipeline_build_software_av1_encoder();
  void pipeline_build_audio_payloader();
  void pipeline_build_video_payloader();
  void parse_pipeline();
  void play_pipeline();
  void handle_gst_message_error(GstMessage* message);
  void handle_gst_message_eos(GstMessage* message);
  void handle_gstreamer_message(GstMessage* message);
};