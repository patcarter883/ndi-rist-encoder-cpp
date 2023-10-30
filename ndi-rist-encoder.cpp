// gst-launch-1.0 -v multiqueue name=demuxq multiqueue name=outq udpsrc port=6000 caps="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" ! demuxq.sink_0 demuxq.src_0 ! rtph264depay ! h264parse ! queue ! avdec_h264 ! videoconvert ! outq.sink_0 outq.src_0 ! autovideosink udpsrc port=6002 caps="application/x-rtp, media=(string)audio, clock-rate=(int)48000, encoding-name=(string)MPEG4-GENERIC, encoding-params=(string)2, streamtype=(string)5, profile-level-id=(string)2, mode=(string)AAC-hbr, config=(string)119056e500, sizelength=(string)13, indexlength=(string)3, indexdeltalength=(string)3" ! demuxq.sink_1 demuxq.src_1 ! rtpmp4gdepay ! aacparse ! queue ! avdec_aac ! audioconvert ! outq.sink_1 outq.src_1 ! autoaudiosink

// ndi-rist-encoder.cpp : Defines the entry point for the application.
//
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <config.h>
#if defined(HAVE_PTHREAD) || defined(_WIN32)
#include "ndi-rist-encoder.h"
#include <FL/Fl.H>
#include "UserInterface.cxx"
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include <threads.h>
#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

UserInterface *ui = new UserInterface;
Fl_Text_Buffer *logBuff = new Fl_Text_Buffer();

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _App App;

struct _App
{
	GstElement *datasrc_pipeline;
	GstElement *filesrc, *parser, *decoder, *videosink, *audiosink, *appsink, *fallbackVideo, *fallbackAudio;
	GstDeviceMonitor *monitor;

	gboolean is_eos;
	GMainLoop *loop;

	RISTNetSender ristSender;

	gboolean isPlaying = false;
};

typedef struct _Config Config;

struct _Config
{
	guint width;
	guint height;
	guint framerate;

	std::string ndi_input_name;
	std::string codec = "h264";
	std::string bitrate = "5000";
	std::string rist_output_address = "127.0.0.1";
	std::string rist_output_port = "5000";
	std::string rist_output_buffer;
	std::string rist_output_bandwidth;
};

/* Globals */
Config config;
App app;

std::thread findNdiThread;
std::thread previewThread;
std::thread encodeThread;
std::thread ristThread;
std::thread ristVideoThread;
std::thread ristAudioThread;

int main(int argc, char **argv)
{
	/* init GStreamer */
	gst_init(&argc, &argv);

	Fl::lock();
	ui->logDisplay->buffer(logBuff);
	ui->show(argc, argv);

	findNdiThread = std::thread(findNdiDevices, ui);

	int result = Fl::run();

	if (findNdiThread.joinable())
	{
		findNdiThread.join();
	}

	if (previewThread.joinable())
	{
		previewThread.join();
	}

	if (encodeThread.joinable())
	{
		encodeThread.join();
	}

	if (ristThread.joinable())
	{
		ristThread.join();
	}

	return result;
}

void logAppend(std::string logMessage)
{
	Fl::lock();
	ui->logDisplay->insert(logMessage.c_str());
	ui->logDisplay->insert("\n");
	Fl::unlock();
	Fl::awake();
}

static uint64_t risttools_convertVideoRTPtoNTP(uint32_t i_rtp)
{
	uint64_t i_ntp;
	int32_t clock = 90000;
	i_ntp = (uint64_t)i_rtp << 32;
	i_ntp /= clock;
	return i_ntp;
}

static uint64_t risttools_convertAudioRTPtoNTP(uint32_t i_rtp)
{
	uint64_t i_ntp;
	int32_t clock = 48000;
	i_ntp = (uint64_t)i_rtp << 32;
	i_ntp /= clock;
	return i_ntp;
}

