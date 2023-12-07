#include "ristsender.h"
#include <string>
#include <csignal>
#include "oob_shared.h"
#include <vcs_version.h>
#include <cinttypes>
#include <librist/udpsocket.h>
#include <atomic>
#include <thread>
#include <chrono>

namespace rist_sender{
/* librist. Copyright © 2020 SipRadius LLC. All right reserved.
 * Author: Sergio Ammirata, Ph.D. <sergio@ammirata.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
static int signalReceived = 0;
static int peer_connected_count = 0;
static struct rist_logging_settings logging_settings = LOGGING_SETTINGS_INITIALIZER;

uint64_t prometheus_id = 0;

struct rist_ctx_wrap {
	struct rist_ctx *ctx;
	uintptr_t id;
	bool sender;
};

struct rist_callback_object {
	int sd;
	struct evsocket_ctx *evctx;
	struct rist_ctx_wrap *receiver_ctx;
	struct rist_ctx_wrap *sender_ctx;
	struct rist_udp_config *udp_config;
	uint8_t recv[RIST_MAX_PACKET_SIZE + 100];
};

struct receive_thread_object {
	int sd;
	struct rist_ctx *ctx[MAX_OUTPUT_COUNT];
	struct rist_udp_config *udp_config;
	uint8_t recv[RIST_MAX_PACKET_SIZE];
};

struct rist_sender_args {
	char* token;
	int profile;
	enum rist_log_level loglevel;
	int encryption_type;
	char* shared_secret;
	int buffer_size;
	int statsinterval;
	uint16_t stream_id;
};

	void (*ristStatistics_cb)(const rist_stats& statistics);

	std::atomic_bool* is_playing;

/*
static uint64_t risttools_convertRTPtoNTP(uint32_t i_rtp)
{
	uint64_t i_ntp;
    int32_t clock = 90000;
    i_ntp = (uint64_t)i_rtp << 32;
    i_ntp /= clock;
	return i_ntp;
}
*/

static void input_udp_recv(struct evsocket_ctx *evctx, int fd, short revents, void *arg)
{
	struct rist_callback_object *callback_object = (rist_callback_object *) arg;
	RIST_MARK_UNUSED(evctx);
	RIST_MARK_UNUSED(revents);
	RIST_MARK_UNUSED(fd);

	ssize_t recv_bufsize = -1;
	struct sockaddr_in addr4 = {0};
	struct sockaddr_in6 addr6 = {0};
	struct sockaddr *addr;
	uint8_t *recv_buf = callback_object->recv;
	struct ipheader ipv4hdr;
	struct udpheader udphdr;
	size_t ipheader_bytes = sizeof(ipv4hdr) + sizeof(udphdr);
	socklen_t addrlen = 0;

	uint16_t address_family = (uint16_t)callback_object->udp_config->address_family;
	if (address_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
		recv_bufsize = udpsocket_recvfrom(callback_object->sd, recv_buf + ipheader_bytes, RIST_MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *) &addr6, &addrlen);
		addr = (struct sockaddr *) &addr6;
	} else {
		addrlen = sizeof(struct sockaddr_in);
		recv_bufsize = udpsocket_recvfrom(callback_object->sd, recv_buf + ipheader_bytes, RIST_MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *) &addr4, &addrlen);
		addr = (struct sockaddr *) &addr4;
	}

	if (recv_bufsize > 0) {
		ssize_t offset = 0;
		struct rist_data_block data_block = { 0 };
		// Delegate ts_ntp to the library by default.
		// If we wanted to be more accurate, we could use the kernel nic capture timestamp (linux)
		data_block.ts_ntp = 0;
		data_block.flags = 0;
		if (callback_object->udp_config->rtp_timestamp && recv_bufsize > 12)
		{
			// Extract timestamp from rtp header
			//uint32_t rtp_time = (recv_buf[4] << 24) | (recv_buf[5] << 16) | (recv_buf[6] << 8) | recv_buf[7];
			// Convert to NTP (assumes 90Khz)
			//data_block.ts_ntp = risttools_convertRTPtoNTP(rtp_time);
			// TODO: Figure out why this does not work (commenting out for now)
		}
		if (callback_object->udp_config->rtp_sequence && recv_bufsize > 12)
		{
			// Extract sequence number from rtp header
			//data_block.seq = (uint64_t)((recv_buf[2] << 8) | recv_buf[3]);
			//data_block.flags = RIST_DATA_FLAGS_USE_SEQ;
			// TODO: Figure out why this does not work (commenting out for now)
		}
		if (callback_object->udp_config->version == 1 && callback_object->udp_config->multiplex_mode == LIBRIST_MULTIPLEX_MODE_IPV4) {
			data_block.virt_src_port = UINT16_MAX;
			data_block.payload = recv_buf + offset;
			data_block.payload_len = recv_bufsize - offset + ipheader_bytes;
			populate_ipv4_rist_header(address_family, recv_buf, recv_bufsize, addr, addrlen);
		}
		else {
			// rtp header will not be stripped out in IPV4 mux mode
			if (callback_object->udp_config->rtp && recv_bufsize > 12)
				offset = 12; // TODO: check for header extensions and remove them as well
			if (callback_object->udp_config->version == 1 && callback_object->udp_config->multiplex_mode == LIBRIST_MULTIPLEX_MODE_VIRT_SOURCE_PORT) {
				data_block.virt_src_port = callback_object->udp_config->stream_id;
			}
			data_block.payload = recv_buf + offset + ipheader_bytes;
			data_block.payload_len = recv_bufsize - offset;
		}
		if (peer_connected_count) {
			if (rist_sender_data_write(callback_object->sender_ctx->ctx, &data_block) < 0)
				rist_log(&logging_settings, RIST_LOG_ERROR, "Error writing data in input_udp_recv, socket=%d\n", callback_object->sd);
		}
	}
	else
	{
		// EWOULDBLOCK = EAGAIN = 11 would be the most common recoverable error (if any)
		if (errno != EWOULDBLOCK)
			rist_log(&logging_settings, RIST_LOG_ERROR, "Input receive failed: errno=%d, ret=%d, socket=%d\n", errno, recv_bufsize, callback_object->sd);
	}
}

