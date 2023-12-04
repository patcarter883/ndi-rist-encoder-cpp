/* librist. Copyright © 2020 SipRadius LLC. All right reserved.
 * Author: Sergio Ammirata, Ph.D. <sergio@ammirata.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <inttypes.h>
#include <librist/librist.h>
#include <librist/udpsocket.h>
#include <version.h>
#include <librist_config.h>
#if HAVE_SRP_SUPPORT
#include "librist/librist_srp.h"
#include "srp_shared.h"
#endif
#include <vcs_version.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "getopt-shim.h"
#include "pthread-shim.h"
#include "time-shim.h"
#include <stdbool.h>
#include <signal.h>
#include "common/attributes.h"
#include <stdatomic.h>
#include "oob_shared.h"


#if defined(_WIN32) || defined(_WIN64)
#define strtok_r strtok_s
#define MSG_DONTWAIT (0)
#endif

#define RIST_MARK_UNUSED(unused_param) ((void)(unused_param))

#define RISTSENDER_VERSION "2"

#define MAX_INPUT_COUNT 20
#define MAX_OUTPUT_COUNT 20
#define RIST_MAX_PACKET_SIZE (10000)