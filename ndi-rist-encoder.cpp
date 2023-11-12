// gst-launch-1.0 -v multiqueue name=outq udpsrc port=6000 caps="application/x-rtp, media=(string)application, clock-rate=(int)90000, encoding-name=(string)X-GST" ! rtpjitterbuffer ! rtpgstdepay ! h264parse ! queue ! avdec_h264 ! videoconvert ! outq.sink_0 outq.src_0 ! autovideosink udpsrc port=6002 caps="application/x-rtp, media=(string)application, clock-rate=(int)90000, encoding-name=(string)X-GST" ! rtpjitterbuffer ! rtpgstdepay ! aacparse ! queue ! avdec_aac ! audioconvert ! outq.sink_1 outq.src_1 ! autoaudiosink

// ndi-rist-encoder.cpp : Defines the entry point for the application.
//
// #ifdef _WIN32
// #include <Windows.h>
// #else
// #include <unistd.h>
// #endif
// #include <config.h>
#include "ndi-rist-encoder.h"
#include <FL/Fl.H>
#include "UserInterface.cxx"
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>
#include <utility>

UserInterface *ui = new UserInterface;
Fl_Text_Buffer *logBuff = new Fl_Text_Buffer();
Fl_Text_Buffer *ristLogBuff = new Fl_Text_Buffer();

typedef struct BufferListFuncData 
{
	RISTNetSender *sender;
	int streamId;
};

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _App App;

struct _App
{
	GstElement *datasrc_pipeline;
	GstElement *filesrc, *parser, *decoder, *videosink, *audiosink, *rtcp0sink, *rtcp1sink, *fallbackVideo, *fallbackAudio, *videoEncoder;
	GstDeviceMonitor *monitor;

	gboolean is_eos;
	gboolean isRunning;
	GMainLoop *loop;

	RISTNetSender *ristSender;

	gboolean isPlaying = false;

	int currentBitrate;
	double previousQuality;
};

typedef struct _Config Config;

struct _Config
{
	guint width;
	guint height;
	guint framerate;

