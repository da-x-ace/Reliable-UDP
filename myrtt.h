#include	"unp.h"


struct my_rtt_info {
  uint32_t	rtt_rtt;	/* most recent measured RTT, in milliseconds */
  uint32_t	rtt_srtt;	/* smoothed RTT estimator, in milliseconds */
  uint32_t	rtt_rttvar;	/* smoothed mean deviation, in milliseconds */
  uint32_t	rtt_rto;	/* current RTO to use, in milliseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint64_t	rtt_base;	/* # millisec since 1/1/1970 at start */
};

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in milliseconds */
#define	RTT_RXTMAX      3000	/* max retransmit timeout value, in milliseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

				/* function prototypes */
void	 my_rtt_debug(struct my_rtt_info *);
void	 my_rtt_init(struct my_rtt_info *);
void	 my_rtt_newpack(struct my_rtt_info *);
int	 my_rtt_start(struct my_rtt_info *);
void	 my_rtt_stop(struct my_rtt_info *, uint32_t);
int	 my_rtt_timeout(struct my_rtt_info *);
uint32_t my_rtt_ts(struct my_rtt_info *);

extern int	rtt_d_flag;	/* can be set to nonzero for addl info */