void sendBufferToRist(GstBuffer *buffer)
{
	GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
	GstMemory *memory;
	GstMapInfo info;

	gst_buffer_map(buffer, &info, GST_MAP_READ);
	gsize &buf_size = info.size;
	guint8 *&buf_data = info.data;
	auto streamId = NULL;
	if (gst_rtp_buffer_map(buffer, GST_MAP_READ, &rtp))
	{
			guint8 rtp_payloadType = gst_rtp_buffer_get_payload_type(&rtp);
	guint32 rtp_ts = gst_rtp_buffer_get_timestamp(&rtp);
	
	switch (rtp_payloadType)
	{
	case 97:
		streamId = 2000;
		break;

	case 96:
		streamId = 1000;
		break;
	
	default:
		break;
	}
	gst_rtp_buffer_unmap(&rtp);
	}
	
	app.ristSender.sendData(buf_data, buf_size, 0,
							streamId);

	
	gst_buffer_unmap(buffer, &info);
}

static GstFlowReturn
on_new_sample_from_sink(GstElement *elt, App *app)
{
	GstSample *sample;
	GstBuffer *buffer;

	/* get the sample from appsink */
	sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
	buffer = gst_sample_get_buffer(sample);
	if (buffer)
	{
		sendBufferToRist(buffer);
		// logAppend("\nPulled no buffers. Exiting...\n");
		// return GST_FLOW_EOS;
	}

	gst_sample_unref(sample);
	return GST_FLOW_OK;
}

static gboolean
datasrc_message(GstBus *bus, GstMessage *message, App *app)
{
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE(message))
	{
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(message, &err, &debug_info);
		logAppend("\nReceived error from datasrc_pipeline...\n");
		logAppend(fmt::format("Error received from element {}: {}\n",
							  GST_OBJECT_NAME(message->src), err->message));
		logAppend(fmt::format("Debugging information: {}\n",
							  debug_info ? debug_info : "none"));
		g_clear_error(&err);
		g_free(debug_info);
		if (g_main_loop_is_running(app->loop))
			g_main_loop_quit(app->loop);
		break;
	case GST_MESSAGE_EOS:
		logAppend("\nReceived EOS from datasrc_pipeline...\n");
		app->is_eos = TRUE;
		break;
	default:
		// logAppend(fmt::format("\n{} received from element {}\n",
		// GST_MESSAGE_TYPE_NAME(message), GST_OBJECT_NAME(message->src)));
		break;
	}
	// gst_debug_bin_to_dot_file(GST_BIN(app->datasrc_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline-debug");
	return TRUE;
}

