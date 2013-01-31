#include	"myrtt.h"

int rtt_d_flag=0;

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4.0 * (ptr)->rtt_rttvar))

static uint32_t my_rtt_minmax(uint32_t rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void my_rtt_init(struct my_rtt_info *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = ((tv.tv_sec)*1000) + ((tv.tv_usec)/1000);		/* # milliseconds since 1/1/1970 at start */

	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 0.75*1000;
	ptr->rtt_rto = my_rtt_minmax(RTT_RTOCALC(ptr));
		/* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
}

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */

/* include rtt_ts */
uint32_t my_rtt_ts(struct my_rtt_info *ptr)
{
	uint64_t		ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	
	
	ts = (((tv.tv_sec)*1000 - ptr->rtt_base)) + (tv.tv_usec / 1000);
	return((uint32_t)ts);
}

void my_rtt_newpack(struct my_rtt_info *ptr)
{
	ptr->rtt_nrexmt = 0;
}

int my_rtt_start(struct my_rtt_info *ptr)
{
	return((ptr->rtt_rto));		
		/* 4return value can be used as: alarm(rtt_start(&foo)) */
}

/* end rtt_ts */

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */

/* include rtt_stop */
void my_rtt_stop(struct my_rtt_info *ptr, uint32_t ms)
{
	int		delta;

	ptr->rtt_rtt = ms;		/* measured RTT in milliseconds */

	/*
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 * We use floating point here for simplicity.
	 */
	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt += delta / 8;		/* g = 1/8 */

	if (delta < 0)
		delta = -delta;				/* |delta| */

	ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4;	/* h = 1/4 */

	ptr->rtt_rto = my_rtt_minmax(RTT_RTOCALC(ptr));
}
/* end rtt_stop */

/* include rtt_timeout */
int my_rtt_timeout(struct my_rtt_info *ptr)
{
	ptr->rtt_rto *= 2;		/* next RTO */

	ptr->rtt_rto = my_rtt_minmax(ptr->rtt_rto);

	if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return(-1);			/* time to give up for this packet */
	return(0);
}
/* end rtt_timeout *

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */

void my_rtt_debug(struct my_rtt_info *ptr)
{
	if (rtt_d_flag == 0)
		return;

	fprintf(stderr, "rtt = %.3d, srtt = %.3d, rttvar = %.3d, rto = %.3d\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}