static void input_udp_sockerr(struct evsocket_ctx *evctx, int fd, short revents, void *arg)
{
	struct rist_callback_object *callback_object = (rist_callback_object *) arg;
	RIST_MARK_UNUSED(evctx);
	RIST_MARK_UNUSED(revents);
	RIST_MARK_UNUSED(fd);
	rist_log(&logging_settings, RIST_LOG_ERROR, "Socket error on sd=%d, stream-id=%d !\n", callback_object->sd, callback_object->udp_config->stream_id);
}

static void connection_status_callback(void *arg, struct rist_peer *peer, enum rist_connection_status peer_connection_status)
{
	(void)arg;
	if (peer_connection_status == RIST_CONNECTION_ESTABLISHED || peer_connection_status == RIST_CLIENT_CONNECTED)
		peer_connected_count++;
	else
		peer_connected_count--;
	rist_log(&logging_settings, RIST_LOG_INFO,"Connection Status changed for Peer %" PRIu64 ", new status is %d, peer connected count is %d\n",
				peer, peer_connection_status, peer_connected_count);
}

static int cb_auth_connect(void *arg, const char* connecting_ip, uint16_t connecting_port, const char* local_ip, uint16_t local_port, struct rist_peer *peer)
{
	struct rist_ctx_wrap *w = (struct rist_ctx_wrap *)arg;
	struct rist_ctx *ctx = w->ctx;
	char buffer[500];
	char message[200];
	int message_len = snprintf(message, 200, "auth,%s:%d,%s:%d", connecting_ip, connecting_port, local_ip, local_port);
	// To be compliant with the spec, the message must have an ipv4 header
	int ret = oob_build_api_payload(buffer, (char *)connecting_ip, (char *)local_ip, message, message_len);
	rist_log(&logging_settings, RIST_LOG_INFO,"Peer has been peer_connected_count, sending oob/api message: %s\n", message);
	struct rist_oob_block oob_block;
	oob_block.peer = peer;
	oob_block.payload = buffer;
	oob_block.payload_len = ret;
	rist_oob_write(ctx, &oob_block);
	return 0;
}

static int cb_auth_disconnect(void *arg, struct rist_peer *peer)
{
	struct rist_ctx *ctx = (struct rist_ctx *)arg;
	(void)ctx;
	(void)peer;
	return 0;
}


