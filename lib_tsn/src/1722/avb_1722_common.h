// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
/**
 * \file avb_1722_common.h
 * \brief AVB 1722 common definitions
 */


#ifndef _AVB_COMMON_H_
#define _AVB_COMMON_H_ 1

short ntoh_16(unsigned char x[2]);
int ntoh_32(unsigned char x[4]) ;
void get_64(unsigned char g[8], unsigned char c[8]);
void set_64(unsigned char g[8], unsigned char c[8]);

void hton_16(unsigned char x[2], unsigned short v);

void hton_32(unsigned char x[4], unsigned int v);

static inline void hton_32_inline(unsigned char x[4], unsigned int v) {
  x[0] = (unsigned char) (v >> 24);
  x[1] = (unsigned char) (v >> 16);
  x[2] = (unsigned char) (v >> 8);
  x[3] = (unsigned char) (v);
}

// Network to/from Host conversion
#define NTOH_U16(x) ( ((unsigned) x[0] << 8) | ((unsigned) x[1]) )
#define NTOH_U32(x) ( ((unsigned) x[0] << 24) | ((unsigned) x[1] << 16) | ((unsigned) x[2] << 8) | ((unsigned) x[3]) )
#define NTOH_U32i(x,i) ( ((unsigned) x[i] << 24) | ((unsigned) x[i+1] << 16) | ((unsigned) x[i+2] << 8) | ((unsigned) x[i+3]) )


#define HTON_U16(x, v) do { x[0] = (v >> 8) & 0xFF; \
                            x[1] = (v & 0xFF); } while (0)
#define HTON_U32(x, v) do { x[0] = (unsigned char) (v >> 24);    \
                            x[1] = (unsigned char) (v >> 16); \
                            x[2] = (unsigned char) (v >> 8) ; \
                            x[3] = (unsigned char) (v); } while (0)

#define HTON_U32i(x, i, v) do { x[i+0] = (v >> 24);       \
                                x[i+1] = (v >> 16);                          \
                                x[i+2] = (v >> 8) ; \
                                x[i+3] = (v); } while (0)

// Frame Header size in bytes definations.
#define AVB_ETHERNET_HDR_SIZE    (18)
#define AVB_TP_HDR_SIZE          (24)

// number of bytes in MAC address
#define MAC_ADRS_BYTE_COUNT      (6)


// AVB frame header
typedef struct
{
  unsigned char DA[MAC_ADRS_BYTE_COUNT];    // dest MAC addr
  unsigned char SA[MAC_ADRS_BYTE_COUNT];    // src MAC addr
  unsigned char TPID[2];  // 802.1P/Q = 8100
  unsigned char flagVID[2];  // contains PCP (priority code point), bits 0,1,2.
                          //          CFI (canonical format indicator), bit 3.
                          // VID bits 4-15
  unsigned char etype[2];
}AVB_Frame_t;


// Macros to access the AVBTP transport layer.
// Usage:
// 1. "x" in following macros are pointer to valid AVBTP_FrameHeader.
// 2. Return the value of the item in HOST byte order.
#define AVBTP_PCP(x)          (x->flagVID[0] >> 5)
#define AVBTP_CFI(x)          ((x->flagVID[0] & 0x10) >> 4)
#define AVBTP_VID(x)          (((x->flagVID[0] & 0xF) << 8) | x->flagVID[1])
#define AVBTP_TPID(x)         ((x->TPID[0] << 8) | x->TPID[1])
#define AVBTP_ETYPE(x)        ((x->etype[0] << 8) | x->etype[1])

// Macros to set the AVBTP transport layer.
// Usgae:
// 1. "x" in following macros are pointer to valid AVBTP_FrameHeader.
// 2. "a" is the value in HOST byte order to set.
#define SET_AVBTP_PCP(x, a)   do{ x->flagVID[0] |= (a << 5); } while (0)
#define SET_AVBTP_CFI(x, a)   do{ x->flagVID[0] |= (a & 0x1) << 4; } while (0)
#define SET_AVBTP_VID(x, a)   do{ x->flagVID[0] |= ((a >> 8) & 0xF); \
                                  x->flagVID[1] = (a & 0xFF); } while (0)
#define SET_AVBTP_TPID(x, a)  do{ x->TPID[0] = (a >> 8); \
                                  x->TPID[1] = a & 0xFF; } while (0)
#define SET_AVBTP_ETYPE(x, a) do{ x->etype[0] = (a >> 8); \
                                  x->etype[1] = a & 0xFF; } while (0)

#define CLEAR_AVBTP_VID(x)   do{ x->flagVID[0] &= 0xF0; \
                                  x->flagVID[1] = 0; } while (0)

