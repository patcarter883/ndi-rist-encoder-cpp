#pragma once
#include "common.h"

#include <FL/Fl.H>

#include "UserInterface.h"
#include <gst/gst.h>
#include <string>

#include "rpc/client.h"


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
  MSGPACK_DEFINE_ARRAY(bitrate,   rist_output_address,   rist_output_buffer_min,   rist_output_buffer_max,   rist_output_rtt_min,   rist_output_rtt_max,   rist_output_reorder_buffer,   rist_output_bandwidth,   rtmp_address,   rtmp_key);
};

}