void *startStream(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstBus *datasrc_bus;
	GstPad *pad;
	std::string datasrc_pipeline_str = "";
	GError *error = NULL;
	GOptionContext *context;

	// Generate a vector of RIST URL's,  ip(name), ports, RIST URL output, listen(true) or send mode (false)
	std::string lURL;
	std::vector<std::tuple<std::string, int>> interfaceListSender;
	if (RISTNetTools::buildRISTURL(config.rist_output_address, config.rist_output_port, lURL, false))
	{
		interfaceListSender.push_back(std::tuple<std::string, int>(lURL, 5));
	}

	// Populate the settings
	RISTNetSender::RISTNetSenderSettings mySendConfiguration;
	app.ristSender.initSender(interfaceListSender, mySendConfiguration);

	app.is_eos = FALSE;
	app.loop = g_main_loop_new(NULL, FALSE);

	datasrc_pipeline_str += fmt::format("multiqueue name=demuxq multiqueue name=outq appsink name=videosink appsink name=audiosink rtpbin name=rtpbin ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux ",
														  config.ndi_input_name);
	datasrc_pipeline_str += "demux.audio ! demuxq.sink_0 demuxq.src_0 ! audioresample ! audioconvert ! avenc_aac ! aacparse ! rtpmp4gpay pt=97 ! rtpbin.send_rtp_sink_1 rtpbin.send_rtp_src_1 ! outq.sink_0 outq.src_0 ! audiosink. ";


	if (config.codec == "h264")
	{
		datasrc_pipeline_str += fmt::format("demux.video ! demuxq.sink_1 demuxq.src_1 ! videoconvert ! video/x-raw,format=I420 ! x264enc bitrate={} speed-preset=fast tune=zerolatency ! h264parse ! rtph264pay config-interval=1 aggregate-mode=max-stap pt=96 ! rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! outq.sink_1 outq.src_1 ! videosink. ", config.bitrate);
	}
	else if (config.codec == "h265")
	{
		datasrc_pipeline_str += fmt::format("demux.video ! queue ! videoconvert ! video/x-raw,format=I420 ! x265enc bitrate={} sliced-threads=true speed-preset=fast tune=zerolatency ! h265parse ! rtph265pay config-interval=1 aggregate-mode=max pt=96 ! queue ! rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! queue ! videosink. ", config.bitrate);
	}
	else if (config.codec == "av1")
	{	
		datasrc_pipeline_str += fmt::format("demux.video ! queue ! videoconvert ! av1enc bitrate={} cpu-used=9 usage-profile=realtime tile-columns=2 tile-rows=2 ! av1parse ! rtpav1pay ! queue ! rtpbin.send_rtp_sink_0  rtpbin.send_rtp_src_0 ! queue ! videosink. ", config.bitrate);
	}

	logAppend(datasrc_pipeline_str.c_str());

	app.datasrc_pipeline = gst_parse_launch(datasrc_pipeline_str.c_str(), NULL);
	if (app.datasrc_pipeline == NULL)
	{
		logAppend("*** Bad datasrc_pipeline ***\n");
	}

	datasrc_bus = gst_element_get_bus(app.datasrc_pipeline);

	/* add watch for messages */
	gst_bus_add_watch(datasrc_bus, (GstBusFunc)datasrc_message, &app);
	gst_object_unref(datasrc_bus);

	app.fallbackVideo = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "fallbackvideo");
	app.fallbackAudio = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "fallbackaudio");


	/* set up appsink */
	app.videosink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "videosink");
	g_object_set(G_OBJECT(app.videosink), "emit-signals", TRUE, "sync", FALSE,
				 NULL);
	g_signal_connect(app.videosink, "new-sample",
					 G_CALLBACK(on_new_sample_from_sink), &app);

	/* set up appsink */
	app.audiosink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "audiosink");
	g_object_set(G_OBJECT(app.audiosink), "emit-signals", TRUE, "sync", FALSE,
				 NULL);
	g_signal_connect(app.audiosink, "new-sample",
					 G_CALLBACK(on_new_sample_from_sink), &app);
	
	logAppend("Changing state of datasrc_pipeline to PLAYING...\n");

	/* only start datasrc_pipeline to ensure we have enough data before
	 * starting datasink_pipeline
	 */

	gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);
	app.isPlaying = true;
	g_main_loop_run(app.loop);
	app.isPlaying = false;
	logAppend("stopping...\n");

	gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);

	g_main_loop_unref(app.loop);

	return 0L;
}

void *previewNDISource(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstElement *pipeline, *ndisrc, *ndidemux, *conv, *videosink;
	GstBus *bus;
	GstMessage *msg;

	std::string pipelineString = fmt::format("ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux   demux.video ! queue ! videoconvert ! autovideosink  demux.audio ! queue ! audioconvert ! autoaudiosink", config.ndi_input_name);
	pipeline = gst_parse_launch(pipelineString.c_str(), NULL);
	bus = gst_element_get_bus(pipeline);

	logAppend(pipelineString);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	msg =
		gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
								   (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
	/* Parse message */
	if (msg != NULL)
	{
		GError *err;
		gchar *debug_info;

		switch (GST_MESSAGE_TYPE(msg))
		{
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &err, &debug_info);
			logAppend(fmt::format("Error received from element {}: {}\n",
								  GST_OBJECT_NAME(msg->src), err->message));
			logAppend(fmt::format("Debugging information: {}\n",
								  debug_info ? debug_info : "none"));
			g_clear_error(&err);
			g_free(debug_info);
			break;
		case GST_MESSAGE_EOS:
			logAppend("End-Of-Stream reached.\n");
			break;
		default:
			/* We should not reach here because we only asked for ERRORs and EOS */
			logAppend("Unexpected message received.\n");
			break;
		}
		gst_message_unref(msg);
	}

	/* Free resources */
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);

	return 0L;
}

void ndi_source_select_cb(Fl_Widget *o, void *v)
{
	config.ndi_input_name = (char *)v;
}

void preview_cb(Fl_Button *o, void *v)
{
	if (previewThread.joinable())
	{
		previewThread.join();
	}
	previewThread = std::thread(previewNDISource, ui);
}

