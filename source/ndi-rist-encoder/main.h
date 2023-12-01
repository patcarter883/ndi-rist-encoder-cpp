#pragma once
#include "common.h"

#include <FL/Fl.H>
#include <RISTNet.h>
#include "UserInterface.h"
#include <gst/gst.h>
#include <string>

#include <atomic>


namespace nre {

void startStream();
void stopStream();

void* previewNDISource();
void* findNdiDevices(void* p);
void addNdiDevice(gpointer devicePtr, gpointer p);

void gotRistStatistics(const rist_stats& statistics);
void logAppend(std::string logMessage);
void ristLogAppend(std::string logMessage);
int ristLog(void* arg, enum rist_log_level, const char* msg);
void rist_loop();

struct RpcData
{
  std::string bitrate;
  std::string rist_output_address;
  std::string rist_output_buffer_min;
  std::string rist_output_buffer_max;
  std::string rist_output_rtt_min;
  std::string rist_output_rtt_max;
  std::string rist_output_reorder_buffer;
  std::string rist_output_bandwidth;
  std::string rtmp_address;
  std::string rtmp_key;
};

}