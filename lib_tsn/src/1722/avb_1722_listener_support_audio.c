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
  int num_samples_in_payload, num_channels_in_payload;
  pAVBHdr = (AVB_DataHeader_t *) &(Buf[avb_ethernet_hdr_size]);
  unsigned char *sample_ptr;
  int i;
  int num_channels = stream_info->num_channels;
  audio_output_fifo_t *map = &stream_info->map[0];
  int stride;

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

  // TODO calcualte correctly! for now use Asumption: Format=INT32BIT, 8 channels, 6 samples per frame
  pktDataLength = 6*8*4;
  num_samples_in_payload = (pktDataLength-8)>>2;

  int prev_num_samples = stream_info->prev_num_samples;
  stream_info->prev_num_samples = num_samples_in_payload;

  if (stream_info->chan_lock < 16)
  {
    int num_channels;

    if (!prev_num_samples || dbc_diff == 0) {
      return 0;
    }

    num_channels = prev_num_samples / dbc_diff;

    if (!stream_info->num_channels_in_payload ||
        stream_info->num_channels_in_payload != num_channels)
    {
      stream_info->num_channels_in_payload = num_channels;
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
    for (int i=0; i<num_channels; i++)
    {
      if (map[i] >= 0)
      {
        audio_output_fifo_set_ptp_timestamp(h, map[i], AVBTP_TIMESTAMP(pAVBHdr), sample_num);
      }
    }
  }

  for (i=0; i<num_channels; i++)
  {
    if (map[i] >= 0)
    {
      audio_output_fifo_maintain(h, map[i], buf_ctl, notified_buf_ctl);
    }
  }


  // now send the samples
  sample_ptr = (unsigned char *) &Buf[(avb_ethernet_hdr_size + AVB_TP_HDR_SIZE)];

  num_channels_in_payload = stream_info->num_channels_in_payload;

  stride = num_channels_in_payload;

  num_channels =
    num_channels < num_channels_in_payload ?
    num_channels :
    num_channels_in_payload;

  for(i=0; i<num_channels; i++)
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


