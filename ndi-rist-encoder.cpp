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

UserInterface *ui = new UserInterface;
Fl_Text_Buffer *logBuff = new Fl_Text_Buffer();
Fl_Thread prime_thread;
std::map<std::string, GstElement*> sourceMap;

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

void *previewNDISource(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstElement *pipeline, *ndisrc, *ndidemux, *conv, *videosink;
	GstBus *bus;
	GstMessage *msg;

	/* setup pipeline */
// 	pipeline = gst_pipeline_new("pipeline");
// 	ndisrc = sourceMap[ui->ndiSourceSelect->mvalue()->label()];
// 	ndidemux = gst_element_factory_make("ndisrcdemux", "demux");
// 	conv = gst_element_factory_make("videoconvert", "conv");
// 	videosink = gst_element_factory_make("xvimagesink", "videosink");
// 	bus = gst_element_get_bus (pipeline);

// 	gst_bin_add_many (GST_BIN (pipeline), ndisrc, ndidemux, conv, videosink, NULL);
//   gst_element_link_many (ndisrc, ndidemux, conv, videosink, NULL);

pipeline = gst_parse_launch(fmt::format("ndisrc ndi-name=\"{}\" ! ndisrcdemux name=demux   demux.video ! queue ! videoconvert ! autovideosink  demux.audio ! queue ! audioconvert ! autoaudiosink", ui->ndiSourceSelect->mvalue()->label()).c_str(), NULL);
bus = gst_element_get_bus (pipeline);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  msg =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
  /* Parse message */
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (msg, &err, &debug_info);
        g_printerr ("Error received from element %s: %s\n",
            GST_OBJECT_NAME (msg->src), err->message);
        g_printerr ("Debugging information: %s\n",
            debug_info ? debug_info : "none");
        g_clear_error (&err);
        g_free (debug_info);
        break;
      case GST_MESSAGE_EOS:
        g_print ("End-Of-Stream reached.\n");
        break;
      default:
        /* We should not reach here because we only asked for ERRORs and EOS */
        g_printerr ("Unexpected message received.\n");
        break;
    }
    gst_message_unref (msg);
  }

  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

	return 0L;
}

void preview_cb(Fl_Button* o, void* v) {
	fl_create_thread(prime_thread, previewNDISource, ui);
}

void *findNdiDevices(void *p)
{
	UserInterface *ui = (UserInterface *)p;
	GstDeviceMonitor *monitor;
	GstBus *bus;
	GstCaps *caps;

	monitor = gst_device_monitor_new();
	bus = gst_device_monitor_get_bus (monitor);
	caps = gst_caps_new_empty_simple("application/x-ndi");
	gst_device_monitor_add_filter(monitor, "Video/Source", caps);
	gst_caps_unref(caps);

	gst_device_monitor_start(monitor);

	uint32_t no_sources = 0;
	while (!no_sources)
	{

		while (1)
		{
			GstMessage* msg = gst_bus_timed_pop_filtered(bus, 0, GST_MESSAGE_DEVICE_ADDED);
			if (msg)
			{
				GstDevice* newDevice;
				gst_message_parse_device_added(msg, &newDevice);
				std::string newDeviceName = gst_device_get_display_name(newDevice);
				sourceMap[newDeviceName] = gst_device_create_element(newDevice, NULL);
				Fl::lock();
				ui->logDisplay->insert(gst_device_get_display_name(newDevice));
				ui->ndiSourceSelect->add(gst_device_get_display_name(newDevice));
				Fl::unlock();
				Fl::awake();
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

	return 0L;
}
#endif // HAVE_PTHREAD || _WIN32