static int cb_recv_oob(void *arg, const struct rist_oob_block *oob_block)
{
	struct rist_ctx *ctx = (struct rist_ctx *)arg;
	(void)ctx;
	int message_len = 0;
	char *message = oob_process_api_message((int)oob_block->payload_len, (char *)oob_block->payload, &message_len);
	if (message) {
		rist_log(&logging_settings, RIST_LOG_INFO,"Out-of-band api data received: %.*s\n", message_len, message);
	}
	return 0;
}


static int cb_stats(void *arg, const struct rist_stats *stats_container)
{
        ristStatistics_cb(*stats_container);
	(void)arg;
	rist_stats_free(stats_container);
	return 0;
}

static void intHandler(int signal)
{
	rist_log(&logging_settings, RIST_LOG_INFO, "Signal %d received\n", signal);
	signalReceived = signal;
}

static struct rist_peer* setup_rist_peer(struct rist_ctx_wrap *w, struct rist_sender_args *setup)
{
	struct rist_ctx *ctx = w->ctx;
	if (rist_stats_callback_set(ctx, setup->statsinterval, cb_stats, (void*)w->id) == -1) {

		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not enable stats callback\n");
		return NULL;
	}

	if (rist_auth_handler_set(ctx, cb_auth_connect, cb_auth_disconnect, w) < 0) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not initialize rist auth handler\n");
		return NULL;
	}

	if (rist_connection_status_callback_set(ctx, connection_status_callback, NULL) == -1) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not initialize rist connection status callback\n");
		return NULL;
	}

	if (setup->profile != RIST_PROFILE_SIMPLE) {
		if (rist_oob_callback_set(ctx, cb_recv_oob, ctx) == -1) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not enable out-of-band data\n");
			return NULL;
		}
	}

	if (rist_stats_callback_set(ctx, setup->statsinterval, cb_stats, NULL) == -1) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not enable stats callback\n");
		return NULL;
	}

	// Rely on the library to parse the url
	struct rist_peer_config *peer_config_link = NULL;
	if (rist_parse_address2(setup->token, &peer_config_link))
	{
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not parse peer options for sender: %s\n", setup->token);
		return NULL;
	}

	/* Process overrides */
	struct rist_peer_config *overrides_peer_config = peer_config_link;
	if (setup->buffer_size) {
		overrides_peer_config->recovery_length_min = setup->buffer_size;
		overrides_peer_config->recovery_length_max = setup->buffer_size;
	}
	if (setup->stream_id) {
		if (setup->stream_id % 2 != 0) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Error parsing peer options for sender: %s, stream-id (%d) must be even!\n\n", setup->token, setup->stream_id);
			return NULL;
		}
		else {
			overrides_peer_config->virt_dst_port = setup->stream_id;
		}
	}

	/* Print config */
	rist_log(&logging_settings, RIST_LOG_INFO, "Link configured with maxrate=%d bufmin=%d bufmax=%d reorder=%d rttmin=%d rttmax=%d congestion_control=%d min_retries=%d max_retries=%d\n",
		peer_config_link->recovery_maxbitrate, peer_config_link->recovery_length_min, peer_config_link->recovery_length_max,
		peer_config_link->recovery_reorder_buffer, peer_config_link->recovery_rtt_min, peer_config_link->recovery_rtt_max,
		peer_config_link->congestion_control_mode, peer_config_link->min_retries, peer_config_link->max_retries);

	struct rist_peer *peer;
	if (rist_peer_create(ctx, &peer, peer_config_link) == -1) {

		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not add peer connector to %s\n", peer_config_link->address);
		free((void *)peer_config_link);
		return NULL;
	}

	rist_peer_config_free2(&peer_config_link);

	return peer;
}

