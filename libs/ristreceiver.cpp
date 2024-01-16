#include "ristreceiver.h"
#include <string>
#include <csignal>
#include "oob_shared.h"
#include <vcs_version.h>
#include <cinttypes>
#include <librist/librist.h>
#include <librist/udpsocket.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <fcntl.h>

namespace rist_receiver {
static std::atomic_int signalReceived = 0;
static struct rist_logging_settings logging_settings = LOGGING_SETTINGS_INITIALIZER;
enum rist_profile profile = RIST_PROFILE_MAIN;
static int peer_connected_count = 0;
std::atomic_bool* is_playing;


struct rist_callback_object {
	int mpeg[MAX_OUTPUT_COUNT];
	struct rist_udp_config *udp_config[MAX_OUTPUT_COUNT];
	uint16_t i_seqnum[MAX_OUTPUT_COUNT];
	struct rist_ctx *receiver_ctx;
};

static inline void risttools_rtp_set_hdr(uint8_t *p_rtp, uint8_t i_type, uint16_t i_seqnum, uint32_t i_timestamp, uint32_t i_ssrc)
{
	p_rtp[0] = 0x80;
	p_rtp[1] = i_type & 0x7f;
	p_rtp[2] = i_seqnum >> 8;
	p_rtp[3] = i_seqnum & 0xff;
    p_rtp[4] = (i_timestamp >> 24) & 0xff;
    p_rtp[5] = (i_timestamp >> 16) & 0xff;
    p_rtp[6] = (i_timestamp >> 8) & 0xff;
    p_rtp[7] = i_timestamp & 0xff;
	p_rtp[8] = (i_ssrc >> 24) & 0xff;
	p_rtp[9] = (i_ssrc >> 16) & 0xff;
	p_rtp[10] = (i_ssrc >> 8) & 0xff;
	p_rtp[11] = i_ssrc & 0xff;
}

static uint32_t risttools_convertNTPtoRTP(uint64_t i_ntp)
{
	i_ntp *= 90000;
	i_ntp = i_ntp >> 32;
	return (uint32_t)i_ntp;
}

static void connection_status_callback(void *arg, struct rist_peer *peer, enum rist_connection_status peer_connection_status)
{
	(void)arg;
	if (peer_connection_status == RIST_CONNECTION_ESTABLISHED || peer_connection_status == RIST_CLIENT_CONNECTED) {
		peer_connected_count++;
	}
	else {
		peer_connected_count--;
	}
	rist_log(&logging_settings, RIST_LOG_INFO,"Connection Status changed for Peer %" PRIu64 ", new status is %d, peer connected count is %d\n",
				peer, peer_connection_status, peer_connected_count);
}

static int cb_recv(void *arg, struct rist_data_block *b)
{
	struct rist_callback_object *callback_object = (rist_callback_object *) arg;
	int found = 0;
	int i = 0;
	for (i = 0; i < MAX_OUTPUT_COUNT; i++) {
		if (!callback_object->udp_config[i])
			continue;
		struct rist_udp_config *udp_config = callback_object->udp_config[i];
		bool found_it = false;
		int mux_mode = 0;
		if (udp_config->version == 1)
			mux_mode = udp_config->multiplex_mode;
		// The stream-id on the udp url gets translated into the virtual destination port of the GRE tunnel
		// and we match on that. The other two muxing modes are not spec compliant and are only
		// guaranteed to work from librist to librist
		if (profile == RIST_PROFILE_SIMPLE || udp_config->stream_id == 0 ||
				(mux_mode == LIBRIST_MULTIPLEX_MODE_VIRT_DESTINATION_PORT && udp_config->stream_id == b->virt_dst_port) ||
				(mux_mode == LIBRIST_MULTIPLEX_MODE_VIRT_SOURCE_PORT && udp_config->stream_id == b->virt_src_port) ||
				(mux_mode == LIBRIST_MULTIPLEX_MODE_IPV4))
		{
			// Normal manual match
			found_it = true;
		}
		else if (mux_mode == LIBRIST_MULTIPLEX_MODE_AUTO)
		{
			// Auto-detect (librist to librist)
			if (b->virt_src_port == UINT16_MAX) {
				mux_mode = LIBRIST_MULTIPLEX_MODE_IPV4;
				found_it = true;
			}
			if (b->virt_src_port < 32768 && udp_config->stream_id == b->virt_src_port) {
				mux_mode = LIBRIST_MULTIPLEX_MODE_VIRT_SOURCE_PORT;
				found_it = true;
			}
			else if (b->virt_src_port >= 32768 && udp_config->stream_id == b->virt_dst_port) {
				mux_mode = LIBRIST_MULTIPLEX_MODE_VIRT_DESTINATION_PORT;
				found_it = true;
			}
		}

		if (found_it) {
			if (callback_object->mpeg[i] > 0) {
				uint8_t *payload = NULL;
				size_t payload_len = 0;
				if (udp_config->rtp) {
					payload = (uint8_t*)malloc(12 + b->payload_len);
					payload_len = 12 + b->payload_len;
					// Transfer payload
					memcpy(payload + 12, b->payload, b->payload_len);
					// Set RTP header (mpegts)
					uint16_t i_seqnum = udp_config->rtp_sequence ? (uint16_t)b->seq : callback_object->i_seqnum[i]++;
					uint32_t i_timestamp = risttools_convertNTPtoRTP(b->ts_ntp);
					uint8_t ptype = 0x21;
					if (udp_config->rtp_ptype != 0)
						ptype = udp_config->rtp_ptype;
					risttools_rtp_set_hdr(payload, ptype, i_seqnum, i_timestamp, b->flow_id);
				}
				else if (mux_mode == LIBRIST_MULTIPLEX_MODE_IPV4) {
					// TODO: filtering based on ip header?
					// with an input string for destination ip and port
					// for now, forward it all
					// use output_udp_config->mux_filter
					size_t ipheader_bytes = sizeof(struct ipheader) + sizeof(struct udpheader);
					payload = (uint8_t *)b->payload;
					payload += ipheader_bytes;
					payload_len = b->payload_len - ipheader_bytes;
				}
				else {
					payload = (uint8_t *)b->payload;
					payload_len = b->payload_len;
				}
				int ret = udpsocket_send(callback_object->mpeg[i], payload, payload_len);
				if (udp_config->rtp)
					free(payload);
				if (ret <= 0 && errno != ECONNREFUSED)
					rist_log(&logging_settings, RIST_LOG_ERROR, "Error %d sending udp packet to socket %d\n", errno, callback_object->mpeg[i]);
				found = 1;
			}
		}
	}


	if (found == 0)
	{
		rist_log(&logging_settings, RIST_LOG_ERROR, "Destination port mismatch, no output found for %d\n", b->virt_dst_port);
		rist_receiver_data_block_free2(&b);
		return -1;
	}
	rist_receiver_data_block_free2(&b);
	return 0;
}

static void intHandler(int signal) {
	signalReceived.store(signal);
}

static int cb_auth_connect(void *arg, const char* connecting_ip, uint16_t connecting_port, const char* local_ip, uint16_t local_port, struct rist_peer *peer)
{
	struct rist_callback_object *callback_object = (rist_callback_object *) arg;
	char buffer[500];
	char message[200];
	int message_len = snprintf(message, 200, "auth,%s:%d,%s:%d", connecting_ip, connecting_port, local_ip, local_port);
	// To be compliant with the spec, the message must have an ipv4 header
	int ret = oob_build_api_payload(buffer, (char *)connecting_ip, (char *)local_ip, message, message_len);
	rist_log(&logging_settings, RIST_LOG_INFO,"Peer has been authenticated, sending oob/api message: %s\n", message);
	struct rist_oob_block oob_block;
	oob_block.peer = peer;
	oob_block.payload = buffer;
	oob_block.payload_len = ret;
	rist_oob_write(callback_object->receiver_ctx, &oob_block);
	return 0;
}

static int cb_auth_disconnect(void *arg, struct rist_peer *peer)
{
	(void)peer;
	struct rist_callback_object *callback_object = (rist_callback_object *) arg;
	(void)callback_object;
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


struct ristreceiver_flow_cumulative_stats {
	uint32_t flow_id;
	uint64_t received;
	uint64_t recovered;
	uint64_t lost;
	struct ristreceiver_flow_cumulative_stats *next;
};

struct ristreceiver_flow_cumulative_stats *stats_list;

static int cb_stats(void *arg, const struct rist_stats *stats_container) {
	rist_log(&logging_settings, RIST_LOG_INFO, "%s\n",  stats_container->stats_json);
	if (stats_container->stats_type == RIST_STATS_RECEIVER_FLOW)
	{
		struct ristreceiver_flow_cumulative_stats *stats = stats_list;
		struct ristreceiver_flow_cumulative_stats **prev = &stats_list;
		while (stats && stats->flow_id != stats_container->stats.receiver_flow.flow_id)
		{
			prev = &stats->next;
			stats = stats->next;
		}
		if (!stats) {
			stats = (ristreceiver_flow_cumulative_stats*)calloc(sizeof(*stats), 1);
			stats->flow_id = stats_container->stats.receiver_flow.flow_id;
			*prev = stats;
		}
		stats->received += stats_container->stats.receiver_flow.received;
		stats->lost += stats_container->stats.receiver_flow.lost;
		stats->recovered += stats_container->stats.receiver_flow.recovered;
		//Bit ugly, but linking in cJSON seems a bit excessive for this 4 variable JSON string
		rist_log(&logging_settings, RIST_LOG_INFO,
				 "{\"flow_cumulative_stats\":{\"flow_id\":%" PRIu32 ",\"received\":%" PRIu64 ",\"recovered\":%" PRIu64 ",\"lost\":%" PRIu64 "}}\n",
				 stats->flow_id, stats->received, stats->recovered, stats->lost);
		(void)arg;
	}
	rist_stats_free(stats_container);
	return 0;
}


int run_rist_receiver(std::string input_url,
	std::string output_url,
	int (*log_cb)(void *arg, rist_log_level, const char *msg),
	std::atomic_bool* ptr_is_playing)

{
	is_playing = ptr_is_playing;
	int option_index;
	int data_read_mode = DATA_READ_MODE_CALLBACK;
	const struct rist_peer_config *peer_input_config[MAX_INPUT_COUNT];
	char *inputurl = NULL;
	char *outputurl = NULL;
	char *shared_secret = NULL;
	int buffer = 0;
	int encryption_type = 0;
	struct rist_callback_object callback_object = { 0 };
	enum rist_log_level loglevel = RIST_LOG_INFO;
	int statsinterval = 1000;
	char *remote_log_address = NULL;
#ifndef _WIN32
	/* Receiver pipe handle */
	int receiver_pipe[2];
#endif

	for (size_t i = 0; i < MAX_OUTPUT_COUNT; i++)
	{
		callback_object.mpeg[i] = 0;
		callback_object.udp_config[i] = NULL;
	}
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
      std::cerr << "Failed to setup logging!\n";
      exit(1);
	}

	rist_log(&logging_settings, RIST_LOG_INFO, "Starting ristreceiver version: %s libRIST library: %s API version: %s\n", LIBRIST_VERSION, librist_version(), librist_api_version());
	inputurl = input_url.data();
	outputurl = output_url.data();

	if (inputurl == NULL || outputurl == NULL) {
		return 1;
	}

	/* rist side */

	struct rist_ctx *ctx;
	if (rist_receiver_create(&ctx, profile, &logging_settings) != 0) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not create rist receiver context\n");
		exit(1);
	}

