// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#include "avb_1722_listener.h"
#include "avb_1722_common.h"
#include "gptp.h"
#include "avb_1722_def.h"
#include "audio_output_fifo.h"
#include <string.h>
#include <xs1.h>
#include <xscope.h>
#include "default_avb_conf.h"
#include "debug_print.h"

#if defined(AVB_1722_FORMAT_AAF)

#if AVB_1722_RECORD_ERRORS
static unsigned char prev_seq_num = 0;
#endif

int avb_1722_listener_process_packet(chanend buf_ctl,
                                     unsigned char Buf[],
                                     int numBytes,
                                     avb_1722_stream_info_t *stream_info,
                                     ptp_time_info_mod64* timeInfo,
                                     int index,
                                     int *notified_buf_ctl,
                                     buffer_handle_t h)
{
  int pktDataLength;
  AVB_DataHeader_t *pAVBHdr;
  int avb_ethernet_hdr_size = (Buf[12]==0x81) ? 18 : 14;
  int num_samples_per_channel;
  pAVBHdr = (AVB_DataHeader_t *) &(Buf[avb_ethernet_hdr_size]);
  unsigned char *sample_ptr;
  int i;
  audio_output_fifo_t *map = &stream_info->map[0];

  // sanity check on number bytes in payload
  if (numBytes <= avb_ethernet_hdr_size + AVB_TP_HDR_SIZE)
  {
    return (0);
  }
  if (AVBTP_VERSION(pAVBHdr) != 0)
  {
    return (0);
  }
  if (AVBTP_SV(pAVBHdr) == 0)
  {
    return (0);
  }

#if AVB_1722_RECORD_ERRORS
  unsigned char seq_num = AVBTP_SEQUENCE_NUMBER(pAVBHdr);
  if ((unsigned char)((unsigned char)seq_num - (unsigned char)prev_seq_num) != 1) {
    debug_printf("DROP %d %d %x\n", seq_num, prev_seq_num, AVBTP_TIMESTAMP(pAVBHdr));
  }
  prev_seq_num = seq_num;
#endif

  // TODO support other samplign rates
  num_samples_per_channel =  6; // at 48kHz

  int num_samples_in_payload = num_samples_per_channel * stream_info->num_channels_in_payload;

  if (stream_info->chan_lock < 16)
  {

    if (!stream_info->num_channels_in_payload)
    {
      stream_info->chan_lock = 0;
      stream_info->rate = 0;
    }

    stream_info->rate += num_samples_in_payload;

    stream_info->chan_lock++;

    if (stream_info->chan_lock == 16)
    {
      stream_info->rate = (stream_info->rate / stream_info->num_channels_in_payload / 16);

      switch (stream_info->rate)
      {
      case 1: stream_info->rate = 8000; break;
      case 2: stream_info->rate = 16000; break;
      case 4: stream_info->rate = 32000; break;
      case 5: stream_info->rate = 44100; break;
      case 6: stream_info->rate = 48000; break;
      case 11: stream_info->rate = 88200; break;
      case 12: stream_info->rate = 96000; break;
      case 24: stream_info->rate = 192000; break;
      default: stream_info->rate = 0; break;
      }
    }

    return 0;
  }

  if ((AVBTP_TV(pAVBHdr)==1))
  {
    unsigned sample_num = 0;
    // register timestamp
    for (int i=0; i<stream_info->num_channels_in_payload; i++)
    {
      if (map[i] >= 0)
      {
        audio_output_fifo_set_ptp_timestamp(h, map[i], AVBTP_TIMESTAMP(pAVBHdr), sample_num);
      }
    }
  }

  for (i=0; i<stream_info->num_channels_in_payload; i++)
  {
    if (map[i] >= 0)
    {
      audio_output_fifo_maintain(h, map[i], buf_ctl, notified_buf_ctl);
    }
  }


  // now send the samples
  sample_ptr = (unsigned char *) &Buf[(avb_ethernet_hdr_size + AVB_TP_HDR_SIZE)];

  int stride = stream_info->num_channels_in_payload;

  for(i=0; i<stream_info->num_channels_in_payload; i++)
  {
    if (map[i] >= 0)
    {
      audio_output_fifo_strided_push(h, map[i], (unsigned int *) sample_ptr,
              stride, num_samples_in_payload);
    }
    sample_ptr += 4;
  }

  return(1);
}

#endif


