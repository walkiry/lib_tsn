// Copyright (c) 2017, d&b audiotechnik GmbH, All rights reserved
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

#if defined(AVB_1722_FORMAT_CRF)

#if AVB_1722_RECORD_ERRORS
static unsigned char prev_seq_num = 0;
#endif

int avb_1722_listener_process_crf_packet(chanend buf_ctl,
                                     unsigned char Buf[],
                                     int numBytes,
                                     avb_1722_stream_info_t *stream_info,
                                     ptp_time_info_mod64* timeInfo,
                                     int index,
                                     int *notified_buf_ctl,
                                     buffer_handle_t h)
{

  AVB_CrfHeader_t *pCrfHdr;
  int crf_ethernet_hdr_size = (Buf[12]==0x81) ? 18 : 14;
  pCrfHdr = (AVB_CrfHeader_t *) &(Buf[crf_ethernet_hdr_size]);
  int i = 0; //just one fake audio channel for crf streams
  audio_output_fifo_t *map = &stream_info->map[0];

  // TODO This is just a Hack to bring Timestamps to the correct buffer. Where is this to be configured correctly?
  map[0] = 8;

 // debug_printf("CRF packet processing... ts %d %d seq %d map %d\n", CRF_TIMESTAMP_HI(pCrfHdr), CRF_TIMESTAMP_LO(pCrfHdr), pCrfHdr->sequence_number, map[i]);

  // sanity check on number bytes in payload
  if (numBytes <= crf_ethernet_hdr_size + AVB_CRF_HDR_SIZE)
  {
    return (0);
  }
  if (CRF_VERSION(pCrfHdr) != 0)
  {
    return (0);
  }
  /*
  if (AVBTP_CD(pAVBHdr) != AVBTP_CD_DATA)
  {
    return (0);
  }
  if (AVBTP_SV(pAVBHdr) == 0)
  {
    return (0);
  }
  */

#if AVB_1722_RECORD_ERRORS
  unsigned char seq_num = AVBTP_SEQUENCE_NUMBER(pAVBHdr);
  if ((unsigned char)((unsigned char)seq_num - (unsigned char)prev_seq_num) != 1) {
    debug_printf("DROP %d %d %x\n", seq_num, prev_seq_num, CRF_TIMESTAMP(pCrfHdr));
  }
  prev_seq_num = seq_num;
#endif


#if 1

  // register tiemstamp
  unsigned sample_num = 0;
  audio_output_fifo_set_ptp_timestamp(h, map[i], CRF_TIMESTAMP_LO(pCrfHdr), sample_num);

  // Notify media clock server
  audio_output_fifo_maintain(h, map[i], buf_ctl, notified_buf_ctl);

  // now send the fake sample
  unsigned int sample = 0;
  int num_channels_in_payload = 1;
  int num_samples_in_payload = 1;
  audio_output_fifo_strided_push(h, map[i], &sample, num_channels_in_payload, num_samples_in_payload);


#endif
  return(1);
}

#endif


