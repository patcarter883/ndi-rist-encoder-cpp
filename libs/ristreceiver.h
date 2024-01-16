/* librist. Copyright © 2020 SipRadius LLC. All right reserved.
 * Author: Sergio Ammirata, Ph.D. <sergio@ammirata.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <librist/librist.h>
#include <string>
#include <atomic>

#if defined(_WIN32) || defined(_WIN64)
# define strtok_r strtok_s
#endif

#define RISTRECEIVER_VERSION "2"

#define MAX_INPUT_COUNT 20
#define MAX_OUTPUT_COUNT 20
#define ReadEnd  0
#define WriteEnd 1
#define DATA_READ_MODE_CALLBACK 0
#define DATA_READ_MODE_POLL 1
#define DATA_READ_MODE_API 2

namespace rist_receiver {
    int run_rist_receiver(std::string input_url,
        std::string output_url, 
        int (*log_cb)(void *arg, rist_log_level, const char *msg),
        std::atomic_bool* ptr_is_playing);
}