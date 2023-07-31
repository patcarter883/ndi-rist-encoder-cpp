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
	GstElement *filesrc, *parser, *decoder, *appsink;
	GstDeviceMonitor *monitor;

	gboolean is_eos;
	GQueue *buf_queue;
	GMutex queue_lock;
	GMainLoop *loop;

	RISTNetSender ristSender;
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

static GstFlowReturn
on_new_sample_from_sink(GstElement *elt, App *app)
{
	GstSample *sample;
	GstBuffer *buffer;
	GstMemory *memory;
	GstMapInfo info;

	/* get the sample from appsink */
	sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
	buffer = gst_sample_get_buffer(sample);
	if (!buffer)
	{
		logAppend("\nPulled NULL buffer. Exiting...\n");
		return GST_FLOW_EOS;
	}

	memory = gst_buffer_get_memory(buffer, 0);

	gst_memory_map(memory, &info, GST_MAP_READ);
	gsize &buf_size = info.size;
	guint8 *&buf_data = info.data;

	app->ristSender.sendData(buf_data, buf_size);

	//   mem = gst_buffer_peek_memory (buffer, 0);
	//   if (mem && gst_is_dmabuf_memory (mem)) {
	//     /* buffer ref required because we will unref sample */
	//     g_mutex_lock (&app->queue_lock);
	//     g_queue_push_tail (app->buf_queue, gst_buffer_ref (buffer));
	//     g_mutex_unlock (&app->queue_lock);
	//   } else {
	//     logAppend ("\nPulled non-dmabuf. Exiting...\n");
	//     return GST_FLOW_EOS;
	//   }

	gst_memory_unmap(memory, &info);
	gst_memory_unref(memory);
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
		break;
	}
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
	app.buf_queue = g_queue_new();
	g_mutex_init(&app.queue_lock);

	datasrc_pipeline_str += fmt::format("appsink name=appsink ndisrc ndi-name=\"{}\" do-timestamp=true ! ndisrcdemux name=demux ",
														  config.ndi_input_name);

	if (config.codec == "h264")
	{
		// datasrc_pipeline_str = datasrc_pipeline_source_str + "video/x-raw,format=I420 ! x264enc ! h264parse config-interval=1 ! queue ! rtph264pay ! rtpbin.send_rtp_sink_0  rtpbin.send_rtp_src_0 ! appsink.";
		datasrc_pipeline_str += fmt::format("demux.video ! queue ! videoconvert ! video/x-raw,format=I420 ! x264enc bitrate={} sliced-threads=true speed-preset=fast tune=zerolatency ! h264parse config-interval=1 ! mpegtsmux alignment=7 name=mux ! appsink. ", config.bitrate);
		datasrc_pipeline_str += "demux.audio ! queue ! audioconvert ! faac ! aacparse ! mux. ! appsink. ";
	}
	else if (config.codec == "h265")
	{
		// datasrc_pipeline_str = datasrc_pipeline_source_str + "x265enc ! h265parse config-interval=1 ! rtph265pay pt=97 ! appsink.";
		datasrc_pipeline_str += "demux.video ! queue ! videoconvert ! x265enc ! h265parse config-interval=1 ! mpegtsmux alignment=7 name=mux ! appsink. ";
		datasrc_pipeline_str += "demux.audio ! queue ! audioconvert ! faac ! aacparse  ! mux. ! appsink. ";

	}
	else if (config.codec == "av1")
	{
		datasrc_pipeline_str += "demux.video ! queue ! videoconvert ! av1enc ! av1parse ! rtpav1pay pt=97 ! rtpbin.send_rtp_sink_0  rtpbin.send_rtp_src_0 ! appsink. ";
		// datasrc_pipeline_str = datasrc_pipeline_source_str + "av1enc ! av1parse config-interval=1 ! mpegtsmux alignment=7 ! appsink.";
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

	/* set up appsink */
	app.appsink = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "appsink");
	g_object_set(G_OBJECT(app.appsink), "emit-signals", TRUE, "sync", FALSE,
				 NULL);
	g_signal_connect(app.appsink, "new-sample",
					 G_CALLBACK(on_new_sample_from_sink), &app);

	logAppend("Changing state of datasrc_pipeline to PLAYING...\n");

	/* only start datasrc_pipeline to ensure we have enough data before
	 * starting datasink_pipeline
	 */
	gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);

	g_main_loop_run(app.loop);

	logAppend("stopping...\n");

	gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);

	gst_object_unref(datasrc_bus);
	g_main_loop_unref(app.loop);
	g_queue_clear(app.buf_queue);
	g_mutex_clear(&app.queue_lock);

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

void addDevice (gpointer devicePtr, gpointer p) {
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

	g_list_foreach (devices, addDevice, NULL);

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