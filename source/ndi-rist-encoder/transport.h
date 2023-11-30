#include "common.h"
#include <fmt/core.h>
#include <gst/gst.h>
#include <RISTNet.h>
#include <utility>

class Transport {

    public:
        Config* config;
        RISTNetSender ristSender;
        std::function<void(const rist_stats& statistics)> statistics_callback;
        int (*log_callback)(void* arg, enum rist_log_level, const char* msg);
        void setup_rist_sender();
        void send_buffer(BufferDataStruct buffer_data);
        Transport(Config* config) {
            this->config = config;
        }

        ~Transport() {
            this->ristSender.destroySender();
        }

    private:
        
};