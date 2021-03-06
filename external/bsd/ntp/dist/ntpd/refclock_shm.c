/*	$NetBSD: refclock_shm.c,v 1.2 2013/12/28 03:20:14 christos Exp $	*/

/*
 * refclock_shm - clock driver for utc via shared memory
 * - under construction -
 * To add new modes: Extend or union the shmTime-struct. Do not
 * extend/shrink size, because otherwise existing implementations
 * will specify wrong size of shared memory-segment
 * PB 18.3.97
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntp_types.h"

#if defined(REFCLOCK) && defined(CLOCK_SHM)

#include "ntpd.h"
#undef fileno
#include "ntp_io.h"
#undef fileno
#include "ntp_refclock.h"
#undef fileno
#include "timespecops.h"
#undef fileno
#include "ntp_stdlib.h"

#undef fileno
#include <ctype.h>
#undef fileno

#ifndef SYS_WINNT
# include <sys/ipc.h>
# include <sys/shm.h>
# include <assert.h>
# include <unistd.h>
# include <stdio.h>
#endif

/*
 * This driver supports a reference clock attached thru shared memory
 */

/* Temp hack to simplify testing of the old mode. */
#define OLDWAY 0

/*
 * SHM interface definitions
 */
#define PRECISION       (-1)    /* precision assumed (0.5 s) */
#define REFID           "SHM"   /* reference ID */
#define DESCRIPTION     "SHM/Shared memory interface"

#define NSAMPLES        3       /* stages of median filter */

/*
 * Function prototypes
 */
static  int     shm_start       (int unit, struct peer *peer);
static  void    shm_shutdown    (int unit, struct peer *peer);
static  void    shm_poll        (int unit, struct peer *peer);
static  void    shm_timer       (int unit, struct peer *peer);
	int	shm_peek	(int unit, struct peer *peer);
	void	shm_clockstats  (int unit, struct peer *peer);

/*
 * Transfer vector
 */
struct  refclock refclock_shm = {
	shm_start,              /* start up driver */
	shm_shutdown,           /* shut down driver */
	shm_poll,		/* transmit poll message */
	noentry,		/* not used: control */
	noentry,		/* not used: init */
	noentry,		/* not used: buginfo */
	shm_timer,              /* once per second */
};

struct shmTime {
	int    mode; /* 0 - if valid is set:
		      *       use values,
		      *       clear valid
		      * 1 - if valid is set:
		      *       if count before and after read of values is equal,
		      *         use values
		      *       clear valid
		      */
	volatile int    count;
	time_t		clockTimeStampSec;
	int		clockTimeStampUSec;
	time_t		receiveTimeStampSec;
	int		receiveTimeStampUSec;
	int		leap;
	int		precision;
	int		nsamples;
	volatile int    valid;
	unsigned	clockTimeStampNSec;	/* Unsigned ns timestamps */
	unsigned	receiveTimeStampNSec;	/* Unsigned ns timestamps */
	int		dummy[8];
};

struct shmunit {
	struct shmTime *shm;	/* pointer to shared memory segment */

	/* debugging/monitoring counters - reset when printed */
	int ticks;		/* number of attempts to read data*/
	int good;		/* number of valid samples */
	int notready;		/* number of peeks without data ready */
	int bad;		/* number of invalid samples */
	int clash;		/* number of access clashes while reading */
};


struct shmTime *getShmTime(int);

struct shmTime *getShmTime (int unit) {
#ifndef SYS_WINNT
	int shmid=0;

	/* 0x4e545030 is NTP0.
	 * Big units will give non-ascii but that's OK
	 * as long as everybody does it the same way.
	 */
	shmid=shmget (0x4e545030 + unit, sizeof (struct shmTime),
		      IPC_CREAT | ((unit < 2) ? 0600 : 0666));
	if (shmid == -1) { /* error */
		msyslog(LOG_ERR, "SHM shmget (unit %d): %m", unit);
		return 0;
	}
	else { /* no error  */
		struct shmTime *p = (struct shmTime *)shmat (shmid, 0, 0);
		if (p == (struct shmTime *)-1) { /* error */
			msyslog(LOG_ERR, "SHM shmat (unit %d): %m", unit);
			return 0;
		}
		return p;
	}
#else
	char buf[10];
	LPSECURITY_ATTRIBUTES psec = 0;
	HANDLE shmid = 0;
	SECURITY_DESCRIPTOR sd;
	SECURITY_ATTRIBUTES sa;

