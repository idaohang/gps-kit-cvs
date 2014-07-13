#ifndef _gpsd_h_
#define _gpsd_h_

/* gpsd.h -- fundamental types and structures for the GPS daemon */

#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include "config.h"
#include "gps.h"

/* Some internal capabilities depend on which drivers we're compiling. */
#ifdef EARTHMATE_ENABLE
#define ZODIAC_ENABLE	
#endif
#if defined(ZODIAC_ENABLE) || defined(SIRFII_ENABLE) || defined(GARMIN_ENABLE) || defined(TSIP_ENABLE) || defined(EVERMORE_ENABLE) || defined(ITALK_ENABLE)
#define BINARY_ENABLE	
#endif
#if defined(TRIPMATE_ENABLE) || defined(BINARY_ENABLE)
#define NON_NMEA_ENABLE	
#endif

#define NMEA_MAX	82		/* max length of NMEA sentence */
#define NMEA_BIG_BUF	(2*NMEA_MAX+1)	/* longer than longest NMEA sentence */

/* factors for converting among confidence interval units */
#define CEP50_SIGMA	1.18
#define DRMS_SIGMA	1.414
#define CEP95_SIGMA	2.45

/* this is where we choose the confidence level to use in reports */
#define GPSD_CONFIDENCE	CEP95_SIGMA

enum isgpsstat_t {
    ISGPS_NO_SYNC, ISGPS_SYNC, ISGPS_SKIP, ISGPS_MESSAGE,
};
#define PREAMBLE_PATTERN 0x66

#define ISGPS_ERRLEVEL_BASE	5

#define NTPSHMSEGS	4		/* number of NTP SHM segments */

#define RTCM_MAX	(RTCM_WORDS_MAX * sizeof(isgps30bits_t))

struct gps_context_t {
    int valid;				/* member validity flags */
#define LEAP_SECOND_VALID	0x01	/* we have or don't need correction */
    /* DGPSIP status */
    bool sentdgps;			/* have we sent a DGPSIP R report? */
    int fixcnt;				/* count of good fixes seen */
    int dsock;				/* socket to DGPS server */
    ssize_t rtcmbytes;			/* byte count of last RTCM104 report */
    char rtcmbuf[RTCM_MAX];		/* last RTCM104 report */
    double rtcmtime;			/* timestamp of last RTCM104 report */ 
    /* timekeeping */
    int leap_seconds;			/* Unix seconds to UTC */
    int century;			/* for NMEA-only devices without ZDA */
#ifdef NTPSHM_ENABLE
    /*@reldef@*/struct shmTime *shmTime[NTPSHMSEGS];
    bool shmTimeInuse[NTPSHMSEGS];
# ifdef PPS_ENABLE
    bool shmTimePPS;
# endif /* PPS_ENABLE */
#endif /* NTPSHM_ENABLE */
};

struct gps_device_t;

struct gps_type_t {
/* GPS method table, describes how to talk to a particular GPS type */
    /*@observer@*/char *typename;
    /*@observer@*//*@null@*/char *trigger;
    int channels;
    /*@null@*/bool (*probe)(struct gps_device_t *session);
    /*@null@*/void (*initializer)(struct gps_device_t *session);
    /*@null@*/ssize_t (*get_packet)(struct gps_device_t *session);
    /*@null@*/gps_mask_t (*parse_packet)(struct gps_device_t *session);
    /*@null@*/ssize_t (*rtcm_writer)(struct gps_device_t *session, char *rtcmbuf, size_t rtcmbytes);
    /*@null@*/bool (*speed_switcher)(struct gps_device_t *session, speed_t speed);
    /*@null@*/void (*mode_switcher)(struct gps_device_t *session, int mode);
    /*@null@*/bool (*rate_switcher)(struct gps_device_t *session, double rate);
    int cycle_chars;
    /*@null@*/void (*wrapup)(struct gps_device_t *session);
    double cycle;
};

