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

//#define PRINT_PRESENTATION 1

// The clock recovery internal representation of the worldlen.  More precision and range than the external
// worldlen representation.  The max percision is 26 bits before the PTP clock recovery multiplcation overflows
#define WORDLEN_FRACTIONAL_BITS 24

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
	const unsigned long long nanoseconds = (100000000LL << WORDLEN_FRACTIONAL_BITS); // 100000000 ticks per seconds for a 100MHz clock?
	/* Calculate what master clock we should be using */
	if ((sample_rate % 48000) == 0) {
	    return (nanoseconds / 48000); // 2083 ticks per sample period
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
	clock_info->stream_info2.outgoing_ptp_ts = outgoing_ptp_ts; // is usually calculated from local_ts
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
	long long diff_event;
	int clock_type = mclock->info.clock_type;

	//debug_printf("update_media_clock\n");

	switch (clock_type) {
	case DEVICE_MEDIA_CLOCK_LOCAL_CLOCK:
	    //debug_printf("DEVICE_MEDIA_CLOCK_LOCAL_CLOCK\n");
		return local_wordlen_to_external_wordlen(clock_info->wordlen);
		break;

	case DEVICE_MEDIA_CLOCK_INPUT_STREAM_DERIVED: {
	    //debug_printf("DEVICE_MEDIA_CLOCK_INPUT_STREAM_DERIVED\n");
		long long ierror, perror;

		// If the stream info isn't valid at all, then return the default clock rate
		if (!clock_info->stream_info2.valid) {
		    //debug_printf("Stream info 2 not valid\n");
			return local_wordlen_to_external_wordlen(clock_info->wordlen);
		}

		// If there are not two stream infos to compare, then return default clock rate
		if (!clock_info->stream_info1.valid) {
            //debug_printf("Stream info 1 not valid\n");
			clock_info->stream_info1 = clock_info->stream_info2;
			clock_info->stream_info2.valid = 0;
			return local_wordlen_to_external_wordlen(clock_info->wordlen);
		}

		// If the stream is unlocked, return the default clock rate
		if (!clock_info->stream_info2.locked) {
            //debug_printf("Stream unlocked\n");
			clock_info->wordlen = calculate_wordlen(clock_info->rate);
			clock_info->stream_info1 = clock_info->stream_info2;
			clock_info->stream_info2.valid = 0;
			clock_info->first = 1;
			clock_info->ierror = 0;

		// We have all the info we need to perform clock recovery
		} else {
            //debug_printf("clock_info valid\n");
		    debug_printf("\n");

#if 1
            // wordlen is in (XMOS-Timerticks per second) << 24
            // 20.83 us = 136533333 external wordlen << 8 >> 24 / 100
            debug_printf("external wordlen %d\n", local_wordlen_to_external_wordlen(clock_info->wordlen));

            //debug_printf("%d %d\n", clock_info->stream_info1.presentation_ts, clock_info->stream_info2.presentation_ts);

            // Calcualte difference of current and previous timestamps
            // Note: This timestamps are based on ptp!
            unsigned int diff_presentation = clock_info->stream_info2.presentation_ts -
                                                            clock_info->stream_info1.presentation_ts;
            debug_printf("diff_presentation %d\n", diff_presentation);

            // Note: Local timestamps are based on 100MHz clock?
            diff_local = clock_info->stream_info2.local_ts
                    - clock_info->stream_info1.local_ts;
            debug_printf("diff_local %d\n",  diff_local);

            diff_event = (signed)mclock->event_ptp - (signed)previous_event_ptp;

            debug_printf("diff_event %d\n",  diff_event);
            debug_printf("clock_info->stream_info1.presentation_ts %d\n", clock_info->stream_info1.presentation_ts);
            debug_printf("clock_info->stream_info2.presentation_ts %d\n", clock_info->stream_info2.presentation_ts);

            debug_printf("clock_info->stream_info1.outgoing_ptp_ts %d\n", clock_info->stream_info1.outgoing_ptp_ts);
            debug_printf("clock_info->stream_info2.outgoing_ptp_ts %d\n", clock_info->stream_info2.outgoing_ptp_ts);

            //debug_printf("mclock->event_ptp %d\n",  mclock->event_ptp);
            //debug_printf("previous_event_ptp %d\n",  previous_event_ptp);

            // This is ptp time in ns!
            ierror = (signed)clock_info->stream_info2.outgoing_ptp_ts -
                    (signed)clock_info->stream_info2.presentation_ts;
            ierror = ierror << WORDLEN_FRACTIONAL_BITS;

            if (clock_info->first) {
                perror = 0;
                clock_info->first = 0;
            } else
                perror = ierror - clock_info->ierror;

            clock_info->ierror = ierror;

#ifndef PRINT_PRESENTATION
            debug_printf("perror %d\n",(signed)(perror>>32));
            debug_printf("ierror %d\n",(signed)(ierror>>32));
#endif


#if 1
            clock_info->wordlen = clock_info->wordlen -
                    ((perror / diff_local) * 800)/11 - //Faktor erhöht, da clock langsamer wurde, nochmal erhöht
                    ((ierror / diff_local) * 1) / 50; // Faktor verringert, da ierror immer negativ und clock sich somit verlangsamt, nochmal verringert, wieder verringert
#else
            clock_info->wordlen = clock_info->wordlen -
                    ((perror / diff_local) * 80)/11 -
                    ((ierror / diff_local) * 1) / 5;
#endif

            // Copy the current stream info and wait for a update of stream info
            clock_info->stream_info1 = clock_info->stream_info2;
            previousprevious_event_ptp = previous_event_ptp;
            previous_event_ptp = mclock->event_ptp;
            clock_info->stream_info2.valid = 0;

#else
            // Note: Local timestamps are based on 100MHz clock?

            // diff_local is round about 20 ms
			diff_local = clock_info->stream_info2.local_ts
					- clock_info->stream_info1.local_ts;

			debug_printf("diff_local %d\n",diff_local);

			// This is ptp time in ns!
			ierror = (signed) clock_info->stream_info2.outgoing_ptp_ts -
					 (signed) clock_info->stream_info2.presentation_ts;

			debug_printf("ierror %d ns\n",ierror);

			ierror = ierror << WORDLEN_FRACTIONAL_BITS;

			if (clock_info->first) {
				perror = 0;
				clock_info->first = 0;
			} else
				perror = ierror - clock_info->ierror;

			clock_info->ierror = ierror;

			debug_printf("perror %d\n",perror);

#if 0
			// These values were tuned for CS2300-CP PLL
			clock_info->wordlen = clock_info->wordlen - ((perror / diff_local) * 32) - ((ierror / diff_local) / 4);
#else
			debug_printf("old wordlen %d\n", clock_info->wordlen);
			// and these for CS2100-CP PLL
			// Note: Value is shifted multiplied by 25, divided by 2 and then shifted by 16 to get fractional baselength (We have 16 Fractional Bits!)
			clock_info->wordlen = clock_info->wordlen - ((perror / diff_local) * 80)/11 - ((ierror / diff_local) * 1) / 5;
#endif

			debug_printf("new wordlen %d\n", clock_info->wordlen);

			clock_info->stream_info1 = clock_info->stream_info2;
			clock_info->stream_info2.valid = 0;
#endif
		}
		break;
	}

		break;
	}

	return local_wordlen_to_external_wordlen(clock_info->wordlen);
}

