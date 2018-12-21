// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
/**
 * \file avb_1722_def.h
 * \brief IEC 61883-6/AVB1722 Audio over 1722 AVB Transport.
 */

#ifndef _AVB1722_DEF_H_
#define _AVB1722_DEF_H_ 1

#include "avb_1722_common.h"


// common definations
#define SW_REF_CLK_PERIOD_NS           (10)
#define IDEAL_NS_IN_SEC                (1000000000)
#define IDEAL_TIMER_TICKS_IN_SECOND    (IDEAL_NS_IN_SEC / SW_REF_CLK_PERIOD_NS)


#define INVALID_ADJUST_NS_PER_SECOND   (0xFFFFFFFF)


// AVB1722 default values
#define AVB1722_DEFAULT_AVB_PKT_DATA_LENGTH    (0x38)

// Default 61883-6 values
#define AVB1722_DEFAULT_TAG                    (1)
#define AVB1722_DEFAULT_CHANNEL                (31)
#define AVB1722_DEFAULT_TCODE                  (0xA)
#define AVB1722_DEFAULT_SY                     (0)

#define AVB1722_DEFAULT_EOH1                   (0)
#define AVB1722_DEFAULT_SID                    (63)
#define AVB1722_DEFAULT_DBS                    (2)
#define AVB1722_DEFAULT_FN                     (0)
#define AVB1722_DEFAULT_QPC                    (0)
#define AVB1722_DEFAULT_SPH                    (0)
#define AVB1722_DEFAULT_DBC                    (0)

#define AVB1722_DEFAULT_EOH2                   (2)
#define AVB1722_DEFAULT_FMT                    (0x10)
#define AVB1722_DEFAULT_FDF                    (2)
#define AVB1722_DEFAULT_SYT                    (0xFFFF)

#define AVB1722_PORT_UNINITIALIZED             (0xDEAD)

#define AVB_DEFAULT_VLAN                   (2)

// NOTE: It is worth pointing out that the 'data block' in 61886-3 means a sample (or
// a collection of samples one for each channel)



// Audio MBLA definitions (top 8 bits for easy combination with sample)
#define AAF_32BIT                           (0x02000000)

// Generic configuration

enum {
  AVB1722_CONFIGURE_TALKER_STREAM = 0,
  AVB1722_CONFIGURE_LISTENER_STREAM,
  AVB1722_ADJUST_TALKER_STREAM,
  AVB1722_ADJUST_LISTENER_STREAM,
  AVB1722_DISABLE_TALKER_STREAM,
  AVB1722_DISABLE_LISTENER_STREAM,
  AVB1722_GET_ROUTER_LINK,
  AVB1722_SET_VLAN,
  AVB1722_TALKER_GO,
  AVB1722_TALKER_STOP,
  AVB1722_SET_PORT,
  AVB1722_ADJUST_LISTENER_CHANNEL_MAP,
  AVB1722_ADJUST_LISTENER_VOLUME,
  AVB1722_GET_COUNTERS
};

// The rate of 1722 packets (8kHz)
#define AVB1722_PACKET_RATE (8000)

// The number of samples per stream in each 1722 packet
#define AVB1722_LISTENER_MAX_NUM_SAMPLES_PER_CHANNEL ((AVB_MAX_AUDIO_SAMPLE_RATE / AVB1722_PACKET_RATE)+1)
#define AVB1722_TALKER_MAX_NUM_SAMPLES_PER_CHANNEL (AVB_MAX_AUDIO_SAMPLE_RATE / AVB1722_PACKET_RATE)

// We add a 2% fudge factor to handle clock difference in the stream transmission shaping
#define AVB1722_PACKET_PERIOD_TIMER_TICKS (((100000000 / AVB1722_PACKET_RATE)*98)/100)

#endif