	std::string ndi_input_name;
	std::string codec = "h264";
	std::string encoder = "software";
	std::string transport = "m2ts";
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

void initUi()
{
	ui->logDisplay->buffer(logBuff);
	ui->ristLogDisplay->buffer(ristLogBuff);
	ui->ristAddressInput->value(config.rist_output_address.c_str());
	ui->ristBandwidthInput->value(config.rist_output_bandwidth.c_str());
	ui->ristBufferInput->value(config.rist_output_buffer.c_str());
	ui->ristPortInput->value(config.rist_output_port.c_str());
	ui->encoderBitrateInput->value(config.bitrate.c_str());
	ui->show(NULL, NULL);
}

int main(int argc, char **argv)
{
	/* init GStreamer */
	gst_init(&argc, &argv);

	Fl::lock();
	initUi();

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

	return result;
}

void logAppend_cb(void *msgPtr)
{
	std::string * msgStr = static_cast<std::string*>(msgPtr);
	ui->logDisplay->insert(msgStr->c_str());
	ui->logDisplay->insert("\n");
	delete msgPtr;
	return;
}

void ristLogAppend_cb(void *msgPtr)
{
	std::string * msgStr = static_cast<std::string*>(msgPtr);
	ui->ristLogDisplay->insert(msgStr->c_str());
	delete msgPtr;
	return;
}

void logAppend(std::string logMessage)
{
	auto *logMessageCpy = new std::string;
	*logMessageCpy = strdup(logMessage.c_str());
	Fl::awake(logAppend_cb,logMessageCpy);
	return;
}



void ristLogAppend(const char * logMessage)
{
	auto *logMessageCpy = new std::string;
	*logMessageCpy = strdup(logMessage);
	Fl::awake(ristLogAppend_cb,logMessageCpy);
	return;
}

int ristLog(void *arg, enum rist_log_level, const char *msg)
{
	ristLogAppend(msg);
	return 1;
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

void stopStream()
{
	app.isRunning = false;
	if (g_main_loop_is_running(app.loop))
	{
		g_main_loop_quit(app.loop);
	}

	Fl::lock();
	ui->btnStopStream->deactivate();
	Fl::unlock();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	Fl::lock();
	ui->btnStartStream->activate();
	Fl::unlock();
	Fl::awake();
}

static gboolean sendBufferToRist(GstBuffer *buffer, RISTNetSender *sender, uint16_t streamId)
{
	GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
	GstMapInfo info;

	gst_buffer_map(buffer, &info, GST_MAP_READ);
	gsize &buf_size = info.size;
	guint8 *&buf_data = info.data;

	sender->sendData(buf_data, buf_size, 0, streamId);

	return true;
}

static gboolean sendBufferListToRist(GstBuffer **buffer, guint idx, gpointer userDataPtr)
{
	BufferListFuncData * userData = (BufferListFuncData*)userDataPtr;
	GstMapInfo info;

	gst_buffer_map(*buffer, &info, GST_MAP_READ);
	gsize &buf_size = info.size;
	guint8 *&buf_data = info.data;

	userData->sender->sendData(buf_data, buf_size, 0, userData->streamId);

	return true;
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
		logAppend("\nReceived EOS from pipeline...\n");
		stopStream();
		break;
	default:
		// logAppend(fmt::format("\n{} received from element {}\n",
		// GST_MESSAGE_TYPE_NAME(message), GST_OBJECT_NAME(message->src)));
		break;
	}
	// gst_debug_bin_to_dot_file(GST_BIN(app->datasrc_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline-debug");
	return TRUE;
}

void *runRistRtcp0(RISTNetSender *sender)
{

	GstSample *sample;
	GstBufferList *bufferlist;

	while (app.isRunning)
	{
		sample = gst_app_sink_pull_sample(GST_APP_SINK(app.rtcp0sink));
		bufferlist = gst_sample_get_buffer_list(sample);
		if (bufferlist)
		{
			BufferListFuncData user_data = {
						.sender = sender,
						.streamId = 2000
					};

			gst_buffer_list_foreach (bufferlist,
                         sendBufferListToRist,
                         &user_data);
		}

		gst_sample_unref(sample);
	}

	return 0L;
}

void *runRistRtcp1(RISTNetSender *sender)
{

	GstSample *sample;
	GstBufferList *bufferlist;

	while (app.isRunning)
	{
		sample = gst_app_sink_pull_sample(GST_APP_SINK(app.rtcp1sink));
		bufferlist = gst_sample_get_buffer_list(sample);
		if (bufferlist)
		{
			BufferListFuncData user_data = {
						.sender = sender,
						.streamId = 2002
					};

			gst_buffer_list_foreach (bufferlist,
                         sendBufferListToRist,
                         &user_data);
		}

		gst_sample_unref(sample);
	}

	return 0L;
}

void *runRistVideo(RISTNetSender *sender)
{

	GstSample *sample;
	GstBuffer *buffer;
	GstBufferList *bufferlist;

	while (app.isRunning)
	{
		sample = gst_app_sink_pull_sample(GST_APP_SINK(app.videosink));
		buffer = gst_sample_get_buffer(sample);
		if (buffer)
		{
			sendBufferToRist(buffer, sender, 1000);
		}
		// bufferlist = gst_sample_get_buffer_list(sample);
		// if (bufferlist)
		// {
		// 	BufferListFuncData user_data = {
		// 				.sender = sender,
		// 				.streamId = 1000
		// 			};

		// 	gst_buffer_list_foreach (bufferlist,
        //                  sendBufferListToRist,
        //                  &user_data);
		// }

		gst_sample_unref(sample);
	}

	return 0L;
}

void *runRistAudio(RISTNetSender *sender)
{

	GstSample *sample;
	GstBuffer *buffer;
	GstBufferList *bufferlist;

	while (app.isRunning)
	{
		/* get the sample from appsink */
		sample = gst_app_sink_pull_sample(GST_APP_SINK(app.audiosink));
		buffer = gst_sample_get_buffer(sample);
		if (buffer)
		{
			sendBufferToRist(buffer, sender, 1000);
		}
		// bufferlist = gst_sample_get_buffer_list(sample);
		// if (bufferlist)
		// {
		// 	BufferListFuncData user_data = {
		// 				.sender = sender,
		// 				.streamId = 1002
		// 			};

		// 	gst_buffer_list_foreach (bufferlist,
        //                  sendBufferListToRist,
        //                  &user_data);
		// }

		gst_sample_unref(sample);
	}
	return 0L;
}


void ristStatistics_cb(void *msgPtr)
{
	rist_stats statistics = *(rist_stats*)msgPtr;
	ui->bandwidthOutput->value(std::to_string(statistics.stats.sender_peer.bandwidth/1000).c_str());
	ui->linkQualityOutput->value(std::to_string(statistics.stats.sender_peer.quality).c_str());
	ui->totalPacketsOutput->value(std::to_string(statistics.stats.sender_peer.sent).c_str());
	ui->retransmittedPacketsOutput->value(std::to_string(statistics.stats.sender_peer.retransmitted).c_str());
	ui->rttOutput->value(std::to_string(statistics.stats.sender_peer.rtt).c_str());
	ui->encodeBitrateOutput->value(std::to_string(app.currentBitrate).c_str());
	delete msgPtr;
	return;
}

void gotRistStatistics(const rist_stats &statistics)
{

	if (app.previousQuality > 0 && (int)statistics.stats.sender_peer.quality != (int)app.previousQuality)
	{
		double qualDiffPct = statistics.stats.sender_peer.quality / app.previousQuality;
		int adjBitrate = (int)(app.currentBitrate * qualDiffPct);
		int bitrateDelta = adjBitrate - app.currentBitrate;

		int newBitrate = min(app.currentBitrate += bitrateDelta / 2, std::stoi(config.bitrate));
		app.currentBitrate = newBitrate;

		g_object_set(G_OBJECT(app.videoEncoder), "bitrate", newBitrate, NULL);
	}

	if (app.previousQuality == 100 && (int)statistics.stats.sender_peer.quality == 100)
	{
		double qualDiffPct = (app.currentBitrate / std::stoi(config.bitrate)) * 100;
		int adjBitrate = (int)(app.currentBitrate * qualDiffPct);
		int bitrateDelta = adjBitrate - app.currentBitrate;

		int newBitrate = min(app.currentBitrate += bitrateDelta / 2, std::stoi(config.bitrate));
		app.currentBitrate = newBitrate;

		g_object_set(G_OBJECT(app.videoEncoder), "bitrate", max(newBitrate, 1000), NULL);
	}	

	app.previousQuality = statistics.stats.sender_peer.quality;

	rist_stats *p_statistics = new rist_stats;
	*p_statistics = statistics;

	Fl::awake(ristStatistics_cb, p_statistics);
}

void *runEncodeThread(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstBus *datasrc_bus;
	std::string datasrc_pipeline_str = "";
	GError *error = NULL;

	app.is_eos = FALSE;
	app.loop = g_main_loop_new(NULL, FALSE);
	app.currentBitrate = std::stoi(config.bitrate);

	RISTNetSender thisRistSender;
	RISTNetSender::RISTNetSenderSettings mySendConfiguration;

	// Generate a vector of RIST URL's,  ip(name), ports, RIST URL output, listen(true) or send mode (false)
	std::string lURL = fmt::format("rist://{}:{}", config.rist_output_address, config.rist_output_port);
	std::vector<std::tuple<std::string, int>> interfaceListSender;
	// if (RISTNetTools::buildRISTURL(config.rist_output_address, config.rist_output_port, lURL, false))
	// {
	interfaceListSender.push_back(std::tuple<std::string, int>(lURL, 0));
	// }

	thisRistSender.statisticsCallback = gotRistStatistics;

	if (config.rist_output_bandwidth != "")
	{
		int recovery_maxbitrate = std::stoi(config.rist_output_bandwidth);
		if (recovery_maxbitrate > 0)
		{
			mySendConfiguration.mPeerConfig.recovery_maxbitrate = recovery_maxbitrate;
		}
	}

	if (config.rist_output_buffer != "")
	{
		int recovery_length = std::stoi(config.rist_output_buffer);
		if (recovery_length > 0)
		{
			mySendConfiguration.mPeerConfig.recovery_length_min = recovery_length;
			mySendConfiguration.mPeerConfig.recovery_length_max = recovery_length;
		}
	}

	mySendConfiguration.mPeerConfig.timing_mode = 2;

	mySendConfiguration.mLogLevel = RIST_LOG_INFO;
	mySendConfiguration.mProfile = RIST_PROFILE_MAIN;
	mySendConfiguration.mLogSetting.get()->log_cb = *ristLog;
	thisRistSender.initSender(interfaceListSender, mySendConfiguration);

	datasrc_pipeline_str += fmt::format("ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux ",
										config.ndi_input_name);

	if (config.transport == "m2ts")
	{
		datasrc_pipeline_str += " appsink buffer-list=true wait-on-eos=false name=videosink mpegtsmux name=tsmux ! tsparse alignment=7 ! videosink. ";
	}

	if (config.transport == "mkv")
	{
		datasrc_pipeline_str += " appsink buffer-list=true wait-on-eos=false name=videosink matroskamux streamable=true name=tsmux ! videosink. ";
	}

	if (config.transport == "rtpgst")
	{
		datasrc_pipeline_str += " appsink buffer-list=true wait-on-eos=false sync=false name=videosink appsink buffer-list=true wait-on-eos=false sync=false name=audiosink ";
	}

	std::string audioDemux = " demux.audio ! queue ! audioresample ! audioconvert ";
	std::string videoDemux = " demux.video ! queue ! videoconvert ";

	std::string audioEncoder = " avenc_aac ";

	std::string videoEncoder;

	std::string audioPayloader;

	std::string videoPayloader;

	if (config.encoder == "amd")
	{
		if (config.codec == "h264")
		{
			videoEncoder = fmt::format("amfh264enc name=vidEncoder  bitrate={} rate-control=cbr usage=low-latency ! video/x-h264,framerate=60/1,profile=high", config.bitrate);
		}
		else if (config.codec == "h265")
		{
			videoEncoder = fmt::format("amfh265enc name=vidEncoder bitrate={} rate-control=cbr usage=low-latency ! video/x-h265,framerate=60/1,profile=high", config.bitrate);
		}
		else if (config.codec == "av1")
		{
			videoEncoder = fmt::format("amfav1enc name=vidEncoder bitrate={} rate-control=cbr usage=low-latency preset=high-quality ! video/x-av1,framerate=60/1", config.bitrate);
		}
	}
	else if (config.encoder == "qsv")
	{
		if (config.codec == "h264")
		{
			videoEncoder = fmt::format("qsvh264enc name=vidEncoder  bitrate={} rate-control=cbr", config.bitrate);
		}
		else if (config.codec == "h265")
		{
			videoEncoder = fmt::format("qsvh265enc name=vidEncoder bitrate={} rate-control=cbr", config.bitrate);
		}
		else if (config.codec == "av1")
		{
			videoEncoder = fmt::format("qsvav1enc name=vidEncoder bitrate={} rate-control=cbr", config.bitrate);
		}
	}
	else if (config.encoder == "nvenc")
	{
		if (config.codec == "h264")
		{
			videoEncoder = fmt::format("nvh264enc name=vidEncoder bitrate={}", config.bitrate);
		}
		else if (config.codec == "h265")
		{
			videoEncoder = fmt::format("nvh265enc name=vidEncoder bitrate={}", config.bitrate);
		}
		else if (config.codec == "av1")
		{
			videoEncoder = fmt::format("nvav1enc name=vidEncoder bitrate={}", config.bitrate);
		}
	}
	else
	{
		if (config.codec == "h264")
		{
			videoEncoder = fmt::format("x264enc name=vidEncoder speed-preset=fast tune=zerolatency bitrate={}", config.bitrate);
		}
		else if (config.codec == "h265")
		{
			videoEncoder = fmt::format("x265enc name=vidEncoder bitrate={} sliced-threads=true speed-preset=fast tune=zerolatency", config.bitrate);
		}
		else if (config.codec == "av1")
		{
			videoEncoder = fmt::format("rav1enc name=vidEncoder bitrate={} speed-preset=8 tile-cols=2 tile-rows=2", config.bitrate);
		}
	}

	if (config.transport == "rtpgst")
	{
		audioPayloader = "queue ! rtpgstpay config-interval=1 pt=97 ! audiosink.";
		videoPayloader = "queue ! rtpgstpay config-interval=1 ! videosink.";
	}
	else
	{
		audioPayloader = "queue ! tsmux. ";
		videoPayloader = "queue ! tsmux. ";
	}

	datasrc_pipeline_str += fmt::format("{} ! {} ! {}  {} ! {} ! {}", audioDemux, audioEncoder, audioPayloader, videoDemux, videoEncoder, videoPayloader);

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

	// app.fallbackVideo = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "fallbackvideo");
	// app.fallbackAudio = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "fallbackaudio");

	app.videoEncoder = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "vidEncoder");

