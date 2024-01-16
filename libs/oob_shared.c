/* librist. Copyright © 2020 SipRadius LLC. All right reserved.
 * Author: Sergio Ammirata, Ph.D. <sergio@ammirata.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <string.h>
#include <stdio.h>
#include "oob_shared.h"
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "socket-shim.h"
#ifdef USE_TUN
#include <librist/udpsocket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

static unsigned short csum(unsigned short *buf, int nwords)
{
	unsigned long sum;
	for(sum=0; nwords>0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum &0xffff);
	sum += (sum >> 16);
	return (unsigned short)(~sum);
}

void populate_ip_header(struct ipheader *ip, char *sourceip, char *destip, unsigned short api_id, unsigned short protocol)
{
	ip->iph_verlen = 0x45;
	ip->iph_tos = 0;
	ip->iph_len = htons(sizeof(struct ipheader));
	ip->iph_ident = htons(api_id);
	ip->iph_flags = 0x0040;
	ip->iph_ttl = 0x40;
	ip->iph_protocol = protocol;
	// The source IP address
	unsigned int address = 0;
	inet_pton(AF_INET, sourceip, &address);
	ip->iph_sourceip = address;
	// The destination IP address
	inet_pton(AF_INET, destip, &address);
	ip->iph_destip = address;
	// make sure the checksum area is zero or the calculated checksum will be wrong
	ip->iph_chksum = 0;
}

void populate_ipv4_rist_header(uint16_t address_family, uint8_t *recv_buf, ssize_t recv_bufsize, struct sockaddr * addr, socklen_t addrlen)
{
	if (address_family == AF_INET6) {
		// TODO: map ipv6 to ipv4
	}
	else {
		// TODO
	}
	(void )address_family;
	(void)recv_buf;
	(void)recv_bufsize;
	(void)addr;
	(void)addrlen;
}

int oob_build_api_payload(char *buffer, char *sourceip, char *destip, char *message, int message_len)
{
	// We populate a valid IP header here but we do not really use it for any type of routing
	// We also populate a message that has the same information already present in the IP header but write it in text format
	// This is only for demonstration purposes as we do not use the IP or the text message for anything on 
	// the receiving end or inside the library. However, this is a good method to create internal communication 
	// messages between peers. When designing your solution, just remember that in the OOB channel, there is no 
	// extra buffer delay and no packet recovery.
	struct ipheader *ip = (struct ipheader *) buffer;
	// unassigned protocol 252 used for API communication, api_id (54321 to identify this API message type)
	populate_ip_header(ip, sourceip, destip, RIST_OOB_API_IP_IDENT_AUTH, RIST_OOB_API_IP_PROTOCOL);
	memcpy(buffer + sizeof(struct ipheader), message, message_len);
	int total_len = sizeof(struct ipheader) + message_len;
	ip->iph_len = htons(total_len);
	// Calculate the checksum for integrity since there is no packet recovery
	ip->iph_chksum = csum((unsigned short *)buffer, total_len);
	return total_len;
}

char *oob_process_api_message(int buffer_len, char *buffer, int *message_len)
{
	struct ipheader *ip = (struct ipheader *) buffer;
	int header_size = sizeof(struct ipheader);

	// Check reported length vs buffer length
	if (htons(buffer_len) != ip->iph_len) {
		*message_len = RIST_OOB_ERROR_INVALID_LENGTH;
		return NULL;
	}

	// Check for protocol type and only process API messages for now
	if (ip->iph_protocol != RIST_OOB_API_IP_PROTOCOL) {
		*message_len = RIST_OOB_ERROR_INVALID_PROTO;
		return NULL;
	}

	// Check for only support API call
	if (ip->iph_ident != htons(RIST_OOB_API_IP_IDENT_AUTH)) {
		*message_len = RIST_OOB_ERROR_INVALID_IDENT;
		return NULL;
	}

	// Move the buffer pointer and return the payload length
	*message_len = buffer_len - header_size;
	return buffer + header_size;
}