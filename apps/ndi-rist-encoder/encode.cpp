#include "encode.h"

using std::string;

void Encode::pipeline_build_source() {
    this->pipeline_str = fmt::format("ndisrc do-timestamp=true ndi-name=\"{}\" ! ndisrcdemux name=demux ",
      this->config->ndi_input_name);
}

void Encode::pipeline_build_sink() {
    this->pipeline_str += " udpsink host=127.0.0.1 port=6000 sync=true name=video_sink  "
      "mpegtsmux name=tsmux ! rtpmp2tpay ! video_sink. ";
}

void Encode::pipeline_build_video_demux() {
    this->pipeline_str += " demux.video ! queue ! videoconvert !";
}

void Encode::pipeline_build_audio_demux() {
    this->pipeline_str += " demux.audio ! queue ! audioresample ! audioconvert !";
}

void Encode::pipeline_build_audio_encoder() {
    this->pipeline_str += " avenc_aac ! aacparse ";
}

void Encode::pipeline_build_video_encoder() {
    switch (this->config->encoder)
    {
    case Encoder::amd:
        pipeline_build_amd_encoder();
        break;

    case Encoder::qsv:
        pipeline_build_qsv_encoder();
        break;

    case Encoder::nvenc:
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
        case Codec::h265:
            pipeline_build_amd_h265_encoder();
            break;

        case Codec::av1:
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
        case Codec::h265:
            pipeline_build_qsv_h265_encoder();
            break;

        case Codec::av1:
            pipeline_build_qsv_av1_encoder();
            break;
        
        default:
            pipeline_build_qsv_h264_encoder();
            break;
        }
}

void Encode::pipeline_build_nvenc_encoder() {
    switch (this->config->codec)
        {
        case Codec::h265:
            pipeline_build_nvenc_h265_encoder();
            break;

        case Codec::av1:
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
        case Codec::h265:
            pipeline_build_software_h265_encoder();
            break;

        case Codec::av1:
            pipeline_build_software_av1_encoder();
            break;
        
        default:
            pipeline_build_software_h264_encoder();
            break;
        }
}


void Encode::pipeline_build_amd_h264_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("amfh264enc name=vidEncoder  bitrate={} rate-control=cbr usage=low-latency preset=quality pre-encode=true pa-hqmb-mode=auto ! video/x-h264,framerate=60/1,profile=high ! h264parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_amd_h265_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("amfh265enc name=vidEncoder bitrate={} rate-control=cbr "
          "usage=low-latency preset=quality pre-encode=true pa-hqmb-mode=auto ! video/x-h265,framerate=60/1 ! h265parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_amd_av1_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("amfav1enc name=vidEncoder bitrate={} rate-control=cbr "
          "usage=low-latency preset=high-quality  pre-encode=true pa-hqmb-mode=auto ! video/x-av1,framerate=60/1 "
          "! av1parse "),
          this->config->bitrate);
}


void Encode::pipeline_build_qsv_h264_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("qsvh264enc name=vidEncoder  bitrate={} rate-control=cbr "
          "target-usage=1 ! h264parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_qsv_h265_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("qsvh265enc name=vidEncoder bitrate={} rate-control=cbr "
          "target-usage=1 ! h265parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_qsv_av1_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("qsvav1enc name=vidEncoder bitrate={} rate-control=cbr "
          "target-usage=1 ! av1parse "),
          this->config->bitrate);
}


void Encode::pipeline_build_nvenc_h264_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("nvh264enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! h264parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_nvenc_h265_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("nvh265enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! h265parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_nvenc_av1_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("nvav1enc name=vidEncoder bitrate={} rc-mode=cbr-hq "
          "preset=low-latency-hq ! av1parse "),
          this->config->bitrate);
}


void Encode::pipeline_build_software_h264_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("x264enc name=vidEncoder speed-preset=fast tune=zerolatency "
          "bitrate={} ! h264parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_software_h265_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("x265enc name=vidEncoder bitrate={} "
          "speed-preset=fast tune=zerolatency ! h265parse "),
          this->config->bitrate);
}

void Encode::pipeline_build_software_av1_encoder() {
    this->pipeline_str += fmt::format(
          fmt::runtime("rav1enc name=vidEncoder bitrate={} speed-preset=8 tile-cols=2 "
          "tile-rows=2 ! av1parse "),
          this->config->bitrate);
}


void Encode::pipeline_build_audio_payloader() {
    this->pipeline_str += "! queue ! tsmux. ";
}

void Encode::pipeline_build_video_payloader() {
    this->pipeline_str += "! queue ! tsmux. ";
}

void Encode::build_pipeline() {
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

    this->log_func(this->pipeline_str);

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
    this->video_sink =
      gst_bin_get_by_name(GST_BIN(this->datasrc_pipeline), "video_sink");

    this->bus = gst_element_get_bus(this->datasrc_pipeline);
} 

void Encode::play_pipeline() {
    do {
    GstMessage *msg = gst_bus_timed_pop (this->bus, -1);
    this->handle_gstreamer_message(msg);
    gst_message_unref(msg);
  } while (this->is_playing);
}

void Encode::run_encode_thread() {
    this->build_pipeline();
    this->parse_pipeline();
    this->is_eos = FALSE;
    this->is_playing = true;
    this->log_func("Playing pipeline.\n");
    gst_element_set_state(this->datasrc_pipeline, GST_STATE_PLAYING);
    this->encode_thread_future = std::async(std::launch::async, &Encode::play_pipeline, this);
}

void Encode::stop_encode_thread() {
    this->is_playing = false;
    gst_element_set_state(this->datasrc_pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(this->datasrc_pipeline));
    gst_object_unref(this->bus);
    this->log_func("Stopping pipeline.\n");
    
    std::future_status status;

    switch (status = this->encode_thread_future.wait_for(std::chrono::seconds(1)); status)
        {
            case std::future_status::timeout:
                this->log_func("Waiting for encoder stop has timed out.");
                break;
            case std::future_status::ready:
                this->log_func("Encoder stopped.");
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
    this->is_playing = false;
}

void Encode::handle_gst_message_eos(GstMessage* message) {
    this->log_func("\nReceived EOS from pipeline...\n");
    this->is_playing = false;
}

void Encode::handle_gstreamer_message(GstMessage* message)
{
  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
      this->handle_gst_message_error(message);
      break;
    case GST_MESSAGE_EOS:
      this->handle_gst_message_eos(message);
      break;
    default:
      break;
  }
}

BufferDataStruct Encode::pull_buffer(){
    GstSample* sample;
    GstBuffer* buffer;
    BufferDataStruct buffer_data;

    sample = gst_app_sink_pull_sample(GST_APP_SINK(this->video_sink));
    buffer = gst_sample_get_buffer(sample);
    if (buffer)
    {
      GstMapInfo info;
      gst_buffer_map(buffer, &info, GST_MAP_READ);
      
        buffer_data.buf_size = info.size;
        buffer_data.buf_data = info.data;
    }
    gst_sample_unref(sample);
    return buffer_data;
}

void Encode::set_encode_bitrate(int new_bitrate) {
    g_object_set(G_OBJECT(this->video_encoder), "bitrate", new_bitrate, NULL);
}
