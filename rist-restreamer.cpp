#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "rist-restreamer.h"
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include <threads.h>
#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _App App;

struct _App
{
	GstElement *datasrc_pipeline;
	GstElement *appsrc;

	gboolean is_eos;
	GMainLoop *loop;

	RISTNetReceiver ristReceiver;

	gboolean isPlaying = false;
};

typedef struct _Config Config;

struct _Config
{

	std::string codec = "h264";
	std::string bitrate = "5000";
	std::string rist_address = "127.0.0.1";
	std::string rist_port = "5000";
	std::string rist_buffer;
	std::string rist_bandwidth;
};

/* Globals */
Config config;
App app;

int main(int argc, char **argv)
{

    GstBus *datasrc_bus;

    // std::string lURL;
	// std::vector<std::tuple<std::string, int>> interfaceListReceiver;
	// if (RISTNetTools::buildRISTURL(config.rist_address, config.rist_port, lURL, false))
	// {
	// 	interfaceListReceiver.push_back(std::tuple<std::string, int>(lURL, 5));
	// }

	// // Populate the settings
	// RISTNetReceiver::RISTNetReceiverSettings myReceiveConfiguration;
	// app.ristReceiver.initReceiver(interfaceListReceiver, myReceiveConfiguration);

}