void streamSource_cb(Fl_Button*, void*) {
	auto *audioPad = gst_element_get_static_pad(app.fallbackAudio, "sink_0");
	auto *videoPad = gst_element_get_static_pad(app.fallbackVideo, "sink_0");

	g_object_set(G_OBJECT(app.fallbackAudio), "active-pad", audioPad, NULL);
	g_object_set(G_OBJECT(app.fallbackVideo), "active-pad", videoPad, NULL);

	gst_object_unref(audioPad);
	gst_object_unref(videoPad);
}

void streamStandby_cb(Fl_Button*, void*) {
	auto *audioPad = gst_element_get_static_pad(app.fallbackAudio, "sink_1");
	auto *videoPad = gst_element_get_static_pad(app.fallbackVideo, "sink_1");

	g_object_set(G_OBJECT(app.fallbackAudio), "active-pad", audioPad, NULL);
	g_object_set(G_OBJECT(app.fallbackVideo), "active-pad", videoPad, NULL);

	gst_object_unref(audioPad);
	gst_object_unref(videoPad);
}

void startStream_cb(Fl_Button *o, void *v)
{
	if (encodeThread.joinable())
	{
		encodeThread.join();
	}

	encodeThread = std::thread(startStream, ui);
	Fl::lock();
	o->deactivate();
	ui->btnStopStream->activate();
	Fl::unlock();
	Fl::awake();
}

void stopStream_cb(Fl_Button *o, void *v)
{
	if (g_main_loop_is_running(app.loop))
		g_main_loop_quit(app.loop);
	Fl::lock();
	o->deactivate();
	ui->btnStartStream->activate();
	Fl::unlock();
	Fl::awake();
}

void addDevice(gpointer devicePtr, gpointer p)
{
	GstDevice *device = (GstDevice *)devicePtr;

	char *newDeviceDisplayName = gst_device_get_display_name(device);
	char *newDeviceNdiName = const_cast<char *>(gst_structure_get_string(gst_device_get_properties(device), "ndi-name"));

	ui->ndiSourceSelect->add(newDeviceDisplayName, 0, ndi_source_select_cb, newDeviceDisplayName, 0);
	gst_object_unref(device);
};

void refreshSources_cb(Fl_Button *o, void *v)
{
	GList *devices = gst_device_monitor_get_devices(app.monitor);

	ui->ndiSourceSelect->clear();
	ui->btnRefreshSources->deactivate();

	g_list_foreach(devices, addDevice, NULL);

	ui->btnRefreshSources->activate();
}

void select_codec_cb(Fl_Menu_ *o, void *v)
{
	config.codec = (char *)v;
}

void rist_address_cb(Fl_Input *o, void *v)
{
	config.rist_output_address = o->value();
}

void rist_port_cb(Fl_Input *o, void *v)
{
	config.rist_output_port = o->value();
}

void rist_buffer_cb(Fl_Input *o, void *v)
{
	config.rist_output_buffer = o->value();
}

void rist_bandwidth_cb(Fl_Input *o, void *v)
{
	config.rist_output_bandwidth = o->value();
}

void *findNdiDevices(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstBus *bus;
	GstCaps *caps;

	app.monitor = gst_device_monitor_new();
	bus = gst_device_monitor_get_bus(app.monitor);
	caps = gst_caps_new_empty_simple("application/x-ndi");
	gst_device_monitor_add_filter(app.monitor, "Video/Source", caps);
	gst_caps_unref(caps);

	gst_device_monitor_start(app.monitor);

	auto start = std::chrono::system_clock::now();
	auto end = std::chrono::system_clock::now();
	while ((std::chrono::duration_cast<std::chrono::seconds>(end - start).count() != 2))
	{
		GstMessage *msg = gst_bus_timed_pop_filtered(bus, 0, GST_MESSAGE_DEVICE_ADDED);
		if (msg)
		{
			GstDevice *newDevice;
			gst_message_parse_device_added(msg, &newDevice);
			char *newDeviceDisplayName = gst_device_get_display_name(newDevice);
			char *newDeviceNdiName = const_cast<char *>(gst_structure_get_string(gst_device_get_properties(newDevice), "ndi-name"));
			Fl::lock();
			ui->ndiSourceSelect->add(newDeviceDisplayName, 0, ndi_source_select_cb, newDeviceDisplayName, 0);
			Fl::unlock();
			Fl::awake();
			gst_object_unref(newDevice);
		}
		end = std::chrono::system_clock::now();
	}

	return 0L;
}
#endif // HAVE_PTHREAD || _WIN32