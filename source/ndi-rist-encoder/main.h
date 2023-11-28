#pragma once

#include <FL/Fl.H>
#include <RISTNet.h>
#include "UserInterface.h"
#include <gst/gst.h>
#include <string>
#include <atomic>
using std::string;

struct BufferListFuncData
{
  RISTNetSender* sender;
  int streamId;
};

struct App
{
  UserInterface* ui = new UserInterface;
  Fl_Text_Buffer* log_buff = new Fl_Text_Buffer;
  Fl_Text_Buffer* rist_log_buff = new Fl_Text_Buffer;

  GstElement* datasrc_pipeline;
  GstElement *video_sink, *video_encoder;
  GstDeviceMonitor* monitor;

  std::atomic_bool is_eos = false;
  std::atomic_bool is_running = false;
  GMainLoop* loop;

  std::atomic_bool is_playing = false;

  uint16_t current_bitrate;
  double previous_quality;
  uint16_t rpc_port = 5999;
};

struct Config
{
  string ndi_input_name;
  string codec = "h264";
  string encoder = "software";
  string transport = "m2ts";
  string bitrate = "4300";
  string rist_output_address = "127.0.0.1:5000";
  string rist_output_buffer_min = "245";
  string rist_output_buffer_max = "5000";
  string rist_output_rtt_min = "40";
  string rist_output_rtt_max = "500";
  string rist_output_reorder_buffer = "240";
  string rist_output_bandwidth = "6000";
  string rtmp_address = "sydney.restream.io/live";
  string rtmp_key = "";
  uint16_t use_rpc_control = 1;
};

/* Globals */
Config config;
App app;

void startStream();
void stopStream();
void* runEncodeThread();
void* runRistVideo(RISTNetSender* sender);

gboolean sendBufferToRist(GstBuffer* buffer,
                          RISTNetSender* sender,
                          uint16_t streamId = NULL);
gboolean sendBufferListToRist(GstBuffer** buffer,
                              guint idx,
                              gpointer userDataPtr);
gboolean handle_gstreamer_bus_message(GstBus* bus,
                                      GstMessage* message,
                                      App* app);
void handle_gst_message_error(GstMessage* message);
void handle_gst_message_eos(GstMessage* message);
void parsePipeline(string pipelineString);
void playPipeline();
void setupRistSender(RISTNetSender *thisRistSender);

void* previewNDISource();
void* findNdiDevices(void* p);
void addNdiDevice(gpointer devicePtr, gpointer p);

void logAppend(string logMessage);
void ristLogAppend(const char* logMessage);
int ristLog(void* arg, enum rist_log_level, const char* msg);
void gotRistStatistics(const rist_stats& statistics);