#pragma once

#include <utility>

#include <RISTNet.h>
#include <librist.h>
#include <fmt/core.h>
#include <gst/gst.h>

#include "common.h"

class Transport
{
public:
  const Config& config;
  RISTNetSender ristSender;
  std::function<void(const rist_stats& statistics)> statistics_callback;
  int (*log_callback)(void* arg, enum rist_log_level, const char* msg);
  void setup_rist_sender();
  void send_buffer(uint8_t* buf_data, gsize buf_size);
  Transport(Config& config, std::function<void(const rist_stats& statistics)> statistics_callback, int (*log_callback)(void* arg, enum rist_log_level, const char* msg)) :
      config{ config },
      statistics_callback{statistics_callback},
      log_callback{log_callback}
  {
      this->ristSender.statisticsCallback = this->statistics_callback;
  }

  ~Transport() { this->ristSender.destroySender(); }
  //Transport(const Transport& other)  // copy constructor
  //    : Transport(other.config)
  //{
  //}

  //Transport(Transport&& other) noexcept  // move constructor
  //    : config(std::exchange(other.config, nullptr))
  //{
  //}

  //Transport& operator=(const Transport& other)  // copy assignment
  //{
  //  return *this = Transport(other);
  //}

  //Transport& operator=(Transport&& other) noexcept  // move assignment
  //{
  //  std::swap(config, other.config);
  //  return *this;
  //}

private:
};