#if defined (HAVE_SYS_TERMIOS_H)
#include <sys/termios.h>
#else
#if defined (HAVE_TERMIOS_H)
#include <termios.h>
#endif
#endif

/*
 * The packet buffers need to be as long than the longest packet we
 * expect to see in any protocol, because we have to be able to hold
 * an entire packet for checksumming.  Thus, in particular, they need
 * to be as long as a SiRF MID 4 packet, 188 bytes payload plus eight bytes 
 * of header/length/checksum/trailer. 
 */
#define MAX_PACKET_LENGTH	196	/* 188 + 8 */

/*
 * We used to define the inbut buffer length as MAX_PACKET_LENGTH*2+1.
 * However, as it turns out, this isn't enough.  We've had a report
 * from one user with a GPS that reports at 20Hz that "sometimes a
 * long/slow context switch will cause the next read from the serial
 * device to be very big. I sometimes see a read of 250 characters or
 * more."
 */
#define INPUT_BUFFER_LENGTH	1024

struct gps_device_t {
/* session object, encapsulates all global state */
    struct gps_data_t gpsdata;
    /*@relnull@*/struct gps_type_t *device_type;
    struct gps_context_t	*context;
    double rtcmtime;	/* timestamp of last RTCM104 correction to GPS */
    struct termios ttyset, ttyset_old;
    /* packet-getter internals */
    int	packet_type;
#define BAD_PACKET	-1
#define NMEA_PACKET	0
#define SIRF_PACKET	1
#define ZODIAC_PACKET	2
#define TSIP_PACKET	3
#define EVERMORE_PACKET	4
#define ITALK_PACKET	5
#define RTCM_PACKET	6
    unsigned int baudindex;
    int saved_baud;
    unsigned int packet_state;
    size_t packet_length;
    unsigned char inbuffer[MAX_PACKET_LENGTH*2+1];
    size_t inbuflen;
    unsigned /*@observer@*/char *inbufptr;
    unsigned char outbuffer[MAX_PACKET_LENGTH+1];
    size_t outbuflen;
    unsigned long char_counter;		/* count characters processed */
    unsigned long retry_counter;	/* count sniff retries */
    double poll_times[FD_SETSIZE];	/* last daemon poll time */
#ifdef NTPSHM_ENABLE
    int shmTime;
# ifdef PPS_ENABLE
    int shmTimeP;
# endif /* PPS_ENABLE */
#endif /* NTPSHM_ENABLE */
    struct gps_fix_t lastfix;	/* use to compute uncertainties */
#ifdef BINARY_ENABLE
    double mag_var;		/* Magnetic variation in degrees */  
    /*
     * The rest of this structure is driver-specific private storage.
     * Because the Garmin driver uses a long buffer, you can have
     * up to 4096+12 bytes of private storage in your own union member
     * without making this structure larger or changing the API at all.
     */
    union {
#ifdef NMEA_ENABLE
	struct {
	    int part, await;		/* for tracking GSV parts */
	    struct tm date;
	    double subseconds;
	} nmea;
#endif /* NMEA_ENABLE */
#ifdef SIRFII_ENABLE
	struct {
	    unsigned int driverstate;	/* for private use */
#define SIRF_LT_231	0x01		/* SiRF at firmware rev < 231 */
#define SIRF_EQ_231     0x02            /* SiRF at firmware rev == 231 */
#define SIRF_GE_232     0x04            /* SiRF at firmware rev >= 232 */
#define UBLOX   	0x08		/* uBlox firmware with packet 0x62 */
	    unsigned long satcounter;
#ifdef NTPSHM_ENABLE
	    unsigned int time_seen;
#define TIME_SEEN_GPS_1	0x01	/* Seen GPS time variant 1? */
#define TIME_SEEN_GPS_2	0x02	/* Seen GPS time variant 2? */
#define TIME_SEEN_UTC_1	0x04	/* Seen UTC time variant 1? */
#define TIME_SEEN_UTC_2	0x08	/* Seen UTC time variant 2? */
#endif /* NTPSHM_ENABLE */
	} sirf;
#endif /* SIRFII_ENABLE */
#ifdef TSIP_ENABLE
	struct {
	    int16_t gps_week;		/* Current GPS week number */
	    bool superpkt;		/* Super Packet mode requested */
	    time_t last_41;		/* Timestamps for packet requests */
	    time_t last_5c;
	    time_t last_6d;
	    unsigned int parity, stopbits; /* saved RS232 link parameters */
	} tsip;
#endif /* TSIP_ENABLE */
#ifdef GARMIN_ENABLE	/* private housekeeping stuff for the Garmin driver */
	struct {
	    unsigned char Buffer[4096+12];	/* Garmin packet buffer */
	    size_t BufferLen;		/* current GarminBuffer Length */
	} garmin;
#endif /* GARMIN_ENABLE */
#ifdef ZODIAC_ENABLE	/* private housekeeping stuff for the Zodiac driver */
	struct {
	    unsigned short sn;		/* packet sequence number */
	    /*
	     * Zodiac chipset channel status from PRWIZCH. Keep it so
	     * raw-mode translation of Zodiac binary protocol can send
	     * it up to the client.
	     */
#define ZODIAC_CHANNELS	12
	    unsigned int Zs[ZODIAC_CHANNELS];	/* satellite PRNs */
	    unsigned int Zv[ZODIAC_CHANNELS];	/* signal values (0-7) */
	} zodiac;
#endif /* ZODIAC_ENABLE */
	/*
	 * This is not conditionalized on RTCM104_ENABLE because we need to
	 * be able to build rtcmdecode even when RTCM support is not
	 * configured in the daemon.  It doesn't take up extra space.
	 */
	struct {
	    /* ISGPS200 decodng */
	    bool            locked;
	    int             curr_offset;
	    isgps30bits_t   curr_word;
	    isgps30bits_t   buf[RTCM_WORDS_MAX];
	    unsigned int    bufindex;
	} isgps;
    } driver;
#endif /* BINARY_ENABLE */
};