	callback_object.receiver_ctx = ctx;

	if (rist_auth_handler_set(ctx, cb_auth_connect, cb_auth_disconnect, (void *)&callback_object) != 0) {

		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not init rist auth handler\n");
		exit(1);
	}

	if (rist_connection_status_callback_set(ctx, connection_status_callback, NULL) == -1) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not initialize rist connection status callback\n");
		exit(1);
	}

	if (profile != RIST_PROFILE_SIMPLE) {
		if (rist_oob_callback_set(ctx, cb_recv_oob, (void *)&callback_object) == -1) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not add enable out-of-band data\n");
			exit(1);
		}
	}

	// if (rist_stats_callback_set(ctx, statsinterval, cb_stats, (void*)0) == -1) {
	// 	rist_log(&logging_settings, RIST_LOG_ERROR, "Could not enable stats callback\n");
	// 	exit(1);
	// }

	char *saveptr1;
	char *inputtoken = strtok_r(inputurl, ",", &saveptr1);
	for (size_t i = 0; i < MAX_INPUT_COUNT; i++) {
		if (!inputtoken)
			break;

		// Rely on the library to parse the url
		struct rist_peer_config *peer_config = NULL;
		if (rist_parse_address2(inputtoken, &peer_config))
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not parse peer options for receiver #%d\n", (int)(i + 1));
			exit(1);
		}

		/* Process overrides */
		struct rist_peer_config *overrides_peer_config = peer_config;
		if (shared_secret && peer_config->secret[0] == 0) {
			strncpy(overrides_peer_config->secret, shared_secret, RIST_MAX_STRING_SHORT -1);
			if (encryption_type)
				overrides_peer_config->key_size = encryption_type;
			else if (!overrides_peer_config->key_size)
				overrides_peer_config->key_size = 128;
		}
		if (buffer) {
			overrides_peer_config->recovery_length_min = buffer;
			overrides_peer_config->recovery_length_max = buffer;
		}

		/* Print config */
		rist_log(&logging_settings, RIST_LOG_INFO, "Link configured with maxrate=%d bufmin=%d bufmax=%d reorder=%d rttmin=%d rttmax=%d congestion_control=%d min_retries=%d max_retries=%d\n",
			peer_config->recovery_maxbitrate, peer_config->recovery_length_min, peer_config->recovery_length_max,
			peer_config->recovery_reorder_buffer, peer_config->recovery_rtt_min,peer_config->recovery_rtt_max,
			peer_config->congestion_control_mode, peer_config->min_retries, peer_config->max_retries);

		peer_input_config[i] = peer_config;

		struct rist_peer *peer;
		if (rist_peer_create(ctx, &peer, peer_input_config[i]) == -1) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not add peer connector to receiver #%i\n", (int)(i + 1));
			exit(1);
		}

		rist_peer_config_free2(&peer_config);
		inputtoken = strtok_r(NULL, ",", &saveptr1);
	}

	/* Mpeg side */
	bool atleast_one_socket_opened = false;
	char *saveptr2;
	char *outputtoken = strtok_r(outputurl, ",", &saveptr2);
	for (size_t i = 0; i < MAX_OUTPUT_COUNT; i++) {

		if (!outputtoken)
			break;

		// First parse extra parameters (?miface=lo&stream-id=1971) and separate the address
		// We are using the rist_parse_address function to create a config object that does not really
		// belong to the udp output. We do this only to avoid writing another parser for the two url
		// parameters available to the udp input/output url
		struct rist_udp_config *udp_config = NULL;
        char hostname[200] = {0};
		if (rist_parse_udp_address2(outputtoken, &udp_config)) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not parse outputurl %s\n", outputtoken);
			goto next;
		}

		// Now parse the address 127.0.0.1:5000
		
		int outputlisten;
		uint16_t outputport;
		if (udpsocket_parse_url((char *)udp_config->address, hostname, 200, &outputport, &outputlisten) || !outputport || strlen(hostname) == 0) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not parse output url %s\n", outputtoken);
			goto next;
		}
		rist_log(&logging_settings, RIST_LOG_INFO, "URL parsed successfully: Host %s, Port %d\n", (char *) hostname, outputport);

		// Open the output socket
		callback_object.mpeg[i] = udpsocket_open_connect(hostname, outputport, udp_config->miface);
		if (callback_object.mpeg[i] < 0) {
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not connect to: Host %s, Port %d\n", (char *) hostname, outputport);
			goto next;
		} else {
			rist_log(&logging_settings, RIST_LOG_INFO, "Output socket is open and bound %s:%d\n", (char *) hostname, outputport);
			atleast_one_socket_opened = true;
		}
		callback_object.udp_config[i] = udp_config;