// AVB common stream data header format
typedef struct
{
  unsigned char subtype;           // bit 0-7 : subtype
  unsigned char version_flags;     // bit 0   : sv. stream id field valid.
                                   // bit 1-3 : version.
                                   // bit 4   : mr. media clock restart.
                                   // bit 5-6 : r. Reserved
                                   // bit 7   : tv. timestamp field valid
  unsigned char sequence_number;   //
  unsigned char reserved_tu;       // bit 0-6 : reserved
                                   // bit 7   : tu. timestamp uncertain.
  unsigned char stream_id[8];      // 802.1Qat Stream ID
  unsigned char avb_timestamp[4];  //
  unsigned char format_specific[4];// bit 0-7 : format
                                   // bit 8-11: nominal sampling rate
                                   // bit 12-13: rsv
                                   // bit 13-23: channels_per_frame
                                   // bit 24-31: bit-depth - 1
} AVB_DataHeader_t;


// Macros to access the AVB Common Header.
// Usage:
// 1. "x" in following macros are pointer to valid AVB Common Header.
// 2. Return the value of the item in HOST byte order.
#define AVBTP_SUBTYPE(x)               (x->subtype)
#define AVBTP_SV(x)                    (x->version_flags >> 7)
#define AVBTP_VERSION(x)               ((x->version_flags >> 4) & 0x7)
#define AVBTP_MR(x)                    ((x->version_flags >> 1) & 0x1)
#define AVBTP_TV(x)                    (x->version_flags & 0x1)
#define AVBTP_SEQUENCE_NUMBER(x)       (x->sequence_number)
#define AVBTP_TU(x)                     (x->reseved_tu & 1)
#define AVBTP_STREAM_ID1(x)            ((x->stream_id[0] << 24) | \
                                        (x->stream_id[1] << 16) | \
                                        (x->stream_id[2] << 8) | \
                                        (x->stream_id[3]))
#define AVBTP_STREAM_ID0(x)            ((x->stream_id[4] << 24) | \
                                        (x->stream_id[5] << 16) | \
                                        (x->stream_id[6] << 8) | \
                                        (x->stream_id[7]))
#define AVBTP_TIMESTAMP(x)             ((x->avb_timestamp[0] << 24) | \
                                        (x->avb_timestamp[1] << 16) | \
                                        (x->avb_timestamp[2] << 8) | \
                                        (x->avb_timestamp[3]))
#define AVBTP_FORMAT_SPECIFIC(x)       ((x->format_specific[0] << 24) | \
                                        (x->format_specific[1] << 16) | \
                                        (x->format_specific[2] << 8) | \
                                        (x->format_specific[3]))


// Macros to set the AVBTP transport layer.
// Usgae:
// 1. "x" in following macros are pointer to valid AVBTP_FrameHeader.
// 2. "a" is the value in HOST byte order to set.
#define SET_AVBTP_SUBTYPE(x, a)           ((x)->subtype |= ((a))
#define SET_AVBTP_SV(x, a)                ((x)->version_flags |= ((a) & 0x1) << 7)
#define SET_AVBTP_VERSION(x, a)           ((x)->version_flags |= ((a) & 0x7) << 4)
#define SET_AVBTP_MR(x, a)                ((x)->version_flags |= ((a) & 0x1) << 3)
#define SET_AVBTP_TV(x, a)                ((x)->version_flags = ((x)->version_flags & ~0x1) | ((a) & 0x1))
#define SET_AVBTP_SEQUENCE_NUMBER(x, a)   ((x)->sequence_number = ((a) & 0xff))
#define SET_AVBTP_TU(x, a)                ((x)->reseved_tu |= ((a) & 0x1))
#define SET_AVBTP_TIMESTAMP(x, a)         hton_32_inline((x)->avb_timestamp, (a))
#define SET_AVBTP_STREAM_ID1(x, a)        hton_32_inline(&(x)->stream_id[0], (a))
#define SET_AVBTP_STREAM_ID0(x, a)        hton_32_inline(&(x)->stream_id[4], (a))
#define SET_AVBTP_FORMAT_SPECIFIC(x, a)   do {  (x)->format_specific[0] = ((a) >> 24)& 0xFF; \
                                                (x)->format_specific[1] = ((a) >> 16)& 0xFF; \
                                                (x)->format_specific[2] = ((a) >> 8)& 0xFF; \
                                                (x)->format_specific[3] = ((a) >> 0)& 0xFF; } while (0)

// constants.
#define AVBTP_CD_DATA      (0)
#define AVBTP_CD_CONTROL   (1)
#define AVB_TPID           (0x8100)
#define AVB_DEFAULT_PCP    (0x3)
#define AVB_DEFAULT_CFI    (0x0)
#define AVB_DEFAULT_VID    (0x2)
#define AVB_1722_ETHERTYPE (0x22f0)

// Default to 2ms delay.
#define AVB_DEFAULT_PRESENTATION_TIME_DELAY_NS              (2000000)

#endif