#define IS_HIGHEST_BIT(v,m)	(v & ~((m<<1)-1))==0

/* here are the available GPS drivers */
extern struct gps_type_t **gpsd_drivers;

/* GPS library internal prototypes */
extern gps_mask_t nmea_parse_input(struct gps_device_t *);
extern gps_mask_t nmea_parse(char *, struct gps_device_t *);
extern int nmea_send(int, const char *, ... );
extern void nmea_add_checksum(char *);

ssize_t pass_rtcm(struct gps_device_t *, char *, size_t);

extern gps_mask_t sirf_parse(struct gps_device_t *, unsigned char *, size_t);
extern gps_mask_t evermore_parse(struct gps_device_t *, unsigned char *, size_t);
extern void packet_reset(struct gps_device_t *);
extern void packet_pushback(struct gps_device_t *);
extern ssize_t packet_process(struct gps_device_t *, size_t);
extern ssize_t packet_get(struct gps_device_t *);
extern int packet_sniff(struct gps_device_t *);

extern int dgpsip_open(struct gps_context_t *, const char *);
extern void dgpsip_poll(struct gps_context_t *);
extern void dgpsip_relay(struct gps_device_t *);
extern void dgpsip_report(struct gps_device_t *);
extern void dgpsip_autoconnect(struct gps_context_t *, 
			       double, double, const char *);

extern int gpsd_open(struct gps_device_t *);
extern bool gpsd_write(struct gps_device_t *, void const *, size_t);
extern bool gpsd_next_hunt_setting(struct gps_device_t *);
extern int gpsd_switch_driver(struct gps_device_t *, char *);
extern void gpsd_set_speed(struct gps_device_t *, speed_t, unsigned char, unsigned int);
extern speed_t gpsd_get_speed(struct termios *);
extern void gpsd_close(struct gps_device_t *);

