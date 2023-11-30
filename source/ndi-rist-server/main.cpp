#include "main.h"

/* Globals */
Config config;
App app;

//Return a connection object. (Return nullptr if you don't want to connect to that client)
std::shared_ptr<RISTNetReceiver::NetworkConnection> validateConnection(const std::string &ipAddress, uint16_t port) {
    std::cout << "Connecting IP: " << ipAddress << ":" << unsigned(port) << "\n";

    //Do we want to allow this connection?
    //Do we have enough resources to accept this connection...

    // if not then -> return nullptr;
    // else return a ptr to a NetworkConnection.
    // this NetworkConnection may contain a pointer to any C++ object you provide.
    // That object ptr will be passed to you when the client communicates with you.
    // If the network connection is dropped the destructor in your class is called as long
    // as you do not also hold a reference to that pointer since it's shared.

    auto netConn = std::make_shared<RISTNetReceiver::NetworkConnection>(); // Create the network connection
    return netConn;
}

void clientDisconnect(const std::shared_ptr<RISTNetReceiver::NetworkConnection>& connection, const rist_peer& peer) {
    std::cout << "Client disconnected from receiver\n";
}

int
dataFromSender(const uint8_t *buf, size_t len, std::shared_ptr<RISTNetReceiver::NetworkConnection> &connection,
               rist_peer *pPeer, uint16_t connectionID) {
    
    GstBuffer *buffer;
    GstFlowReturn ret;

    buffer = gst_buffer_new_memdup (buf, len);

   g_signal_emit_by_name (app.videosrc, "push-buffer", buffer, &ret);
    gst_buffer_unref (buffer);

  if (ret != GST_FLOW_OK) {
      /* some error, stop sending data */
      cerr << "Appsrc buffer push error\n";
      g_signal_emit_by_name (app.videosrc, "end-of-stream", &ret);
      return 1;
  }

    return 0; //Keep connection
}

int ristLog(void *arg, enum rist_log_level logLevel, const char *msg)
{
    cout << msg << endl;
	return 1;
}

void stop() {
  std::cout << "Stopping." << "\n";
	app.isPlaying = false;
  if (g_main_loop_is_running(app.loop))
	{
		g_main_loop_quit(app.loop);
	}
  if (app.gstreamer_thread->joinable()) {
    app.gstreamer_thread->join();
  }
}

void handle_gst_message_error(GstMessage* message)
{
  GError* err;
  gchar* debug_info;
  gst_message_parse_error(message, &err, &debug_info);
  logAppend("\nReceived error from datasrc_pipeline...\n");
  logAppend(fmt::format("Error received from element {}: {}\n",
                        GST_OBJECT_NAME(message->src),
                        err->message));
  logAppend(fmt::format("Debugging information: {}\n",
                        debug_info ? debug_info : "none"));
  g_clear_error(&err);
  g_free(debug_info);
  stop();
}

void handle_gst_message_eos(GstMessage* message)
{
  logAppend("\nReceived EOS from pipeline...\n");
  stop();
}

gboolean handle_gstreamer_bus_message(GstBus* bus,
                                      GstMessage* message,
                                      App* app)
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

void parsePipeline(string pipelineString)
{
  app.datasrc_pipeline = NULL;
  GError* error = NULL;
  GstBus* datasrc_bus;

  app.datasrc_pipeline = gst_parse_launch(pipelineString.c_str(), &error);
  if (error) {
    logAppend(fmt::format("Parse Error: {}", error->message));
    g_clear_error(&error);
  }
  if (app.datasrc_pipeline == NULL) {
    logAppend("*** Bad datasrc_pipeline ***\n");
  }

  datasrc_bus = gst_element_get_bus(app.datasrc_pipeline);

  /* add watch for messages */
  gst_bus_add_watch(
      datasrc_bus, (GstBusFunc)handle_gstreamer_bus_message, &app);
  gst_object_unref(datasrc_bus);
}

void playPipeline(std::shared_ptr<std::thread> pipelineThread)
{
  app.is_eos = FALSE;
  app.loop = g_main_loop_new(NULL, FALSE);
  app.is_playing = true;
  logAppend("Setting pipeline to PLAYING.\n");
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_PLAYING);
  pipelineThread = std::make_shared<std::thread>(g_main_loop_run, app.loop);
  if (pipelineThread->joinable())
  {
    pipelineThread->join();
  }
  app.is_playing = false;
  gst_element_set_state(app.datasrc_pipeline, GST_STATE_NULL);

  gst_object_unref(GST_OBJECT(app.datasrc_pipeline));
  g_main_loop_unref(app.loop);
}

