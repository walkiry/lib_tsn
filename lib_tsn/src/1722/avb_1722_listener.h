// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
/**
 * \file avb_1722_listener.h
 * \brief IEC 61883-6/AVB1722 Listener definitions
 */

#ifndef _AVB1722_LISTENER_H_
#define _AVB1722_LISTENER_H_ 1
#ifndef __XC__
#define streaming
#endif
#include <xccompat.h>
#include "default_avb_conf.h"
#include "avb_1722_def.h"
#include "gptp.h"
#include "audio_buffering.h"

#ifndef MAX_INCOMING_AVB_STREAMS
#define MAX_INCOMING_AVB_STREAMS (AVB_NUM_SINKS+1) //+1 CRF Stream
#endif

#ifndef AVB_MAX_CHANNELS_PER_LISTENER_STREAM
#define AVB_MAX_CHANNELS_PER_LISTENER_STREAM 8
#endif

#ifndef MAX_AVB_STREAMS_PER_LISTENER
#define MAX_AVB_STREAMS_PER_LISTENER 4
#endif


typedef struct avb_1722_stream_info_t {
  short active;                    //!< 1-bit flag to say if the stream is active
  short state;                     //!< Generic state info
  int chan_lock;                   //!< Counter for locking onto a data stream
  int rate;                        //!< The estimated rate of the audio traffic
  int frames_per_packet;           //!< The number of frames per packet
  int num_channels_in_payload;     //!< The number of channels in the 1722 payloads
  int num_channels;                //!< The number of expected channels in the 1722 payload
  int last_sequence;               //!< The sequence number from the last 1722 packet
  audio_output_fifo_t map[AVB_MAX_CHANNELS_PER_LISTENER_STREAM];
} avb_1722_stream_info_t;

#ifdef __XC__
int avb_1722_listener_process_packet(chanend? buf_ctl,
                                     unsigned char Buf[],
                                     int numBytes,
                                     REFERENCE_PARAM(avb_1722_stream_info_t, stream_info),
                                     NULLABLE_REFERENCE_PARAM(ptp_time_info_mod64, timeInfo),
                                     int index,
                                     REFERENCE_PARAM(int, notified_buf_ctl),
                                     buffer_handle_t h);
int avb_1722_listener_process_crf_packet(chanend? buf_ctl,
                                     unsigned char Buf[],
                                     int numBytes,
                                     REFERENCE_PARAM(avb_1722_stream_info_t, stream_info),
                                     NULLABLE_REFERENCE_PARAM(ptp_time_info_mod64, timeInfo),
                                     int index,
                                     REFERENCE_PARAM(int, notified_buf_ctl),
                                     buffer_handle_t h);
int avb_1722_listener_process_aaf_packet(chanend? buf_ctl,
                                     unsigned char Buf[],
                                     int numBytes,
                                     REFERENCE_PARAM(avb_1722_stream_info_t, stream_info),
                                     NULLABLE_REFERENCE_PARAM(ptp_time_info_mod64, timeInfo),
                                     int index,
                                     REFERENCE_PARAM(int, notified_buf_ctl),
                                     buffer_handle_t h);
#else
int avb_1722_listener_process_packet(chanend buf_ctl,
                                     unsigned char Buf[],
                                     int numBytes,
                                     REFERENCE_PARAM(avb_1722_stream_info_t, stream_info),
				                             REFERENCE_PARAM(ptp_time_info_mod64, timeInfo),
                                     int index,
                                     REFERENCE_PARAM(int, notified_buf_ctl),
                                     buffer_handle_t h);
#endif

struct listener_counters {
  unsigned received_1722;
};

typedef struct avb_1722_listener_state_s {
  avb_1722_stream_info_t listener_streams[MAX_AVB_STREAMS_PER_LISTENER];
  int notified_buf_ctl;
  int router_link;
  struct listener_counters counters;
} avb_1722_listener_state_t;


#endif
