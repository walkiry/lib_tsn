// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#include <xccompat.h>
#include <xscope.h>
#include "xassert.h"
#include "gptp.h"
#include "avb_1722_def.h"
#include "print.h"
#include "media_clock_internal.h"
#include "media_clock_client.h"
#include "misc_timer.h"

// The clock recovery internal representation of the worldlen.  More precision and range than the external
// worldlen representation.  The max percision is 26 bits before the PTP clock recovery multiplcation overflows
#define WORDLEN_FRACTIONAL_BITS 16

/**
 *  \brief Records the state of the media stream
 *
 *  It is used by the stream based clock recovery to store stream state, and
 *  therefore work out deltas between the last clock recovery state and the
 *  current one
 */
typedef struct stream_info_t {
    int valid;
    unsigned int local_ts;
    unsigned int outgoing_ptp_ts;
    unsigned int presentation_ts;
    int locked;
    int fill;
} stream_info_t;

/**
 * \brief Records the state of the clock recovery for one media clock
 */
typedef struct clock_info_t {
    unsigned long long wordlen;
    long long ierror;
    long long perror;
    unsigned int rate;
    int first;
    stream_info_t stream_info1;
    stream_info_t stream_info2;
} clock_info_t;

/// The array of media clock state structures
static clock_info_t clock_states[AVB_NUM_MEDIA_CLOCKS];

static unsigned int previous_event_ptp;
static unsigned int previousprevious_event_ptp;

/**
 * \brief Converts the internal 64 bit wordlen into an external 32 bit wordlen
 */
static unsigned int local_wordlen_to_external_wordlen(unsigned long long w) {
    return (w >> (WORDLEN_FRACTIONAL_BITS - WC_FRACTIONAL_BITS));
}

static unsigned long long calculate_wordlen(unsigned int sample_rate) {
    const unsigned long long nanoseconds = (100000000LL << WORDLEN_FRACTIONAL_BITS);
    /* Calculate what master clock we should be using */
    if ((sample_rate % 48000) == 0) {
        return (nanoseconds / 48000);
    }
    else if ((sample_rate % 44100) == 0) {
        return (nanoseconds / 44100);
    }
    else {
        fail("Unsupported sample rate");
    }
}

void init_media_clock_recovery(chanend ptp_svr,
                               int clock_num,
                               unsigned int clk_time,
                               unsigned int rate) {
    clock_info_t *clock_info = &clock_states[clock_num];

    clock_info->first = 1;
    clock_info->rate = rate;
    clock_info->ierror = 0;
    clock_info->perror = 0;
    if (rate != 0) {
        clock_info->wordlen = calculate_wordlen(clock_info->rate);
    } else {
        clock_info->wordlen = 0;
    }

    clock_info->stream_info1.valid = 0;
    clock_info->stream_info2.valid = 0;
}

void update_media_clock_stream_info(int clock_index,
                                    unsigned int local_ts,
                                    unsigned int outgoing_ptp_ts,
                                    unsigned int presentation_ts,
                                    int locked,
                                    int fill) {
    clock_info_t *clock_info = &clock_states[clock_index];

    clock_info->stream_info2.local_ts = local_ts;
    clock_info->stream_info2.outgoing_ptp_ts = outgoing_ptp_ts;
    clock_info->stream_info2.presentation_ts = presentation_ts;
    clock_info->stream_info2.valid = 1;
    clock_info->stream_info2.locked = locked;
    clock_info->stream_info2.fill = fill;
}

void inform_media_clock_of_lock(int clock_index) {
    clock_info_t *clock_info = &clock_states[clock_index];
    clock_info->stream_info2.valid = 0;
}

#define MAX_ERROR_TOLERANCE 100

unsigned int update_media_clock(chanend ptp_svr,
                                int clock_index,
                                const media_clock_t *mclock,
                                unsigned int t2,
                                int period0) {
    clock_info_t *clock_info = &clock_states[clock_index];
    long long diff_local;
    int clock_type = mclock->info.clock_type;

    switch (clock_type) {
    case DEVICE_MEDIA_CLOCK_LOCAL_CLOCK:
        return local_wordlen_to_external_wordlen(clock_info->wordlen);
        break;

    case DEVICE_MEDIA_CLOCK_INPUT_STREAM_DERIVED: {
        long long ierror, perror, derror;

        // If the stream info isn't valid at all, then return the default clock rate
        if (!clock_info->stream_info2.valid)
            return local_wordlen_to_external_wordlen(clock_info->wordlen);

        // If there are not two stream infos to compare, then return default clock rate
        if (!clock_info->stream_info1.valid) {
            clock_info->stream_info1 = clock_info->stream_info2;
            clock_info->stream_info2.valid = 0;
            return local_wordlen_to_external_wordlen(clock_info->wordlen);
        }

        // If the stream is unlocked, return the default clock rate
        if (!clock_info->stream_info2.locked) {
            clock_info->wordlen = calculate_wordlen(clock_info->rate);
            clock_info->stream_info1 = clock_info->stream_info2;
            clock_info->stream_info2.valid = 0;
            clock_info->first = 1;
            clock_info->ierror = 0;
            clock_info->perror = 0;

        // We have all the info we need to perform clock recovery
        } else {
            diff_local = clock_info->stream_info2.local_ts
                    - clock_info->stream_info1.local_ts;

            debug_printf("\n");

            debug_printf("stream_info2.outgoing_ptp_ts %d\n", clock_info->stream_info2.outgoing_ptp_ts);
            debug_printf("stream_info2.presentation_ts %d\n", clock_info->stream_info2.presentation_ts);

            debug_printf("stream_info1.outgoing_ptp_ts %d\n", clock_info->stream_info1.outgoing_ptp_ts);
            debug_printf("stream_info1.presentation_ts %d\n", clock_info->stream_info1.presentation_ts);

            perror = (signed) clock_info->stream_info2.outgoing_ptp_ts -
                                 (signed) clock_info->stream_info2.presentation_ts;

            perror = perror << WORDLEN_FRACTIONAL_BITS;

            if (clock_info->first) {
                perror = 0;
                ierror = 0;
                clock_info->first = 0;
            } else {

                ierror = perror + clock_info->ierror;

                if (ierror > (long long) 1073741824 << WORDLEN_FRACTIONAL_BITS)
                    ierror = 1073741824<< WORDLEN_FRACTIONAL_BITS; // max value for ierror = 2^62
                else if (ierror < (long long) -1073741824 << WORDLEN_FRACTIONAL_BITS)
                    ierror = -1073741824 << WORDLEN_FRACTIONAL_BITS;

                derror = perror - clock_info->perror;
            }

            clock_info->ierror = ierror;
            clock_info->perror = perror;

            debug_printf("perror %x %x\n", (unsigned)(perror>>32), (unsigned)perror );
            debug_printf("ierror %x %x\n", (unsigned)(ierror>>32), (unsigned)ierror );
            debug_printf("derror %x %x\n", (unsigned)(derror>>32), (unsigned)derror );

            // These values were tuned for CS2100-CP PLL
            clock_info->wordlen = clock_info->wordlen
                    - ((ierror / diff_local) / 500)
                    ;//- ((perror / diff_local) / 10)
                    //- ((derror / diff_local) / 5);

            clock_info->stream_info1 = clock_info->stream_info2;
            previousprevious_event_ptp = previous_event_ptp;
            previous_event_ptp = mclock->event_ptp;
            clock_info->stream_info2.valid = 0;
        }
        break;
    }

        break;
    }

    return local_wordlen_to_external_wordlen(clock_info->wordlen);
}