extern void gpsd_zero_satellites(/*@out@*/struct gps_data_t *sp)/*@modifies sp@*/;
/* extern void gpsd_interpret_subframe(struct gps_device_t *, unsigned int[]); */
extern void gpsd_interpret_subframe(unsigned int[]);
extern /*@ observer @*/ char *gpsd_hexdump(const void *, size_t);
extern int netlib_connectsock(const char *, const char *, const char *);

extern int ntpshm_init(struct gps_context_t *, bool);
extern int ntpshm_alloc(struct gps_context_t *);
extern bool ntpshm_free(struct gps_context_t *, int);
extern int ntpshm_put(struct gps_device_t *, double);
extern int ntpshm_pps(struct gps_device_t *,struct timeval *);

extern void isgps_init(/*@out@*/struct gps_device_t *);
enum isgpsstat_t isgps_decode(struct gps_device_t *, 
			      bool (*preamble_match)(isgps30bits_t *),
			      bool (*length_check)(struct gps_device_t *),
			      size_t,
			      unsigned int);
extern unsigned int isgps_parity(isgps30bits_t);
extern enum isgpsstat_t rtcm_decode(struct gps_device_t *, unsigned int);

extern void rtcm_dump(struct gps_device_t *, /*@out@*/char[], size_t);
extern int rtcm_undump(/*@out@*/struct rtcm_t *, char *);
extern void rtcm_unpack(struct gps_device_t *);
extern bool rtcm_repack(struct gps_device_t *);

extern void ecef_to_wgs84fix(struct gps_data_t *,
			     double, double, double, 
			     double, double, double);
extern gps_mask_t dop(struct gps_data_t *);

/* srecord.c */
extern void hexdump(size_t, unsigned char *, unsigned char *);
extern unsigned char sr_sum(unsigned int, unsigned int, unsigned char *);
extern int bin2srec(unsigned int, unsigned int, unsigned int, unsigned char *, unsigned char *);
extern int srec_hdr(unsigned int, unsigned char *, unsigned char *);
extern int srec_fin(unsigned int, unsigned char *);
extern unsigned char hc(unsigned char);

/* exported bits for the GPS flasher */
bool sirf_write(int fd, unsigned char *msg);

/* application interface */
extern void gpsd_init(struct gps_device_t *, struct gps_context_t *, char *);
extern int gpsd_activate(struct gps_device_t *);
extern void gpsd_deactivate(struct gps_device_t *);
extern gps_mask_t gpsd_poll(struct gps_device_t *);
extern void gpsd_wrap(struct gps_device_t *);

/* caller should supply this */
void gpsd_report(int, const char *, ...);

#ifdef S_SPLINT_S
extern bool finite(double);
extern int cfmakeraw(struct termios *);
extern struct protoent *getprotobyname(const char *);
extern /*@observer@*/char *strptime(const char *,const char *tp,/*@out@*/struct tm *)/*@modifies tp@*/;
extern struct tm *gmtime_r(const time_t *,/*@out@*/struct tm *tp)/*@modifies tp@*/;
extern struct tm *localtime_r(const time_t *,/*@out@*/struct tm *tp)/*@modifies tp@*/;
extern float roundf(float x);
#endif /* S_SPLINT_S */

/* some OSes don't have round(). fake it if need be */
#ifndef HAVE_ROUND
#define	round(x) ((double)rint(x))
#define roundf(x) ((float)rintf(x))
#endif /* !HAVE_ROUND */

/* OpenBSD and FreeBSD don't seem to have NAN, NetBSD does, others? */
/* XXX test for this in configure? */
#if defined(__OpenBSD__) || defined(__FreeBSD__)
#ifndef NAN
#define NAN (0.0/0.0)
#endif /* !NAN */
#endif /* list of Operating Systems */

#endif /* _gpsd_h_ */
