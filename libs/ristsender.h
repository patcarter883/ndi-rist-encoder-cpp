/* librist. Copyright © 2020 SipRadius LLC. All right reserved.
 * Author: Sergio Ammirata, Ph.D. <sergio@ammirata.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <string>
#include <librist/librist.h>
#include <atomic>

namespace rist_sender{
#if defined(_WIN32) || defined(_WIN64)
#define strtok_r strtok_s
#define MSG_DONTWAIT (0)
#endif

#define RIST_MARK_UNUSED(unused_param) ((void)(unused_param))

#define RISTSENDER_VERSION "2"

#define MAX_INPUT_COUNT 20
#define MAX_OUTPUT_COUNT 20
#define RIST_MAX_PACKET_SIZE (10000)

int run_rist_sender(std::string input_url,
        std::string output_url,
        int (*log_cb)(void* arg, rist_log_level, const char* msg),
        void (*stats_cb)(const rist_stats& statistics),
        std::atomic_bool* ptr_is_playing);
}