static int input_loop(struct rist_callback_object* callback_object)
{
	// This is my main loop (one thread per receiver)
	while (*is_playing) {
		if (callback_object->receiver_ctx)
		{
			// RIST receiver
			struct rist_data_block *b = NULL;
			int queue_size = rist_receiver_data_read2(callback_object->receiver_ctx->ctx, &b, 5);
			if (queue_size > 0) {
				if (queue_size % 10 == 0 || queue_size > 50)
					rist_log(&logging_settings, RIST_LOG_WARN, "Falling behind on rist_receiver_data_read: %d\n", queue_size);
				if (b && b->payload) {
					if (peer_connected_count) {
						int w = rist_sender_data_write(callback_object->sender_ctx->ctx, b);
						// TODO: report error?
						(void) w;
					}
					rist_receiver_data_block_free2(&b);
				}
			}
		}
		else
		{
			// UDP receiver. Infinite wait, 100 socket events
			evsocket_loop_single(callback_object->evctx, 5, 100);
		}
	}
	return 0;
}

static struct rist_ctx_wrap *configure_rist_output_context(char* outputurl,
	struct rist_sender_args *peer_args, const struct rist_udp_config *udp_config,
	bool npd, enum rist_profile profile)
{
	struct rist_ctx *sender_ctx;
	// Setup the output rist objects (a brand new instance per receiver)
	char *saveptroutput;
	char *tmpoutputurl = (char*)malloc(strlen(outputurl) +1);
	strcpy(tmpoutputurl, outputurl);
	char *outputtoken = strtok_r(tmpoutputurl, ",", &saveptroutput);

	// All output peers should be on the same context per receiver
	if (rist_sender_create(&sender_ctx, static_cast<rist_profile>(peer_args->profile), 0, &logging_settings) != 0) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not create rist sender context\n");
		return  NULL;
	}
	struct rist_ctx_wrap *w = (rist_ctx_wrap*)malloc(sizeof(*w));
	w->ctx = sender_ctx;
	w->id = prometheus_id++;
	w->sender = true;
	if (npd) {
		if (profile == RIST_PROFILE_SIMPLE)
			rist_log(&logging_settings, RIST_LOG_INFO, "NULL packet deletion enabled on SIMPLE profile. This is non-compliant but might work if receiver supports it (librist does)\n");
		else
			rist_log(&logging_settings, RIST_LOG_INFO, "NULL packet deletion enabled. Support for this feature is not guaranteed to be present on receivers. Please make sure the receiver supports it (librist does)\n");
		if (rist_sender_npd_enable(sender_ctx) != 0) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Failed to enable null packet deletion\n");
		}
	}
	for (size_t j = 0; j < MAX_OUTPUT_COUNT; j++) {
		peer_args->token = outputtoken;
		peer_args->stream_id = udp_config->stream_id;
		struct rist_peer *peer = setup_rist_peer(w, peer_args);
		if (peer == NULL)
			return  NULL;
		outputtoken = strtok_r(NULL, ",", &saveptroutput);
		if (!outputtoken)
			break;
	}
	free(tmpoutputurl);

	return w;
}

int run_rist_sender(std::string input_url,
        std::string output_url,
        int (*log_cb)(void* arg, rist_log_level, const char* msg),
        void (*stats_cb)(const rist_stats& statistics),
		std::atomic_bool *ptr_is_playing)
{
	struct rist_callback_object callback_object[MAX_INPUT_COUNT] = { {0} };
	struct evsocket_event *event[MAX_INPUT_COUNT];
	char *inputurl = NULL;
	char *outputurl = NULL;
	int buffer_size = 0;
	int encryption_type = 0;
	int statsinterval = 1000;
	enum rist_profile profile = RIST_PROFILE_MAIN;
	enum rist_log_level loglevel = RIST_LOG_INFO;
	bool npd = false;
	int faststart = 0;
	struct rist_sender_args peer_args;
	char *remote_log_address = NULL;
	bool thread_started[MAX_INPUT_COUNT +1] = {false};
        std::vector<std::thread> thread_main_loop;
        is_playing = ptr_is_playing;

	for (size_t i = 0; i < MAX_INPUT_COUNT; i++)
		event[i] = NULL;

#ifdef _WIN32
#define STDERR_FILENO 2
    signal(SIGINT, intHandler);
    signal(SIGTERM, intHandler);
    signal(SIGABRT, intHandler);
#else
	struct sigaction act = { {0} };
	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, NULL);