next:
		outputtoken = strtok_r(NULL, ",", &saveptr2);
	}

	if (!atleast_one_socket_opened) {
		exit(1);
	}

	if (data_read_mode == DATA_READ_MODE_CALLBACK) {
		if (rist_receiver_data_callback_set2(ctx, cb_recv, &callback_object))
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not set data_callback pointer\n");
			exit(1);
		}
	}
#ifndef _WIN32
	else if (data_read_mode == DATA_READ_MODE_POLL) {
		if (pipe(receiver_pipe))
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not create pipe for file descriptor channel\n");
			exit(1);
		}
		if (fcntl(receiver_pipe[WriteEnd], F_SETFL, O_NONBLOCK) < 0)
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not set pipe to non blocking mode\n");
 			exit(1);
 		}
		if (fcntl(receiver_pipe[ReadEnd], F_SETFL, O_NONBLOCK) < 0)
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not set pipe to non blocking mode\n");
 			exit(1);
 		}
		if (rist_receiver_data_notify_fd_set(ctx, receiver_pipe[WriteEnd]))
		{
			rist_log(&logging_settings, RIST_LOG_ERROR, "Could not set file descriptor channel\n");
			exit(1);
		}
	}
#endif

	if (rist_start(ctx)) {
		rist_log(&logging_settings, RIST_LOG_ERROR, "Could not start rist receiver\n");
		exit(1);
	}
	/* Start the rist protocol thread */
	if (data_read_mode == DATA_READ_MODE_CALLBACK) {
while (*is_playing) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
	}
	else if (data_read_mode == DATA_READ_MODE_API) {
#ifndef _WIN32
		int prio_max = sched_get_priority_max(SCHED_RR);
		struct sched_param param = { 0 };
		param.sched_priority = prio_max;
		if (pthread_setschedparam(pthread_self(), SCHED_RR, &param) != 0)
			rist_log(&logging_settings, RIST_LOG_WARN, "Failed to set data output thread to RR scheduler with prio of %i\n", prio_max);
#else
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
		// Master loop
		while (*is_playing)
		{
			struct rist_data_block *b = NULL;
			int queue_size = rist_receiver_data_read2(ctx, &b, 5);
			if (queue_size > 0) {
				if (queue_size % 10 == 0 || queue_size > 50) {
					// We need a better way to report on this
					uint32_t flow_id = b ? b->flow_id : 0;
					rist_log(&logging_settings, RIST_LOG_WARN, "Falling behind on rist_receiver_data_read: count %d, flow id %u\n", queue_size, flow_id);
				}
				if (b && b->payload) cb_recv(&callback_object, b);
			}
			if (signalReceived.load())
			{
				rist_log(&logging_settings, RIST_LOG_INFO, "Signal %d received\n", signal);
				break;
			}
		}
	}