	// /* set up appsink */
	app.videosink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "videosink");

	ristVideoThread = std::thread(runRistVideo, &thisRistSender);

	if (config.transport == "rtp" || config.transport == "rtpgst")
	{

		// /* set up appsink */
		app.audiosink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "audiosink");
		app.rtcp0sink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "rtcp0sink");
		app.rtcp1sink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "rctp1sink");

		ristAudioThread = std::thread(runRistAudio, &thisRistSender);
	}

	app.isPlaying = true;
	logAppend("Setting pipeline to PLAYING.\n");

	gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);

	g_main_loop_run(app.loop);
	app.isPlaying = false;
	gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(app.datasrc_pipeline));
	g_main_loop_unref(app.loop);

	if (ristVideoThread.joinable())
	{
		ristVideoThread.join();
	}
	if (ristAudioThread.joinable())
	{
		ristAudioThread.join();
	}

	thisRistSender.destroySender();

	return 0L;
}

void startStream()
{
	app.isRunning = true;
	if (encodeThread.joinable())
	{
		encodeThread.join();
	}

	encodeThread = std::thread(runEncodeThread, ui);

	Fl::lock();
	ui->btnStartStream->deactivate();
	ui->btnStopStream->activate();
	Fl::unlock();
	Fl::awake();
}

