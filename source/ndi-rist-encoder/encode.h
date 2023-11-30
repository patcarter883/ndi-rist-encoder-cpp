#pragma once

#include "common.h"
#include <fmt/core.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <atomic>
#include <thread>
#include <future>
#include <utility>

class Encode {

    public:
        Config* config;
        std::atomic_bool is_playing;
        std::atomic_bool is_running;
        std::atomic_bool is_eos;
        std::future<void> encode_thread_future;
        std::function<void(std::string logMessage)> log_func;
        void run_encode_thread();
        void stop_encode_thread();
        Encode(Config* config) {
            this->config = config;
        }
        ~Encode() {
            gst_object_unref(GST_OBJECT(this->datasrc_pipeline));
            g_main_loop_unref(this->loop);
        }
        BufferDataStruct pull_buffer();
        void set_encode_bitrate(int newBitrate);

    private:
        std::string pipeline_str;
        GstElement* datasrc_pipeline;
        GstElement *video_sink, *video_encoder;
        GMainLoop* loop;
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
        gboolean handle_gstreamer_bus_message(GstBus* bus, GstMessage* message, gpointer user_data);
};