#endif

	// Default log settings
    struct rist_logging_settings *log_ptr = &logging_settings;
    if (rist_logging_set(&log_ptr, loglevel, log_cb, NULL, NULL,
                         stderr) != 0) {
      rist_log(&logging_settings, RIST_LOG_ERROR,  "Failed to setup default logging!\n");
      exit(1);
	}

	ristStatistics_cb = stats_cb;

	rist_log(&logging_settings, RIST_LOG_INFO, "Starting ristsender version: %s libRIST library: %s API version: %s\n", LIBRIST_VERSION, librist_version(), librist_api_version());
	inputurl = input_url.data();
	outputurl = output_url.data();

	if (inputurl == NULL || outputurl == NULL) {
		return 1;
	}

	if (faststart < 0 || faststart > 1) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Invalid or not implemented fast-start mode %d\n", faststart);
		exit(1);
	}

	if (profile == RIST_PROFILE_SIMPLE || faststart > 0)
		peer_connected_count = 1;

	peer_args.loglevel = loglevel;
	peer_args.profile = profile;
	peer_args.encryption_type = encryption_type;
	peer_args.buffer_size = buffer_size;
	peer_args.statsinterval = statsinterval;

	bool rist_listens = false;
	if (strstr(outputurl, "://@") != NULL) {
		rist_listens = true;
	}

	// Setup the input udp/rist objects: listen to the given address(es)
	int32_t stream_id_check[MAX_INPUT_COUNT ];
	for (size_t j = 0; j < MAX_INPUT_COUNT; j++)
		stream_id_check[j] = -1;
	struct evsocket_ctx *evctx = NULL;
	bool atleast_one_socket_opened = false;
	char *saveptrinput;
	char *inputtoken = strtok_r(inputurl, ",", &saveptrinput);
	struct rist_udp_config *udp_config = NULL;
	for (size_t i = 0; i < MAX_INPUT_COUNT; i++) {
		if (!inputtoken)
			break;

		bool found_empty = false;
		// First parse extra url and parameters
		if (rist_parse_udp_address2(inputtoken, &udp_config)) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not parse inputurl %s\n", inputtoken);
			goto next;
		}

		// Check for duplicate stream-ids and reject the entire config if we have any dups
		
		for (size_t j = 0; j < MAX_INPUT_COUNT; j++) {
			if (stream_id_check[j] == -1 && !found_empty) {
				stream_id_check[j] = (int32_t)udp_config->stream_id;
				rist_log(&logging_settings, RIST_LOG_INFO, "Assigning stream-id %d to this input\n", udp_config->stream_id);
				found_empty = true;
			} else if ((uint16_t)stream_id_check[j] == udp_config->stream_id) {
				rist_log(&logging_settings, RIST_LOG_ERROR, "Every input must have a unique stream-id (%d) when you multiplex\n", udp_config->stream_id);
				goto shutdown;
			}
		}

		// Setup the output rist objects
		if (rist_listens && i > 0) {
			if (callback_object[0].udp_config->version == 1 && (callback_object[0].udp_config->multiplex_mode == LIBRIST_MULTIPLEX_MODE_VIRT_DESTINATION_PORT || udp_config->multiplex_mode == LIBRIST_MULTIPLEX_MODE_VIRT_DESTINATION_PORT)) {
				rist_log(&logging_settings, RIST_LOG_ERROR, "Multiplexing is not allowed when any peer is in listening mode unless you enable non standard muxing on all inputs\n");
				goto shutdown;
			}
			else {
				// Single context for all inputs, we multiplex on the payload
				callback_object[i].sender_ctx = callback_object[0].sender_ctx;
			}
		}
		else
		{
			// A brand new instance/context per receiver
			callback_object[i].sender_ctx = configure_rist_output_context(outputurl, &peer_args, udp_config, npd, profile);
			if (callback_object[i].sender_ctx == NULL)
				goto shutdown;
		}

		if (strcmp(udp_config->prefix, "rist") == 0) {
			// This is a rist input (new context for each listener)
			struct rist_ctx_wrap *w = (rist_ctx_wrap*)calloc(1, sizeof(*w));
			w->id = prometheus_id++;
			callback_object[i].receiver_ctx = w;
			if (rist_receiver_create(&callback_object[i].receiver_ctx->ctx, static_cast<rist_profile>(peer_args.profile), &logging_settings) != 0) {
				rist_log(&logging_settings, RIST_LOG_ERROR, "Could not create rist receiver context\n");
				goto next;
			}
			peer_args.token = inputtoken;
			struct rist_peer *peer = setup_rist_peer(callback_object[i].receiver_ctx, &peer_args);
			if (peer == NULL)
				atleast_one_socket_opened = true;
			rist_udp_config_free2(&udp_config);
			udp_config = NULL;
		}
		else {
			if(!evctx)
				evctx = evsocket_create();
			// This is a udp input, i.e. 127.0.0.1:5000
			char hostname[200] = {0};
			int inputlisten;
			uint16_t inputport;
			if (udpsocket_parse_url(udp_config->address, hostname, 200, &inputport, &inputlisten) || !inputport || strlen(hostname) == 0) {
				rist_log(&logging_settings, RIST_LOG_ERROR, "Could not parse input url %s\n", inputtoken);
				goto next;
			}
			rist_log(&logging_settings, RIST_LOG_INFO, "URL parsed successfully: Host %s, Port %d\n", (char *) hostname, inputport);

			callback_object[i].sd = udpsocket_open_bind(hostname, inputport, udp_config->miface);
			if (callback_object[i].sd < 0) {
				rist_log(&logging_settings, RIST_LOG_ERROR, "Could not bind to: Host %s, Port %d, miface %s.\n",
					(char *) hostname, inputport, udp_config->miface);
				goto next;
			} else {
				udpsocket_set_nonblocking(callback_object[i].sd);
				rist_log(&logging_settings, RIST_LOG_INFO, "Input socket is open and bound %s:%d\n", (char *) hostname, inputport);
				atleast_one_socket_opened = true;
			}
			callback_object[i].udp_config = udp_config;
			udp_config = NULL;
			callback_object[i].evctx = evctx;
			event[i] = evsocket_addevent(callback_object[i].evctx, callback_object[i].sd, EVSOCKET_EV_READ, input_udp_recv, input_udp_sockerr,
				(void *)&callback_object[i]);
		}

