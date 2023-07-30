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
#include "threads.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>


UserInterface *ui = new UserInterface;
Fl_Text_Buffer *logBuff = new Fl_Text_Buffer();
Fl_Thread prime_thread;

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _App App;

struct _App
{
	GstElement *datasrc_pipeline;
	GstElement *filesrc, *parser, *decoder, *appsink;

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
	std::string rist_output_address = "127.0.0.1";
	std::string rist_output_port = "5000";
	std::string rist_output_buffer;
	std::string rist_output_bandwidth;
};

/* Globals */
Config config;

int main(int argc, char **argv)
{
	/* init GStreamer */
	gst_init(&argc, &argv);

	Fl::lock();
	ui->logDisplay->buffer(logBuff);
	ui->show(argc, argv);

	fl_create_thread(prime_thread, findNdiDevices, ui);

	int result = Fl::run();

	return result;
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
		g_print("\nPulled NULL buffer. Exiting...\n");
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
	//     g_print ("\nPulled non-dmabuf. Exiting...\n");
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
	switch (GST_MESSAGE_TYPE(message))
	{
	case GST_MESSAGE_ERROR:
		g_error("\nReceived error from datasrc_pipeline...\n");
		if (g_main_loop_is_running(app->loop))
			g_main_loop_quit(app->loop);
		break;
	case GST_MESSAGE_EOS:
		g_print("\nReceived EOS from datasrc_pipeline...\n");
		app->is_eos = TRUE;
		break;
	default:
		break;
	}
	return TRUE;
}

void *startStream(void *p)
{
	App app;
	GstBus *datasrc_bus;
	GstPad *pad;
	std::string datasrc_pipeline_str;
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

	std::string datasrc_pipeline_source_str = fmt::format("appsink name=appsink ndisrc ndi-name=\"{}\" ! ndisrcdemux name=demux demux.video ! queue ! videoconvert ! ",
														  config.ndi_input_name);

	if (config.codec == "h264")
	{
		datasrc_pipeline_str = datasrc_pipeline_source_str + "x264enc ! h264parse ! rtph264pay pt=97 ! appsink.";
	}
	else if (config.codec == "h265")
	{
		datasrc_pipeline_str = datasrc_pipeline_source_str + "x265enc ! h265parse ! rtph265pay pt=97 ! appsink.";
	}
	else if (config.codec == "av1")
	{
		datasrc_pipeline_str = datasrc_pipeline_source_str + "av1enc ! av1parse ! rtpav1pay pt=97 ! appsink.";
	}

	g_printerr(datasrc_pipeline_str.c_str());
	app.datasrc_pipeline = gst_parse_launch(datasrc_pipeline_str.c_str(), NULL);
	if (app.datasrc_pipeline == NULL)
	{
		g_print("*** Bad datasrc_pipeline ***\n");
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

	g_print("Changing state of datasrc_pipeline to PLAYING...\n");

	/* only start datasrc_pipeline to ensure we have enough data before
	 * starting datasink_pipeline
	 */
	gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);

	g_main_loop_run(app.loop);

	g_print("stopping...\n");

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

	pipeline = gst_parse_launch(fmt::format("ndisrc ndi-name=\"{}\" ! ndisrcdemux name=demux   demux.video ! queue ! videoconvert ! autovideosink  demux.audio ! queue ! audioconvert ! autoaudiosink", ui->ndiSourceSelect->mvalue()->label()).c_str(), NULL);
	bus = gst_element_get_bus(pipeline);

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
			g_printerr("Error received from element %s: %s\n",
					   GST_OBJECT_NAME(msg->src), err->message);
			g_printerr("Debugging information: %s\n",
					   debug_info ? debug_info : "none");
			g_clear_error(&err);
			g_free(debug_info);
			break;
		case GST_MESSAGE_EOS:
			g_print("End-Of-Stream reached.\n");
			break;
		default:
			/* We should not reach here because we only asked for ERRORs and EOS */
			g_printerr("Unexpected message received.\n");
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

void preview_cb(Fl_Button *o, void *v)
{
	fl_create_thread(prime_thread, previewNDISource, ui);
}

void startStream_cb(Fl_Button *o, void *v)
{
	fl_create_thread(prime_thread, startStream, ui);
}

void ndi_source_select_cb(Fl_Widget *o, void *v)
{
	config.ndi_input_name = (char *)v;
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
	GstDeviceMonitor *monitor;
	GstBus *bus;
	GstCaps *caps;

	monitor = gst_device_monitor_new();
	bus = gst_device_monitor_get_bus(monitor);
	caps = gst_caps_new_empty_simple("application/x-ndi");
	gst_device_monitor_add_filter(monitor, "Video/Source", caps);
	gst_caps_unref(caps);

	gst_device_monitor_start(monitor);

	uint32_t no_sources = 0;
	while (!no_sources)
	{

		while (1)
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
			else
			{
				break;
			}
		}

		// // Get the updated list of sources

		// p_sources = gst_device_monitor_get_devices(monitor);
		// no_sources = g_list_length(p_sources);

		// // Display all the sources.
		// Fl::lock();
		// ui->logDisplay->insert(fmt::format("NDI Sources Updated ({} found).\n", no_sources).c_str());
		// for (uint32_t i = 0; i < no_sources; i++)
		// {
		// 	ui->ndiSourceSelect->add(gst_device_get_display_name((GstDevice *)p_sources[i].data));
		// }
		// ui->logDisplay->insert("\n");
		// Fl::unlock();
		// Fl::awake();
	}
	gst_device_monitor_stop(monitor);
	return 0L;
}
#endif // HAVE_PTHREAD || _WIN32