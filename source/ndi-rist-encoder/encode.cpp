#include "common.h"
#include <fmt/core.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <atomic>
#include <thread>
#include <future>
#include <utility>

using std::string;

class Encode {

    public:
        Config* config;
        std::atomic_bool is_playing  {false};
        std::atomic_bool is_running  {false};
        std::atomic_bool is_eos  {false};
        std::future<void> encode_thread_future;
        std::function<void(const char* logMessage)> log_func = nullptr;
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
        string pipeline_str { "" };
        GstElement* datasrc_pipeline;
        GstElement *video_sink, *video_encoder;
        GMainLoop* loop;
        void pipeline_build();
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
        gboolean handle_gstreamer_bus_message(GstBus* bus, GstMessage* message);
};

void Encode::pipeline_build_source() {
    this->pipeline_str = fmt::format("ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux ",
      this->config.ndi_input_name);
}

void Encode::pipeline_build_sink() {
    this->pipeline_str += " appsink buffer-list=false wait-on-eos=false sync=true name=video_sink  "
      "mpegtsmux name=tsmux ! tsparse alignment=7 ! video_sink. ";
}

void Encode::pipeline_build_video_demux() {
    this->pipeline_str += " demux.video ! queue ! videoconvert ";
}

void Encode::pipeline_build_audio_demux() {
    this->pipeline_str += " demux.audio ! queue ! audioresample ! audioconvert ";
}

void Encode::pipeline_build_audio_encoder() {
    this->pipeline_str += " avenc_aac ! aacparse ";
}

void Encode::pipeline_build_video_encoder() {
    switch (this->config->encoder)
    {
    case "amd":
        pipeline_build_amd_encoder();
        break;

    case "qsv":
        pipeline_build_qsv_encoder();
        break;

    case "nvenc":
        pipeline_build_nvenc_encoder();
        break;
    
    default:
        pipeline_build_software_encoder();
        break;
    }   
}

void Encode::pipeline_build_amd_encoder() {
    switch (this->config->codec)
        {
        case "h265":
            pipeline_build_amd_h265_encoder();
            break;

        case "av1":
            pipeline_build_amd_av1_encoder();
            break;
        
        default:
            pipeline_build_amd_h264_encoder();
            break;
        }
}

void Encode::pipeline_build_qsv_encoder() {
    switch (this->config->codec)
        {
        case "h265":
            pipeline_build_qsv_h265_encoder();
            break;

        case "av1":
            pipeline_build_qsv_av1_encoder();
            break;
        
        default:
            pipeline_build_qsv_h264_encoder()
            break;
        }
}

void Encode::pipeline_build_nvenc_encoder() {
    switch (this->config->codec)
        {
        case "h265":
            pipeline_build_nvenc_h265_encoder();
            break;

        case "av1":
            pipeline_build_nvenc_av1_encoder();
            break;
        
        default:
            pipeline_build_nvenc_h264_encoder();
            break;
        }
}

void Encode::pipeline_build_software_encoder() {
    switch (this->config->codec)
        {
        case "h265":
            pipeline_build_software_h265_encoder();
            break;

        case "av1":
            pipeline_build_software_av1_encoder();
            break;
        
        default:
            pipeline_build_software_h264_encoder();
            break;
        }
}


void Encode::pipeline_build_amd_h264_encoder() {
    this->pipeline_str += fmt::format(
          "amfh264enc name=vidEncoder  bitrate={} rate-control=cbr "
          "usage=low-latency ! video/x-h264,framerate=60/1,profile=high ! "
          "h264parse ",
          config.bitrate);
}

void Encode::pipeline_build_amd_h265_encoder() {
    this->pipeline_str += fmt::format(
          "amfh265enc name=vidEncoder bitrate={} rate-control=cbr "
          "usage=low-latency ! video/x-h265,framerate=60/1 ! h265parse ",
          config.bitrate);
}

void Encode::pipeline_build_amd_av1_encoder() {
    this->pipeline_str += fmt::format(
          "amfav1enc name=vidEncoder bitrate={} rate-control=cbr "
          "usage=low-latency preset=high-quality ! video/x-av1,framerate=60/1 "
          "! av1parse ",
          config.bitrate);
}


void Encode::pipeline_build_qsv_h264_encoder() {
    this->pipeline_str += fmt::format(
          "qsvh264enc name=vidEncoder  bitrate={} rate-control=cbr "
          "target-usage=1 ! h264parse ",
          config.bitrate);
}

void Encode::pipeline_build_qsv_h265_encoder() {
    this->pipeline_str += fmt::format(
          "qsvh265enc name=vidEncoder bitrate={} rate-control=cbr "
          "target-usage=1 ! h265parse ",
          config.bitrate);
}

void Encode::pipeline_build_qsv_av1_encoder() {
    this->pipeline_str += fmt::format(
          "qsvav1enc name=vidEncoder bitrate={} rate-control=cbr "
          "target-usage=1 ! av1parse ",
          config.bitrate);
}


void Encode::pipeline_build_nvenc_h264_encoder() {
    this->pipeline_str += fmt::format(
          "nvh264enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! h264parse ",
          config.bitrate);
}