void run_gstreamer_thread() {
    RISTNetReceiver ristReceiver;
    GstCaps *caps;

  ristReceiver.validateConnectionCallback = std::bind(
      &validateConnection, std::placeholders::_1, std::placeholders::_2);
  // receive data from the client
  ristReceiver.networkDataCallback = std::bind(&dataFromSender,
                                                    std::placeholders::_1,
                                                    std::placeholders::_2,
                                                    std::placeholders::_3,
                                                    std::placeholders::_4,
													std::placeholders::_5);
                           // client has disconnected
    ristReceiver.clientDisconnectedCallback =
        std::bind(&clientDisconnect, std::placeholders::_1, std::placeholders::_2);
  std::vector<std::string> interfaceListReceiver;
  interfaceListReceiver.push_back(config.rist_input_address);
  RISTNetReceiver::RISTNetReceiverSettings myReceiveConfiguration;
  myReceiveConfiguration.mLogLevel = app.debug ? RIST_LOG_DEBUG : RIST_LOG_INFO;
	myReceiveConfiguration.mProfile = RIST_PROFILE_MAIN;
	myReceiveConfiguration.mLogSetting.get()->log_cb = *ristLog;
  // Initialize the receiver
  if (!ristReceiver.initReceiver(interfaceListReceiver,
                                      myReceiveConfiguration))
  {
    cerr << "Couldn't start RIST Receiver.\n";
    return;
  }

  GError *error = NULL;
  GstBus *datasrc_bus;

	app.loop = g_main_loop_new(NULL, FALSE);
	// std::string pipeline_str = "flvmux streamable=true name=mux ! queue ! rtmpsink location='" + config.rtmp_output_address + "' name=rtmpSink multiqueue name=outq udpsrc port=6000 name=videosrc ! queue2 ! tsparse set-timestamps=true ! tsdemux name=demux demux. ! av1parse ! queue ! nvav1dec ! queue ! cudascale ! queue ! nvh264enc rc-mode=cbr-hq bitrate=16000 gop-size=120 preset=hq ! video/x-h264,framerate=60/1,profile=high ! h264parse ! outq.sink_0 outq.src_0 ! mux.  demux. ! aacparse ! queue max-size-time=5000000000 ! outq.sink_1 outq.src_1 ! mux.";
	std::string pipeline_str = "flvmux streamable=true name=mux ! queue ! rtmpsink location='" + config.rtmp_output_address + "' name=rtmpSink multiqueue name=outq appsrc emit-signals=false block=true is-live=true name=videosrc ! queue2 ! tsparse set-timestamps=true alignment=7 ! tsdemux name=demux demux. ! av1parse ! queue ! nvav1dec ! queue ! cudascale ! video/x-raw(memory:CUDAMemory),width=2560,height=1440 ! queue ! nvh264enc rc-mode=cbr-hq bitrate=16000 gop-size=120 preset=hq ! video/x-h264,framerate=60/1,profile=high ! h264parse ! outq.sink_0 outq.src_0 ! mux.  demux. ! aacparse ! queue max-size-time=5000000000 ! outq.sink_1 outq.src_1 ! mux.";
	// std::string pipeline_str = "flvmux streamable=true name=mux ! queue ! rtmpsink name=rtmpSink location='rtmp://sydney.restream.io/live/re_6467989_5e25e884e7fc0b843888 live=true' multiqueue name=outq appsrc name=videosrc ! queue2 ! tsparse set-timestamps=true alignment=7 ! tsdemux name=demux demux. ! av1parse ! queue ! d3d11av1dec ! queue ! cudascale ! video/x-raw,width=2560,height=1440 ! queue ! amfh264enc rate-control=cbr bitrate=16000 gop-size=120 ! video/x-h264,framerate=60/1,profile=high ! h264parse ! outq.sink_0 outq.src_0 ! mux.  demux. ! aacparse ! queue max-size-time=5000000000 ! outq.sink_1 outq.src_1 ! mux.";
  parsePipeline(pipeline_str);

	app.videosrc = gst_bin_get_by_name(GST_BIN(app.datasrc_pipeline), "videosrc");
/* set the caps on the source */
  caps = gst_caps_new_simple ("video/mpegts",
    "systemstream",G_TYPE_BOOLEAN,true,
    "packetsize",G_TYPE_INT,188,
     NULL);
   gst_app_src_set_caps(GST_APP_SRC(app.videosrc), caps);

  playPipeline(app.gstreamer_thread);

  ristReceiver.destroyReceiver();

  return;
}

void start(std::string rtmpTarget)
{
  std::cout << "Start Requested for destination " <<  rtmpTarget << "\n";
	app.is_playing = true;
	config.rtmp_output_address = rtmpTarget;
	app.gstreamer_thread = std::make_shared<std::thread>(std::thread(app.gstreamer_thread));
}

void runRpc() {
 // Create a server that listens on port 8080, or whatever the user selected
  rpc::server srv("0.0.0.0", 5999);

  std::cout << "RIST Restreamer Started - Listening on port " << srv.port() << "\n";

  srv.bind("start", &start);
  srv.bind("stop", &stop);

  // Run the server loop.
  srv.run();
}

int main(int argc, char **argv)
{
  gst_init(&argc, &argv);
   const vector<string> args(argv+1,argv+argc); // convert C-style to modern C++
   for (auto a : args)
   {
    if (a=="debug")
    {
      app.debug = TRUE;
    }
   }
  
  runRpc();

  if (app.gstreamer_thread->joinable()) {
    app.gstreamer_thread->join();
  }

 
  return 0;
}
