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

static int previous_ts;

int avb_1722_listener_process_aaf_packet(chanend buf_ctl,
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
  int num_frames_per_packet = stream_info->frames_per_packet;
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

  unsigned int channels_per_frame = ( AVBTP_FORMAT_SPECIFIC(pAVBHdr) >> 8 ) & 0x3ff;

  if (stream_info->chan_lock < 16)
  {

    unsigned int nsr =  ( AVBTP_FORMAT_SPECIFIC(pAVBHdr) >> 20 ) & 0xf;

    if (!channels_per_frame)
    {
      stream_info->chan_lock = 0;
      stream_info->rate = 0;
    }

    stream_info->chan_lock++;

    if (stream_info->chan_lock == 16)
    {
      switch (nsr)
      {
      case 0x5: stream_info->rate =  48000; stream_info->frames_per_packet =  6; break;
      case 0x7: stream_info->rate =  96000; stream_info->frames_per_packet = 12; break;
      case 0x9: stream_info->rate = 192000; stream_info->frames_per_packet = 24; break;
      default:  stream_info->rate = 0; break;
      }
    }

    stream_info->num_channels_in_payload = channels_per_frame;

    //debug_printf("Locked to AAF stream rate %d frames %d channels %d hdr_size %d\n",
    //        stream_info->rate,
    //        stream_info->frames_per_packet,
    //        channels_per_frame,
    //        avb_ethernet_hdr_size);

    return 0;
  }

  /*
  if( channels_per_frame != stream_info->num_channels )
  {
      debug_printf("channels_per_frame mismatch!\n");
      return 0;
  }
  */

  if ((AVBTP_TV(pAVBHdr)==1))
  {
    unsigned sample_num = 0;
    // register timestamp
    //debug_printf("register timestamp %d\n", AVBTP_TIMESTAMP(pAVBHdr));
    //int diff = AVBTP_TIMESTAMP(pAVBHdr) - previous_ts;
    //previous_ts = AVBTP_TIMESTAMP(pAVBHdr);
    //debug_printf("timestamp diff %d\n", diff);
    for (int i=0; i<channels_per_frame; i++)
    {
      if (map[i] >= 0)
      {
        audio_output_fifo_set_ptp_timestamp(h, map[i], AVBTP_TIMESTAMP(pAVBHdr), sample_num);
      }
    }
  }

  for (i=0; i<channels_per_frame; i++)
  {
    if (map[i] >= 0)
    {
      audio_output_fifo_maintain(h, map[i], buf_ctl, notified_buf_ctl);
    }
  }

  // now send the samples
  sample_ptr = (unsigned char *) &Buf[(avb_ethernet_hdr_size + AVB_TP_HDR_SIZE)];

  int stride = channels_per_frame;

  int num_samples_in_payload = channels_per_frame * num_frames_per_packet;
  //debug_printf("num_samples_in_payload %d\n", num_samples_in_payload);

  for(i=0; i<channels_per_frame; i++)
  {
    if (map[i] >= 0)
    {
      audio_output_fifo_strided_push(h, map[i], (unsigned int *) sample_ptr,
              stride, num_samples_in_payload);
      //debug_printf("pushed channel %d with stride %d samples %d\n", i, stride, num_samples_in_payload);
    }
    sample_ptr += 4;
  }

  return(1);
}

#endif


