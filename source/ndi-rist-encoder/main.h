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

}