#ifndef _WIN32
	else if (data_read_mode == DATA_READ_MODE_POLL) {
		char pipebuffer[256];
		fd_set readfds;
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 5000;
		while (*is_playing) {
			FD_ZERO(&readfds);
			FD_SET(receiver_pipe[ReadEnd], &readfds);
			int ret = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
			if (ret == -1 && errno != EINTR) {
				rist_log(&logging_settings, RIST_LOG_ERROR, "Pipe read error %d, exiting\n", errno);
				break;
			}
			else if (ret == 0) {
				// Normal timeout (loop and wait)
				continue;
			}
			/* Consume bytes from pipe (irrelevant data) */
			for (;;) {
				if (read(receiver_pipe[ReadEnd], &pipebuffer, sizeof(pipebuffer)) <= 0) {
					if (errno != EAGAIN)
						rist_log(&logging_settings, RIST_LOG_ERROR, "Error reading data from pipe: %d\n", errno);
					break;
				}
			}
			/* Consume data from library */
			struct rist_data_block *b = NULL;
			int queue_size = 0;
			for (;;) {
				queue_size = rist_receiver_data_read2(ctx, &b, 0);
				if (queue_size > 0) {
					if (queue_size % 10 == 0 || queue_size > 50) {
						// We need a better way to report on this
						uint32_t flow_id = b ? b->flow_id : 0;
						rist_log(&logging_settings, RIST_LOG_WARN, "Falling behind on rist_receiver_data_read: count %d, flow id %u\n", queue_size, flow_id);
					}
					if (b && b->payload) cb_recv(&callback_object, b);
				}
				else
					break;
			}
			if (signalReceived.load())
			{
				rist_log(&logging_settings, RIST_LOG_INFO, "Signal %d received\n", signal);
				break;
			}
		}
	}
#endif
	rist_log(&logging_settings, RIST_LOG_ERROR, "DESTROY\n");
	rist_destroy(ctx);

	for (size_t i = 0; i < MAX_OUTPUT_COUNT; i++) {
		// Free udp_config object
		if ((void *)callback_object.udp_config[i])
			rist_udp_config_free2(&callback_object.udp_config[i]);
	}

	rist_logging_unset_global();

	// struct ristreceiver_flow_cumulative_stats *stats, *next;
	// stats = stats_list;
	// while (stats)
	// {
	// 	next = stats->next;
	// 	free(stats);
	// 	stats = next;
	// }
	return 0;
}
}