next:
		inputtoken = strtok_r(NULL, ",", &saveptrinput);
	}

 	if (!atleast_one_socket_opened) {
 		goto shutdown;
 	}

	thread_main_loop.emplace_back(std::thread(input_loop, &callback_object[0]));

	if (evctx && thread_main_loop.empty())
	{
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not start udp receiver thread\n");
		goto shutdown;
	}
	thread_started[0] = true;

	for (size_t i = 0; i < MAX_INPUT_COUNT; i++) {
		if (((rist_listens && i == 0) || !rist_listens) &&
			 callback_object[i].sender_ctx && rist_start(callback_object[i].sender_ctx->ctx) == -1) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not start rist sender\n");
			goto shutdown;
		}
		if (callback_object[i].receiver_ctx && rist_start(callback_object[i].receiver_ctx->ctx) == -1) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not start rist receiver\n");
			goto shutdown;
		}
                thread_main_loop.emplace_back(std::thread(input_loop, &callback_object[i]));
                if (callback_object[i].receiver_ctx
                    && thread_main_loop.size() != (i + 1))
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not start send rist receiver thread\n");
			goto shutdown;
		} else if (callback_object[i].receiver_ctx) {
			thread_started[i+1] = true;
		}
	}

	while (*is_playing) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }

shutdown:
	if (udp_config) {
		rist_udp_config_free2(&udp_config);
	}
	for (size_t i = 0; i < MAX_INPUT_COUNT; i++) {
		// Remove socket events
		if (event[i])
			evsocket_delevent(callback_object[i].evctx, event[i]);
		// Free udp_config object
		if ((void *)callback_object[i].udp_config)
			rist_udp_config_free2(&callback_object[i].udp_config);
		// Cleanup rist listeners
		if (callback_object[i].receiver_ctx) {
			rist_destroy(callback_object[i].receiver_ctx->ctx);
			free(callback_object[i].receiver_ctx);
		}
		// Cleanup rist sender and their peers
		if (callback_object[i].sender_ctx) {
			rist_destroy(callback_object[i].sender_ctx->ctx);
			free(callback_object[i].sender_ctx);
		}
	}

	for (size_t i = 0; i <= MAX_INPUT_COUNT; i++) {
                if (thread_started[i] && thread_main_loop[i].joinable())
                        thread_main_loop[i].join();
	}

	rist_logging_unset_global();
	return 0;
}
}