void Encode::pipeline_build_nvenc_h265_encoder() {
    this->pipeline_str += fmt::format(
          "nvh265enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! h265parse ",
          config.bitrate);
}

void Encode::pipeline_build_nvenc_av1_encoder() {
    this->pipeline_str += fmt::format(
          "nvav1enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! av1parse ",
          config.bitrate);
}


void Encode::pipeline_build_software_h264_encoder() {
    this->pipeline_str += fmt::format(
          "x264enc name=vidEncoder speed-preset=fast tune=zerolatency "
          "bitrate={} ! h264parse ",
          config.bitrate);
}

void Encode::pipeline_build_software_h265_encoder() {
    this->pipeline_str += fmt::format(
          "x265enc name=vidEncoder bitrate={} "
          "speed-preset=fast tune=zerolatency ! h265parse ",
          config.bitrate);
}

void Encode::pipeline_build_software_av1_encoder() {
    this->pipeline_str += fmt::format(
          "rav1enc name=vidEncoder bitrate={} speed-preset=8 tile-cols=2 "
          "tile-rows=2 ! av1parse ",
          config.bitrate);
}


void Encode::pipeline_build_audio_payloader() {
    this->pipeline_str += "queue ! tsmux. ";
}

void Encode::pipeline_build_video_payloader() {
    this->pipeline_str += "queue ! tsmux. ";
}

void Encode::pipeline_build() {
    this->pipeline_build_source();
    this->pipeline_build_sink();
    this->pipeline_build_audio_demux();
    this->pipeline_build_audio_encoder();
    this->pipeline_build_audio_payloader();
    this->pipeline_build_video_demux();
    this->pipeline_build_video_encoder();
    this->pipeline_build_video_payloader();
}

void Encode::parse_pipeline() {
    this->datasrc_pipeline = NULL;
    GError* error = NULL;
    GstBus* datasrc_bus;

    this->datasrc_pipeline = gst_parse_launch(this->pipeline_str.c_str(), &error);
    if (error) {
        this->log_func(fmt::format("Parse Error: {}", error->message));
        g_clear_error(&error);
    }
    if (this->datasrc_pipeline == NULL) {
        this->log_func("*** Bad datasrc_pipeline ***\n");
    }

    this->video_encoder =
      gst_bin_get_by_name(GST_BIN(this->datasrc_pipeline), "vidEncoder");

    GstBus* datasrc_bus = gst_element_get_bus(this->datasrc_pipeline);
    /* add watch for messages */
    gst_bus_add_watch(
        datasrc_bus, (GstBusFunc)this->handle_gstreamer_bus_message);
    gst_object_unref(datasrc_bus);
}

void Encode::play_pipeline() {
    g_main_loop_run(this->loop);
}

void Encode::run_encode_thread() {
    this->is_eos = FALSE;
    this->loop = g_main_loop_new(NULL, FALSE);
    this->is_playing = true;
    this->log_func("Playing pipeline.\n");
    gst_element_set_state(this->datasrc_pipeline, GST_STATE_PLAYING);
    this->encode_thread_future = std::async(std::launch::async, this->&play_pipeline);
}

void Encode::stop_encode_thread() {
    this->is_playing = false;
    gst_element_set_state(this->datasrc_pipeline, GST_STATE_NULL);
    this->log_func("Stopping pipeline.\n");
    
    std::future_status status;

    switch (status = this->encode_thread_future.wait_for(std::chrono::seconds(1)); status)
        {
            case std::future_status::timeout:
                logAppend("Waiting for encoder stop has timed out.");
                break;
            case std::future_status::ready:
                logAppend("Encoder stopped.");
                break;
        }
}

void Encode::handle_gst_message_error(GstMessage* message) {
    GError* err;
    gchar* debug_info;
    gst_message_parse_error(message, &err, &debug_info);
    this->log_func("\nReceived error from datasrc_pipeline...\n");
    this->log_func(fmt::format("Error received from element {}: {}\n",
                            GST_OBJECT_NAME(message->src),
                            err->message));
    this->log_func(fmt::format("Debugging information: {}\n",
                            debug_info ? debug_info : "none"));
    g_clear_error(&err);
    g_free(debug_info);
}

void Encode::handle_gst_message_eos(GstMessage* message) {
    this->log_func("\nReceived EOS from pipeline...\n");
}

gboolean Encode::handle_gstreamer_bus_message(GstBus* bus, GstMessage* message)
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

BufferDataStruct Encode::pull_buffer() {
    GstSample* sample;
    GstBuffer* buffer;

    sample = gst_app_sink_pull_sample(GST_APP_SINK(this->video_sink));
    buffer = gst_sample_get_buffer(sample);
    if (buffer)
    {
      GstMapInfo info;
      gst_buffer_map(buffer, &info, GST_MAP_READ);
      BufferDataStruct buffer_data;
        buffer_data.buf_size = info.size;
        buffer_data.buf_data = info.data;
        return buffer_data;

    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // wait 1/4 of 60fps frametime
    }
    gst_sample_unref(sample);
}

void Encode::set_encode_bitrate(int new_bitrate) {
    g_object_set(G_OBJECT(this->video_encoder), "bitrate", new_bitrate, NULL);
}
