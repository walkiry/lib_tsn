// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#include "debug_print.h"
#include <print.h>
#include <xscope.h>
#include "default_avb_conf.h"
#include <xclib.h>

#if AVB_NUM_SOURCES > 0 && defined(AVB_1722_FORMAT_AAF)

#include <xccompat.h>
#include <string.h>

#include "avb_1722_talker.h"
#include "gptp.h"

// default audio sample type 24bits.
unsigned int AVB1722_audioSampleType = AAF_32BIT;

/** Update fields in the 1722 header which change for each PDU
 *
 *  \param Buf the buffer containing the packet
 *  \param valid_ts the timestamp is valid flag
 *  \param avbtp_ts the 32 bit PTP timestamp
 *  \param pkt_data_length the number of samples in the PDU
 *  \param sequence_number the 1722 sequence number
 *  \param stream_id0 the bottom 32 bits of the stream id
 */
static void AVB1722_AVBTP_HeaderGen(unsigned char Buf[],
        int valid_ts,
        unsigned avbtp_ts,
        int sequence_number,
        const unsigned stream_id0)
{
    AVB_DataHeader_t *pAVBHdr = (AVB_DataHeader_t *) &(Buf[AVB_ETHERNET_HDR_SIZE]);

    // only stamp the AVBTP timestamp when required.
    if (valid_ts) {
        SET_AVBTP_TV(pAVBHdr, 1); // AVB timestamp valid.
        SET_AVBTP_TIMESTAMP(pAVBHdr, avbtp_ts); // Valid ns field.
    } else {
        SET_AVBTP_TV(pAVBHdr, 0); // AVB timestamp not valid.
        SET_AVBTP_TIMESTAMP(pAVBHdr, 0); // NULL the timestmap field as well.
    }

    // update stream ID by adding stream number to preloaded stream ID
    // (ignoring the fact that talkerStreamIdExt is stored MSB-first - it's just an ID)
    SET_AVBTP_STREAM_ID0(pAVBHdr, stream_id0);

    // update the ...
    SET_AVBTP_SEQUENCE_NUMBER(pAVBHdr, sequence_number);
}


/** This configure AVB Talker buffer for a given stream configuration.
 *  It updates the static portion of Ehternet/AVB transport layer headers.
 */
void AVB1722_Talker_bufInit(unsigned char Buf0[],
        avb1722_Talker_StreamConfig_t *pStreamConfig,
        int vlanid)
{
    int i;
    unsigned char *Buf = &Buf0[2];
    AVB_Frame_t *pEtherHdr = (AVB_Frame_t *) &(Buf[0]);
    AVB_DataHeader_t *p1722Hdr = (AVB_DataHeader_t *) &(Buf[AVB_ETHERNET_HDR_SIZE]);

    unsigned data_block_size;

    // store the sample type
    switch (pStreamConfig->sampleType)
    {
    case AAF_32BIT:
        AVB1722_audioSampleType = AAF_32BIT;
        data_block_size = pStreamConfig->num_channels * 1;
        break;
    default:
        AVB1722_audioSampleType = AAF_32BIT;
        data_block_size = pStreamConfig->num_channels * 1;
        break;
    }

    debug_printf("pStreamConfig->num_channels %d\n", pStreamConfig->num_channels);

    // clear all the bytes in header.
    memset( (void *) Buf, 0, (AVB_ETHERNET_HDR_SIZE + AVB_TP_HDR_SIZE));

    // 1. Initialise the ethernet layer.
    // copy both Src/Dest MAC address
    for (i = 0; i < MAC_ADRS_BYTE_COUNT; i++) {
        pEtherHdr->DA[i] = pStreamConfig->destMACAdrs[i];
        pEtherHdr->SA[i] = pStreamConfig->srcMACAdrs[i];
    }
    SET_AVBTP_TPID(pEtherHdr, AVB_TPID);
    SET_AVBTP_PCP(pEtherHdr, AVB_DEFAULT_PCP);
    SET_AVBTP_CFI(pEtherHdr, AVB_DEFAULT_CFI);
    SET_AVBTP_VID(pEtherHdr, vlanid);
    SET_AVBTP_ETYPE(pEtherHdr, AVB_1722_ETHERTYPE);

    // 2. Initialise the AVB TP layer.
    // NOTE: Since the data structure is cleared before we only set the requird bits.
    SET_AVBTP_SV(p1722Hdr, 1); // set stream ID to valid.
    SET_AVBTP_STREAM_ID0(p1722Hdr, pStreamConfig->streamId[0]);
    SET_AVBTP_STREAM_ID1(p1722Hdr, pStreamConfig->streamId[1]);

    SET_AVBTP_SUBTYPE(p1722Hdr, pStreamConfig->subtype);

    SET_AVBTP_FORMAT_SPECIFIC(p1722Hdr, pStreamConfig->format_specific);

}

int avb1722_create_packet(unsigned char Buf0[],
        avb1722_Talker_StreamConfig_t *stream_info,
        ptp_time_info_mod64 *timeInfo,
        audio_frame_t *frame,
        int stream)
{
    unsigned int presentation_time = stream_info->timestamp;
    int timestamp_valid = stream_info->timestamp_valid;
    int num_channels = stream_info->num_channels;
    int current_samples_in_packet = stream_info->current_samples_in_packet;
    int stream_id0 = stream_info->streamId[0];
    unsigned int *map = stream_info->map;
    int total_samples_in_packet;
    int num_samples_per_channel;

    // align packet 2 chars into the buffer so that samples are
    // word align for fast copying.
    unsigned char *Buf = &Buf0[2];
    unsigned int *dest = (unsigned int *) &Buf[(AVB_ETHERNET_HDR_SIZE + AVB_TP_HDR_SIZE)];

    int stride = num_channels;
    unsigned ptp_ts = 0;
    int pkt_data_length;

    dest += (current_samples_in_packet * stride);

    // TODO support other sampling rates
    num_samples_per_channel = 6; // at 48kHz

    // Figure out the number of samples in the 1722 packet
    int num_samples_in_payload = num_samples_per_channel * stream_info->num_channels;

    for (int i = 0; i < num_channels; i++)
    {
        unsigned sample = frame->samples[map[i]];
        sample = byterev(sample);
        *dest = sample;
        dest += 1;
    }

    timestamp_valid = 1;
    presentation_time = frame->timestamp;

    current_samples_in_packet++;

    // samples_per_channel is the number of times we need to call this function
    // i.e. the number of audio frames we need to iterate through to get a full packet worth of samples
    if (current_samples_in_packet == num_samples_per_channel)
    {

        total_samples_in_packet = num_samples_per_channel * num_channels;

        pkt_data_length = total_samples_in_packet << 2;

        // perform required updates to header
        if (timestamp_valid) {
            ptp_ts = local_timestamp_to_ptp_mod32(presentation_time, timeInfo);
            ptp_ts = ptp_ts + stream_info->presentation_delay;
        }

        // Update timestamp value and valid flag.
        AVB1722_AVBTP_HeaderGen(Buf, timestamp_valid, ptp_ts, stream_info->sequence_number, stream_id0);

        stream_info->sequence_number++;
        stream_info->current_samples_in_packet = 0;
        stream_info->timestamp_valid = 0;
        return (AVB_ETHERNET_HDR_SIZE + AVB_TP_HDR_SIZE + pkt_data_length);
    }

    stream_info->timestamp_valid = timestamp_valid;
    stream_info->timestamp = presentation_time;
    stream_info->current_samples_in_packet = current_samples_in_packet;

    return 0;
}

#endif