	snprintf(buf, sizeof(buf), "NTP%d", unit);
	if (unit >= 2) { /* world access */
		if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
			msyslog(LOG_ERR,"SHM InitializeSecurityDescriptor (unit %d): %m", unit);
			return 0;
		}
		if (!SetSecurityDescriptorDacl(&sd, 1, 0, 0)) {
			msyslog(LOG_ERR, "SHM SetSecurityDescriptorDacl (unit %d): %m", unit);
			return 0;
		}
		sa.nLength=sizeof (SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = &sd;
		sa.bInheritHandle = 0;
		psec = &sa;
	}
	shmid = CreateFileMapping ((HANDLE)0xffffffff, psec, PAGE_READWRITE,
				 0, sizeof (struct shmTime), buf);
	if (!shmid) { /*error*/
		char buf[1000];

		FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
			       0, GetLastError (), 0, buf, sizeof (buf), 0);
		msyslog(LOG_ERR, "SHM CreateFileMapping (unit %d): %s", unit, buf);
		return 0;
	} else {
		struct shmTime *p = (struct shmTime *) MapViewOfFile (shmid,
								    FILE_MAP_WRITE, 0, 0, sizeof (struct shmTime));
		if (p == 0) { /*error*/
			char buf[1000];

			FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
				       0, GetLastError (), 0, buf, sizeof (buf), 0);
			msyslog(LOG_ERR,"SHM MapViewOfFile (unit %d): %s", unit, buf) 
			return 0;
		}
		return p;
	}
#endif
}
/*
 * shm_start - attach to shared memory
 */
static int
shm_start(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	struct shmunit *up;

	pp = peer->procptr;
	pp->io.clock_recv = noentry;
	pp->io.srcclock = peer;
	pp->io.datalen = 0;
	pp->io.fd = -1;

	up = emalloc_zero(sizeof(*up));

	up->shm = getShmTime(unit);

	/*
	 * Initialize miscellaneous peer variables
	 */
	memcpy((char *)&pp->refid, REFID, 4);
	if (up->shm != 0) {
		pp->unitptr = up;
		up->shm->precision = PRECISION;
		peer->precision = up->shm->precision;
		up->shm->valid = 0;
		up->shm->nsamples = NSAMPLES;
		pp->clockdesc = DESCRIPTION;
		return 1;
	} else {
		free(up);
		return 0;
	}
}


/*
 * shm_shutdown - shut down the clock
 */
static void
shm_shutdown(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	struct shmunit *up;

	pp = peer->procptr;
	up = pp->unitptr;

	if (NULL == up)
		return;
#ifndef SYS_WINNT
	/* HMS: shmdt() wants char* or const void * */
	(void) shmdt ((char *)up->shm);
#else
	UnmapViewOfFile (up->shm);
#endif
	free(up);
}


/*
 * shm_timer - called every second
 */
static  void
shm_timer(int unit, struct peer *peer)
{
	if (OLDWAY)
		return;

	shm_peek(unit, peer);
}


/*
 * shm_poll - called by the transmit procedure
 */
static void
shm_poll(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	int ok;

	pp = peer->procptr;

	if (OLDWAY) {
		ok = shm_peek(unit, peer);
		if (!ok)
			return;
	}

        /*
         * Process median filter samples. If none received, declare a
         * timeout and keep going.
         */
        if (pp->coderecv == pp->codeproc) {
                refclock_report(peer, CEVNT_TIMEOUT);
		shm_clockstats(unit, peer);
                return;
        }
	pp->lastref = pp->lastrec;
	refclock_receive(peer);
	shm_clockstats(unit, peer);
}

/*
 * shm_peek - try to grab a sample
 */
