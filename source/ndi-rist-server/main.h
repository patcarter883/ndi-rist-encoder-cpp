#pragma once

#include <iostream>

#include "gst/gstmessage.h"
using std::cerr;
using std::cout;
using std::endl;
#include <thread>
#include <future>
#include <vector>
#include <string>
#include <atomic>
using std::string; using std::vector;

#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <gst/video/video.h>

#include "RISTNet.h"
#include "rpc/server.h"

/* Structure to contain all our information, so we can pass it to callbacks */
struct App
{
  GstElement* datasrc_pipeline;
  GstElement* videosrc;

  std::atomic_bool is_eos;
  std::atomic_bool isRunning;
  GMainLoop* loop;

  std::atomic_bool is_playing = false;
  gboolean debug = false;

  std::shared_ptr<std::thread> gstreamer_thread;
};

struct Config
{
  std::string rist_input_address =
      "rist://@0.0.0.0:5000?bandwidth=10000buffer-min=245&buffer-max=5000&rtt-min=40&rtt-max=500&reorder-buffer=120&congestion-control=1";
  std::string rtmp_output_address =
      "";
};