void *previewNDISource(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstElement *pipeline;
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

void streamSource_cb(Fl_Button *, void *)
{
	auto *audioPad = gst_element_get_static_pad(app.fallbackAudio, "sink_0");
	auto *videoPad = gst_element_get_static_pad(app.fallbackVideo, "sink_0");

	g_object_set(G_OBJECT(app.fallbackAudio), "active-pad", audioPad, NULL);
	g_object_set(G_OBJECT(app.fallbackVideo), "active-pad", videoPad, NULL);

	gst_object_unref(audioPad);
	gst_object_unref(videoPad);
}

void streamStandby_cb(Fl_Button *, void *)
{
	auto *audioPad = gst_element_get_static_pad(app.fallbackAudio, "sink_1");
	auto *videoPad = gst_element_get_static_pad(app.fallbackVideo, "sink_1");

	g_object_set(G_OBJECT(app.fallbackAudio), "active-pad", audioPad, NULL);
	g_object_set(G_OBJECT(app.fallbackVideo), "active-pad", videoPad, NULL);

	gst_object_unref(audioPad);
	gst_object_unref(videoPad);
}

void startStream_cb(Fl_Button *o, void *v)
{

	startStream();
}

void stopStream_cb(Fl_Button *o, void *v)
{
	stopStream();
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

void select_encoder_cb(Fl_Menu_ *o, void *v)
{
	config.encoder = (char *)v;
}

void select_transport_cb(Fl_Menu_ *o, void *v)
{
	config.transport = (char *)v;
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

void encoder_bitrate_cb(Fl_Input *o, void *v)
{
	config.bitrate = o->value();
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