int shm_peek(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	struct shmunit *up;
	struct shmTime *shm;

	/*
	 * This is the main routine. It snatches the time from the shm
	 * board and tacks on a local timestamp.
	 */
	pp = peer->procptr;
	up = pp->unitptr;
	up->ticks++;
	if (up->shm == 0) {
		/* try to map again - this may succeed if meanwhile some-
		body has ipcrm'ed the old (unaccessible) shared mem segment */
		up->shm = getShmTime(unit);
	}
	shm = up->shm;
	if (shm == 0) {
		refclock_report(peer, CEVNT_FAULT);
		return 0;
	}
	if (shm->valid) {
		struct timespec tvr;
		struct timespec tvt;
		struct tm *t;
		char timestr[20];	/* "%Y-%m-%dT%H:%M:%S" + 1 */
		int c;
		int ok = 1;
		unsigned cns_new, rns_new;
		int cnt;

		tvr.tv_sec = 0;
		tvr.tv_nsec = 0;
		tvt.tv_sec = 0;
		tvt.tv_nsec = 0;
		switch (shm->mode) {
		    case 0:
			tvr.tv_sec	= shm->receiveTimeStampSec;
			tvr.tv_nsec	= shm->receiveTimeStampUSec * 1000;
			rns_new		= shm->receiveTimeStampNSec;
			tvt.tv_sec	= shm->clockTimeStampSec;
			tvt.tv_nsec	= shm->clockTimeStampUSec * 1000;
			cns_new		= shm->clockTimeStampNSec;

			/* Since these comparisons are between unsigned
			** variables they are always well defined, and any
			** (signed) underflow will turn into very large
			** unsigned values, well above the 1000 cutoff
			*/
			if (   ((cns_new - (unsigned)tvt.tv_nsec) < 1000)
			    && ((rns_new - (unsigned)tvr.tv_nsec) < 1000)) {
				tvt.tv_nsec = cns_new;
				tvr.tv_nsec = rns_new;
			}
			// At this point tvr and tvt contains valid ns-level
			// timestamps, possibly generated by extending the
			// old us-level timestamps

			break;

		    case 1:
			cnt = shm->count;

			tvr.tv_sec	= shm->receiveTimeStampSec;
			tvr.tv_nsec	= shm->receiveTimeStampUSec * 1000;
			rns_new		= shm->receiveTimeStampNSec;
			tvt.tv_sec	= shm->clockTimeStampSec;
			tvt.tv_nsec	= shm->clockTimeStampUSec * 1000;
			cns_new		= shm->clockTimeStampNSec;
			ok = (cnt == shm->count);

			/* Since these comparisons are between unsigned
			** variables they are always well defined, and any
			** (signed) underflow will turn into very large
			** unsigned values, well above the 1000 cutoff
			*/
			if (   ((cns_new - (unsigned)tvt.tv_nsec) < 1000)
			    && ((rns_new - (unsigned)tvr.tv_nsec) < 1000)) {
				tvt.tv_nsec = cns_new;
				tvr.tv_nsec = rns_new;
			}
			// At this point tvr and tvt contains valid ns-level
			// timestamps, possibly generated by extending the
			// old us-level timestamps

			break;

		    default:
			msyslog (LOG_ERR, "SHM: bad mode found in shared memory: %d",shm->mode);
			return 0;
		}

		/* XXX NetBSD has incompatible tv_sec */
		t = gmtime((const time_t *)&tvt.tv_sec);

		/* add ntpq -c cv timecode in ISO 8601 format */
		strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", t);
		c = snprintf(pp->a_lastcode, sizeof(pp->a_lastcode),
			     "%s.%09ldZ", timestr, (long)tvt.tv_nsec);
		pp->lencode = ((size_t)c < sizeof(pp->a_lastcode))
				 ? c
				 : 0;

		shm->valid = 0;
		if (ok) {
			pp->lastrec = tspec_stamp_to_lfp(tvr);
			pp->polls++;
			pp->day	= t->tm_yday+1;
			pp->hour = t->tm_hour;
			pp->minute = t->tm_min;
			pp->second = t->tm_sec;
			pp->nsec = tvt.tv_nsec;
			peer->precision = shm->precision;
			pp->leap = shm->leap;
		} else {
			refclock_report(peer, CEVNT_FAULT);
			msyslog (LOG_NOTICE, "SHM: access clash in shared memory");
			up->clash++;
			return 0;
		}
	} else {
		refclock_report(peer, CEVNT_TIMEOUT);
		up->notready++;
		return 0;
	}
	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		up->bad++;
		return 0;
	}
	up->good++;
	return 1;
}

/*
 * shm_clockstats - dump and reset counters
 */
void shm_clockstats(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	struct shmunit *up;
	char logbuf[256];

	pp = peer->procptr;
	up = pp->unitptr;

	if (!(pp->sloppyclockflag & CLK_FLAG4))
		return;

	snprintf(logbuf, sizeof(logbuf), "%3d %3d %3d %3d %3d",
		 up->ticks, up->good, up->notready, up->bad, up->clash);
	record_clock_stats(&peer->srcadr, logbuf);

	up->ticks = up->good = up->notready = up->bad = up->clash = 0;

}

#else
NONEMPTY_TRANSLATION_UNIT
#endif /* REFCLOCK */
