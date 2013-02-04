#include "globals.h"
#include <getopt.h>

#include "csctapi/cardreaders.h"
#include "modules.h"
#include "readers.h"

#include "extapi/coolapi.h"
#include "module-anticasc.h"
#include "module-cacheex.h"
#include "module-cccam.h"
#include "module-dvbapi-azbox.h"
#include "module-dvbapi-mca.h"
#include "module-ird-guess.h"
#include "module-lcd.h"
#include "module-led.h"
#include "module-stat.h"
#include "module-webif.h"
#include "oscam-chk.h"
#include "oscam-client.h"
#include "oscam-config.h"
#include "oscam-ecm.h"
#include "oscam-emm.h"
#include "oscam-files.h"
#include "oscam-garbage.h"
#include "oscam-lock.h"
#include "oscam-net.h"
#include "oscam-reader.h"
#include "oscam-string.h"
#include "oscam-time.h"
#include "oscam-work.h"
#include "reader-common.h"

/*****************************************************************************
        Globals
*****************************************************************************/
const char *syslog_ident = "oscam";
char *oscam_pidfile = NULL;
char default_pidfile[64];

int32_t exit_oscam=0;
struct s_module modules[CS_MAX_MOD];
struct s_cardsystem cardsystems[CS_MAX_MOD];
struct s_cardreader cardreaders[CS_MAX_MOD];

struct s_client * first_client = NULL; //Pointer to clients list, first client is master
struct s_client * first_client_hashed[CS_CLIENT_HASHBUCKETS];  // Alternative hashed client list
struct s_reader * first_active_reader = NULL; //list of active readers (enable=1 deleted = 0)
LLIST * configured_readers = NULL; //list of all (configured) readers

uint16_t  len4caid[256];    // table for guessing caid (by len)
char  cs_confdir[128]=CS_CONFDIR;
uint16_t cs_dblevel=0;   // Debug Level
int32_t thread_pipe[2] = {0, 0};
int8_t cs_restart_mode=1; //Restartmode: 0=off, no restart fork, 1=(default)restart fork, restart by webif, 2=like=1, but also restart on segfaults
uint8_t cs_http_use_utf8 = 0;
int8_t cs_capture_SEGV=0;
int8_t cs_dump_stack=0;
uint16_t cs_waittime = 60;
char  cs_tmpdir[200]={0x00};
CS_MUTEX_LOCK system_lock;
CS_MUTEX_LOCK config_lock;
CS_MUTEX_LOCK gethostbyname_lock;
CS_MUTEX_LOCK clientlist_lock;
CS_MUTEX_LOCK readerlist_lock;
CS_MUTEX_LOCK fakeuser_lock;
CS_MUTEX_LOCK ecmcache_lock;
CS_MUTEX_LOCK readdir_lock;
CS_MUTEX_LOCK hitcache_lock;
pthread_key_t getclient;
static int32_t bg;
static int32_t gbdb;
static int32_t max_pending = 32;

struct s_client *timecheck_client;

//Cache for  ecms, cws and rcs:
struct ecm_request_t	*ecmcwcache = NULL;
uint32_t ecmcwcache_size = 0;

struct  s_config  cfg;

int log_remove_sensitive = 1;

char    *prog_name = NULL;
char    *processUsername = NULL;

/*****************************************************************************
        Statics
*****************************************************************************/
#define _check(CONFIG_VAR, text) \
	do { \
		if (config_##CONFIG_VAR()) \
			printf(" %s", text); \
	} while(0)

/* Prints usage information and information about the built-in modules. */
static void show_usage(void)
{
	printf("%s",
"  ___  ____   ___\n"
" / _ \\/ ___| / __|__ _ _ __ ___\n"
"| | | \\___ \\| |  / _` | '_ ` _ \\\n"
"| |_| |___) | |_| (_| | | | | | |\n"
" \\___/|____/ \\___\\__,_|_| |_| |_|\n\n");
	printf("OSCam cardserver v%s, build r%s (%s)\n", CS_VERSION, CS_SVN_VERSION, CS_TARGET);
	printf("Copyright (C) 2009-2013 OSCam developers.\n");
	printf("This program is distributed under GPLv3.\n");
	printf("OSCam is based on Streamboard mp-cardserver v0.9d written by dukat\n");
	printf("Visit http://www.streamboard.tv/oscam/ for more details.\n\n");

	printf(" Features   :");
	_check(WEBIF, "webif");
	_check(TOUCH, "touch");
	_check(MODULE_MONITOR, "monitor");
	_check(WITH_SSL, "ssl");
	if (!config_WITH_STAPI())
		_check(HAVE_DVBAPI, "dvbapi");
	else
		_check(WITH_STAPI, "dvbapi_stapi");
	_check(IRDETO_GUESSING, "irdeto-guessing");
	_check(CS_ANTICASC, "anticascading");
	_check(WITH_DEBUG, "debug");
	_check(WITH_LB, "loadbalancing");
	_check(LCDSUPPORT, "lcd");
	_check(LEDSUPPORT, "led");
	printf("\n");

	printf(" Protocols  :");
	_check(MODULE_CAMD33, "camd33");
	_check(MODULE_CAMD35, "camd35_udp");
	_check(MODULE_CAMD35_TCP, "camd35_tcp");
	_check(MODULE_NEWCAMD, "newcamd");
	_check(MODULE_CCCAM, "cccam");
	_check(MODULE_CCCSHARE, "cccam_share");
	_check(MODULE_PANDORA, "pandora");
	_check(MODULE_GHTTP, "ghttp");
	_check(CS_CACHEEX, "cache-exchange");
	_check(MODULE_GBOX, "gbox");
	_check(MODULE_RADEGAST, "radegast");
	_check(MODULE_SERIAL, "serial");
	_check(MODULE_CONSTCW, "constcw");
	printf("\n");

	printf(" Readers    :");
	_check(READER_NAGRA, "nagra");
	_check(READER_IRDETO, "irdeto");
	_check(READER_CONAX, "conax");
	_check(READER_CRYPTOWORKS, "cryptoworks");
	_check(READER_SECA, "seca");
	_check(READER_VIACCESS, "viaccess");
	_check(READER_VIDEOGUARD, "videoguard");
	_check(READER_DRE, "dre");
	_check(READER_TONGFANG, "tongfang");
	_check(READER_BULCRYPT, "bulcrypt");
	_check(READER_GRIFFIN, "griffin");
	printf("\n");

	printf(" CardReaders:");
	_check(CARDREADER_PHOENIX, "phoenix");
	_check(CARDREADER_INTERNAL_AZBOX, "internal_azbox");
	_check(CARDREADER_INTERNAL_COOLAPI, "internal_coolapi");
	_check(CARDREADER_INTERNAL_SCI, "internal_sci");
	_check(CARDREADER_SC8IN1, "sc8in1");
	_check(CARDREADER_MP35, "mp35");
	_check(CARDREADER_SMARGO, "smargo");
	_check(CARDREADER_PCSC, "pcsc");
	_check(CARDREADER_SMART, "smartreader");
	_check(CARDREADER_DB2COM, "db2com");
	_check(CARDREADER_STAPI, "stapi");
	printf("\n");
	printf(" ConfigDir  : %s\n", CS_CONFDIR);
	printf("\n");
	printf(" Usage: oscam [parameters]\n");
	printf("\n Directories:\n");
	printf(" -c, --config-dir <dir>  | Read configuration files from <dir>.\n");
	printf("                         . Default: %s\n", CS_CONFDIR);
	printf(" -t, --temp-dir <dir>    | Set temporary directory to <dir>.\n");
#if defined(__CYGWIN__)
	printf("                         . Default: (OS-TMP)\n");
#else
	printf("                         . Default: /tmp/.oscam\n");
#endif
	printf("\n Startup:\n");
	printf(" -b, --daemon            | Start in the background as daemon.\n");
	printf(" -B, --pidfile <pidfile> | Create pidfile when starting.\n");
	if (config_WEBIF()) {
	printf(" -r, --restart <level>   | Set restart level:\n");
	printf("                         .   0 - Restart disabled (exit on restart request).\n");
	printf("                         .   1 - WebIf restart is active (default).\n");
	printf("                         .   2 - Like 1, but also restart on segfaults.\n");
	}
	printf(" -w, --wait <secs>       | Set how much seconds to wait at startup for the\n");
	printf("                         . system clock to be set correctly. Default: 60\n");
	printf("\n Logging:\n");
	printf(" -I, --syslog-ident <ident> | Set syslog ident. Default: oscam\n");
	printf(" -S, --show-sensitive    | Do not filter sensitive info (card serials, boxids)\n");
	printf("                         . from the logs.\n");
	printf(" -d, --debug <level>     | Set debug level mask used for logging:\n");
	printf("                         .     0 - No extra debugging (default).\n");
	printf("                         .     1 - Detailed error messages.\n");
	printf("                         .     2 - ATR parsing info, ECM, EMM and CW dumps.\n");
	printf("                         .     4 - Traffic from/to the reader.\n");
	printf("                         .     8 - Traffic from/to the clients.\n");
	printf("                         .    16 - Traffic to the reader-device on IFD layer.\n");
	printf("                         .    32 - Traffic to the reader-device on I/O layer.\n");
	printf("                         .    64 - EMM logging.\n");
	printf("                         .   128 - DVBAPI logging.\n");
	printf("                         .   256 - Loadbalancer logging.\n");
	printf("                         .   512 - CACHEEX logging.\n");
	printf("                         .  1024 - Client ECM logging.\n");
	printf("                         . 65535 - Debug all.\n");
	printf("\n Settings:\n");
	printf(" -p, --pending-ecm <num> | Set the maximum number of pending ECM packets.\n");
	printf("                         . Default: 32 Max: 255\n");
	if (config_WEBIF()) {
	printf(" -u, --utf8              | Enable WebIf support for UTF-8 charset.\n");
	}
	printf("\n Debug parameters:\n");
	printf(" -a, --crash-dump        | Write oscam.crash file on segfault. This option\n");
	printf("                         . needs GDB to be installed and OSCam executable to\n");
	printf("                         . contain the debug information (run oscam-XXXX.debug)\n");
	printf(" -s, --capture-segfaults | Capture segmentation faults.\n");
	printf(" -g, --gcollect <mode>   | Garbage collector debug mode:\n");
	printf("                         .   1 - Immediate free.\n");
	printf("                         .   2 - Check for double frees.\n");
	printf("\n Information:\n");
	printf(" -h, --help              | Show command line help text.\n");
	printf(" -V, --build-info        | Show OSCam binary configuration and version.\n");
}
#undef _check

/* Keep the options sorted */
static const char short_options[] = "aB:bc:d:g:hI:p:r:Sst:uVw:";

/* Keep the options sorted by short option */
static const struct option long_options[] = {
	{ "crash-dump",			no_argument,       NULL, 'a' },
	{ "pidfile",			required_argument, NULL, 'B' },
	{ "daemon",				no_argument,       NULL, 'b' },
	{ "config-dir",			required_argument, NULL, 'c' },
	{ "debug",				required_argument, NULL, 'd' },
	{ "gcollect",			required_argument, NULL, 'g' },
	{ "help",				no_argument,       NULL, 'h' },
	{ "syslog-ident",		required_argument, NULL, 'I' },
	{ "pending-ecm",		required_argument, NULL, 'p' },
	{ "restart",			required_argument, NULL, 'r' },
	{ "show-sensitive",		no_argument,       NULL, 'S' },
	{ "capture-segfaults",	no_argument,       NULL, 's' },
	{ "temp-dir",			required_argument, NULL, 't' },
	{ "utf8",				no_argument,       NULL, 'u' },
	{ "build-info",			no_argument,       NULL, 'V' },
	{ "wait",				required_argument, NULL, 'w' },
	{ 0, 0, 0, 0 }
};

static void write_versionfile(bool use_stdout);

static void parse_cmdline_params(int argc, char **argv) {
	int i;
	while ((i = getopt_long(argc, argv, short_options, long_options, NULL)) != EOF) {
		if (i == '?')
			fprintf(stderr, "ERROR: Unknown command line parameter: %s\n", argv[optind - 1]);
		switch(i) {
		case 'a': // --crash-dump
			cs_dump_stack = 1;
			break;
		case 'B': // --pidfile
			oscam_pidfile = optarg;
			break;
		case 'b': // --daemon
			bg = 1;
			break;
		case 'c': // --config-dir
			cs_strncpy(cs_confdir, optarg, sizeof(cs_confdir));
			break;
		case 'd': // --debug
			cs_dblevel = atoi(optarg);
			break;
		case 'g': // --gcollect
			gbdb = atoi(optarg);
			break;
		case 'h': // --help
			show_usage();
			exit(EXIT_SUCCESS);
			break;
		case 'I': // --syslog-ident
			syslog_ident = optarg;
			break;
		case 'p': // --pending-ecm
			max_pending = atoi(optarg) <= 0 ? 32 : MIN(atoi(optarg), 255);
			break;
		case 'r': // --restart
			if (config_WEBIF()) {
				cs_restart_mode = atoi(optarg);
			}
			break;
		case 'S': // --show-sensitive
			log_remove_sensitive = !log_remove_sensitive;
			break;
		case 's': // --capture-segfaults
			cs_capture_SEGV = 1;
			break;
		case 't': { // --temp-dir
			mkdir(optarg, S_IRWXU);
			int j = open(optarg, O_RDONLY);
			if (j >= 0) {
				close(j);
				cs_strncpy(cs_tmpdir, optarg, sizeof(cs_tmpdir));
			} else {
				printf("WARNING: Temp dir does not exist. Using default value.\n");
			}
			break;
		}
		case 'u': // --utf8
			if (config_WEBIF()) {
				cs_http_use_utf8 = 1;
				printf("WARNING: Web interface UTF-8 mode enabled. Carefully read documentation as bugs may arise.\n");
			}
			break;
		case 'V': // --build-info
			write_versionfile(true);
			exit(EXIT_SUCCESS);
			break;
		case 'w': // --wait
			cs_waittime = strtoul(optarg, NULL, 10);
			break;
		}
	}
}

#define write_conf(CONFIG_VAR, text) \
	fprintf(fp, "%-30s %s\n", text ":", config_##CONFIG_VAR() ? "yes" : "no")

#define write_readerconf(CONFIG_VAR, text) \
	fprintf(fp, "%-30s %s\n", text ":", config_##CONFIG_VAR() ? "yes" : "no - no EMM support!")

#define write_cardreaderconf(CONFIG_VAR, text) \
	fprintf(fp, "%s%-19s %s\n", "cardreader_", text ":", config_##CONFIG_VAR() ? "yes" : "no")

static void write_versionfile(bool use_stdout) {
	FILE *fp = stdout;
	if (!use_stdout) {
		char targetfile[256];
		snprintf(targetfile, sizeof(targetfile) - 1, "%s%s", get_tmp_dir(), "/oscam.version");
		targetfile[sizeof(targetfile) - 1] = 0;
		fp = fopen(targetfile, "w");
		if (!fp) {
			cs_log("Cannot open %s (errno=%d %s)", targetfile, errno, strerror(errno));
			return;
		}
		struct tm st;
		time_t now = time(NULL);
		localtime_r(&now, &st);

		fprintf(fp, "Unix starttime: %ld\n", (long)now);
		fprintf(fp, "Starttime:      %02d.%02d.%04d %02d:%02d:%02d\n",
			st.tm_mday, st.tm_mon + 1, st.tm_year + 1900,
			st.tm_hour, st.tm_min, st.tm_sec);
	}

	fprintf(fp, "Version:        oscam-%s-r%s\n", CS_VERSION, CS_SVN_VERSION);

	fprintf(fp, "\n");
	write_conf(WEBIF, "Web interface support");
	write_conf(TOUCH, "Touch interface support");
	write_conf(WITH_SSL, "SSL support");
	write_conf(HAVE_DVBAPI, "DVB API support");
	if (config_HAVE_DVBAPI()) {
		write_conf(WITH_AZBOX, "DVB API with AZBOX support");
		write_conf(WITH_MCA, "DVB API with MCA support");
		write_conf(WITH_COOLAPI, "DVB API with COOLAPI support");
		write_conf(WITH_STAPI, "DVB API with STAPI support");
	}
	write_conf(CS_ANTICASC, "Anti-cascading support");
	write_conf(IRDETO_GUESSING, "Irdeto guessing");
	write_conf(WITH_DEBUG, "Debug mode");
	write_conf(MODULE_MONITOR, "Monitor");
	write_conf(WITH_LB, "Loadbalancing support");
	write_conf(LCDSUPPORT, "LCD support");
	write_conf(LEDSUPPORT, "LED support");
	write_conf(IPV6SUPPORT, "IPv6 support");
	write_conf(CS_CACHEEX, "Cache exchange support");

	fprintf(fp, "\n");
	write_conf(MODULE_CAMD33, "camd 3.3x");
	write_conf(MODULE_CAMD35, "camd 3.5 UDP");
	write_conf(MODULE_CAMD35_TCP, "camd 3.5 TCP");
	write_conf(MODULE_NEWCAMD, "newcamd");
	write_conf(MODULE_CCCAM, "CCcam");
	write_conf(MODULE_CCCSHARE, "CCcam share");
	write_conf(MODULE_PANDORA, "Pandora");
	write_conf(MODULE_GHTTP, "ghttp");
	write_conf(MODULE_GBOX, "gbox");
	write_conf(MODULE_RADEGAST, "radegast");
	write_conf(MODULE_SERIAL, "serial");
	write_conf(MODULE_CONSTCW, "constant CW");

	fprintf(fp, "\n");
	write_conf(WITH_CARDREADER, "Reader support");
	if (config_WITH_CARDREADER()) {
		fprintf(fp, "\n");
		write_readerconf(READER_NAGRA, "Nagra");
		write_readerconf(READER_IRDETO, "Irdeto");
		write_readerconf(READER_CONAX, "Conax");
		write_readerconf(READER_CRYPTOWORKS, "Cryptoworks");
		write_readerconf(READER_SECA, "Seca");
		write_readerconf(READER_VIACCESS, "Viaccess");
		write_readerconf(READER_VIDEOGUARD, "NDS Videoguard");
		write_readerconf(READER_DRE, "DRE Crypt");
		write_readerconf(READER_TONGFANG, "TONGFANG");
		write_readerconf(READER_BULCRYPT, "Bulcrypt");
		write_readerconf(READER_GRIFFIN, "Griffin");
		fprintf(fp, "\n");
		write_cardreaderconf(CARDREADER_PHOENIX, "phoenix");
		write_cardreaderconf(CARDREADER_INTERNAL_AZBOX, "internal_azbox");
		write_cardreaderconf(CARDREADER_INTERNAL_COOLAPI, "internal_coolapi");
		write_cardreaderconf(CARDREADER_INTERNAL_SCI, "internal_sci");
		write_cardreaderconf(CARDREADER_SC8IN1, "sc8in1");
		write_cardreaderconf(CARDREADER_MP35, "mp35");
		write_cardreaderconf(CARDREADER_SMARGO, "smargo");
		write_cardreaderconf(CARDREADER_PCSC, "pcsc");
		write_cardreaderconf(CARDREADER_SMART, "smartreader");
		write_cardreaderconf(CARDREADER_DB2COM, "db2com");
		write_cardreaderconf(CARDREADER_STAPI, "stapi");
	} else {
		write_readerconf(WITH_CARDREADER, "Reader Support");
	}
	if (!use_stdout)
		fclose(fp);
}
#undef write_conf
#undef write_readerconf
#undef write_cardreaderconf

#define report_emm_support(CONFIG_VAR, text) \
	do { \
		if (!config_##CONFIG_VAR()) \
			cs_log("Binary without %s module - no EMM processing for %s possible!", text, text); \
	} while(0)

static void do_report_emm_support(void) {
	if (!config_WITH_CARDREADER()) {
		cs_log("Binary without Cardreader Support! No EMM processing possible!");
	} else {
		report_emm_support(READER_NAGRA, "Nagra");
		report_emm_support(READER_IRDETO, "Irdeto");
		report_emm_support(READER_CONAX, "Conax");
		report_emm_support(READER_CRYPTOWORKS, "Cryptoworks");
		report_emm_support(READER_SECA, "Seca");
		report_emm_support(READER_VIACCESS, "Viaccess");
		report_emm_support(READER_VIDEOGUARD, "NDS Videoguard");
		report_emm_support(READER_DRE, "DRE Crypt");
		report_emm_support(READER_TONGFANG, "TONGFANG");
		report_emm_support(READER_BULCRYPT, "Bulcrypt");
		report_emm_support(READER_GRIFFIN, "Griffin");
	}
}
#undef report_emm_support

#ifdef NEED_DAEMON
// The compat function is not called daemon() because this may cause problems.
static int32_t do_daemon(int32_t nochdir, int32_t noclose)
{
  int32_t fd;

  switch (fork())
  {
    case -1: return (-1);
    case 0:  break;
    default: _exit(0);
  }

  if (setsid()==(-1))
    return(-1);

  if (!nochdir)
    (void)chdir("/");

  if (!noclose && (fd=open("/dev/null", O_RDWR, 0)) != -1)
  {
    (void)dup2(fd, STDIN_FILENO);
    (void)dup2(fd, STDOUT_FILENO);
    (void)dup2(fd, STDERR_FILENO);
    if (fd>2)
      (void)close(fd);
  }
  return(0);
}
#else
#define do_daemon daemon
#endif

static bool config_freed;

static void cs_cleanup(void)
{
	if (config_freed)
		return;

	stat_finish();

	cccam_done_share();

	kill_all_clients();

	//cleanup readers:
	struct s_client *cl;
	struct s_reader *rdr;
	for (rdr=first_active_reader; rdr ; rdr=rdr->next) {
		cl = rdr->client;
		if(cl){
			rdr_log(rdr, "Killing reader");
			kill_thread(cl);
			// Stop MCR reader display thread
			if (cl->typ == 'r' && cl->reader && cl->reader->typ == R_SC8in1
					&& cl->reader->sc8in1_config && cl->reader->sc8in1_config->display_running) {
				cl->reader->sc8in1_config->display_running = 0;
			}
		}
	}
	first_active_reader = NULL;

	init_free_userdb(cfg.account);
	cfg.account = NULL;
	init_free_sidtab();

	if (oscam_pidfile)
		unlink(oscam_pidfile);

	config_free();
	config_freed = true;

	cs_log("cardserver down");

	cs_close_log();
}

/*
 * flags: 1 = restart, 2 = don't modify if SIG_IGN, may be combined
 */
static void set_signal_handler(int32_t sig, int32_t flags, void (*sighandler))
{
  struct sigaction sa;
  sigaction(sig, (struct sigaction *) 0, &sa);
  if (!((flags & 2) && (sa.sa_handler==SIG_IGN)))
  {
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=(flags & 1) ? SA_RESTART : 0;
    sa.sa_handler=sighandler;
    sigaction(sig, &sa, (struct sigaction *) 0);
  }
}

static void cs_master_alarm(void)
{
  cs_log("PANIC: master deadlock!");
  fprintf(stderr, "PANIC: master deadlock!");
  fflush(stderr);
}

static void cs_sigpipe(void)
{
	if (cs_dblevel & D_ALL_DUMP)
		cs_log("Got sigpipe signal -> captured");
}

static void cs_dummy(void) {
	return;
}

/* Switch debuglevel forward one step (called when receiving SIGUSR1). */
static void cs_debug_level(void) {
	switch (cs_dblevel) {
		case 0:
			cs_dblevel = 1;
			break;
		case 128:
			cs_dblevel = 255;
			break;
		case 255:
			cs_dblevel = 0;
			break;
		default:
			cs_dblevel <<= 1;
	}

	cs_log("debug_level=%d", cs_dblevel);
}

/**
 * write stacktrace to oscam.crash. file is always appended
 * Usage:
 * 1. compile oscam with debug parameters (Makefile: DS_OPTS="-ggdb")
 * 2. you need gdb installed and working on the local machine
 * 3. start oscam with parameter: -a
 */
static void cs_dumpstack(int32_t sig)
{
	FILE *fp = fopen("oscam.crash", "a+");

	time_t timep;
	char buf[200];

	time(&timep);
	cs_ctime_r(&timep, buf);

	fprintf(stderr, "crashed with signal %d on %swriting oscam.crash\n", sig, buf);

	fprintf(fp, "%sOSCam cardserver v%s, build r%s (%s)\n", buf, CS_VERSION, CS_SVN_VERSION, CS_TARGET);
	fprintf(fp, "FATAL: Signal %d: %s Fault. Logged StackTrace:\n\n", sig, (sig == SIGSEGV) ? "Segmentation" : ((sig == SIGBUS) ? "Bus" : "Unknown"));
	fclose(fp);

	FILE *cmd = fopen("/tmp/gdbcmd", "w");
	fputs("bt\n", cmd);
	fputs("thread apply all bt\n", cmd);
	fclose(cmd);

	snprintf(buf, sizeof(buf)-1, "gdb %s %d -batch -x /tmp/gdbcmd >> oscam.crash", prog_name, getpid());
	if(system(buf) == -1)
		fprintf(stderr, "Fatal error on trying to start gdb process.");

	exit(-1);
}


/**
 * called by signal SIGHUP
 *
 * reloads configs:
 *  - useraccounts (oscam.user)
 *  - services ids (oscam.srvid)
 *  - tier ids     (oscam.tiers)
 *  Also clears anticascading stats.
 **/
static void cs_reload_config(void)
{
		cs_accounts_chk();
		init_srvid();
		init_tierid();
		ac_init_stat();
}

/* Sets signal handlers to ignore for early startup of OSCam because for example log
   could cause SIGPIPE errors and the normal signal handlers can't be used at this point. */
static void init_signal_pre(void)
{
		set_signal_handler(SIGPIPE , 1, SIG_IGN);
		set_signal_handler(SIGWINCH, 1, SIG_IGN);
		set_signal_handler(SIGALRM , 1, SIG_IGN);
		set_signal_handler(SIGHUP  , 1, SIG_IGN);
}

/* Sets the signal handlers.*/
static void init_signal(int8_t isDaemon)
{
		set_signal_handler(SIGINT, 3, cs_exit);
#if defined(__APPLE__)
		set_signal_handler(SIGEMT, 3, cs_exit);
#endif
		set_signal_handler(SIGTERM, 3, cs_exit);

		set_signal_handler(SIGWINCH, 1, SIG_IGN);
		set_signal_handler(SIGPIPE , 0, cs_sigpipe);
		set_signal_handler(SIGALRM , 0, cs_master_alarm);
		set_signal_handler(SIGHUP  , 1, isDaemon?cs_dummy:cs_reload_config);
		set_signal_handler(SIGUSR1, 1, isDaemon?cs_dummy:cs_debug_level);
		set_signal_handler(SIGUSR2, 1, isDaemon?cs_dummy:cs_card_info);
		set_signal_handler(OSCAM_SIGNAL_WAKEUP, 0, isDaemon?cs_dummy:cs_dummy);

		if(!isDaemon){
			if (cs_capture_SEGV) {
				set_signal_handler(SIGSEGV, 1, cs_exit);
				set_signal_handler(SIGBUS, 1, cs_exit);
			}
			else if (cs_dump_stack) {
				set_signal_handler(SIGSEGV, 1, cs_dumpstack);
				set_signal_handler(SIGBUS, 1, cs_dumpstack);
			}

			cs_log("signal handling initialized");
		}
	return;
}

void cs_exit(int32_t sig)
{
	if (cs_dump_stack && (sig == SIGSEGV || sig == SIGBUS))
		cs_dumpstack(sig);

	set_signal_handler(SIGCHLD, 1, SIG_IGN);
	set_signal_handler(SIGHUP , 1, SIG_IGN);
	set_signal_handler(SIGPIPE, 1, SIG_IGN);

	if (sig==SIGALRM) {
		cs_debug_mask(D_TRACE, "thread %8lX: SIGALRM, skipping", (unsigned long)pthread_self());
		return;
	}

  if (sig && (sig!=SIGQUIT))
    cs_log("thread %8lX exit with signal %d", (unsigned long)pthread_self(), sig);

  struct s_client *cl = cur_client();
  if (!cl)
  	return;

	if (cl->typ == 'h' || cl->typ == 's') {
		led_status_stopping();
		led_stop();
		lcd_thread_stop();

#if !defined(__CYGWIN__)
	char targetfile[256];
		snprintf(targetfile, 255, "%s%s", get_tmp_dir(), "/oscam.version");
		if (unlink(targetfile) < 0)
			cs_log("cannot remove oscam version file %s (errno=%d %s)", targetfile, errno, strerror(errno));
#endif
		coolapi_close_all();
  }

	// this is very important - do not remove
	if (cl->typ != 's') {
		cs_debug_mask(D_TRACE, "thread %8lX ended!", (unsigned long)pthread_self());

		free_client(cl);

		//Restore signals before exiting thread
		set_signal_handler(SIGPIPE , 0, cs_sigpipe);
		set_signal_handler(SIGHUP  , 1, cs_reload_config);

		pthread_exit(NULL);
		return;
	}

	cs_cleanup();

	if (!exit_oscam)
	  exit_oscam = sig?sig:1;

	if (sig == SIGINT)
		exit(sig);
}

/* Checks if the date of the system is correct and waits if necessary. */
static void init_check(void){
	char *ptr = __DATE__;
	int32_t month, year = atoi(ptr + strlen(ptr) - 4), day = atoi(ptr + 4);
	if(day > 0 && day < 32 && year > 2010 && year < 9999){
		struct tm timeinfo;
		char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		for(month = 0; month < 12; ++month){
			if(!strncmp(ptr, months[month], 3)) break;
		}
		if(month > 11) month = 0;
		memset(&timeinfo, 0, sizeof(timeinfo));
		timeinfo.tm_mday = day;
		timeinfo.tm_mon = month;
		timeinfo.tm_year = year - 1900;
		time_t builddate = mktime(&timeinfo) - 86400;
	  int32_t i = 0;
	  while(time((time_t*)0) < builddate){
	  	if(i == 0) cs_log("The current system time is smaller than the build date (%s). Waiting up to %d seconds for time to correct", ptr, cs_waittime);
	  	cs_sleepms(1000);
	  	++i;
	  	if(i > cs_waittime){
	  		cs_log("Waiting was not successful. OSCam will be started but is UNSUPPORTED this way. Do not report any errors with this version.");
				break;
	  	}
	  }
	  // adjust login time of first client
	  if(i > 0) first_client->login=time((time_t *)0);
	}
}

/* Starts a thread named nameroutine with the start function startroutine. */
void start_thread(void * startroutine, char * nameroutine) {
	pthread_t temp;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	cs_debug_mask(D_TRACE, "starting thread %s", nameroutine);
	pthread_attr_setstacksize(&attr, PTHREAD_STACK_SIZE);
	cs_writelock(&system_lock);
	int32_t ret = pthread_create(&temp, &attr, startroutine, NULL);
	if (ret)
		cs_log("ERROR: can't create %s thread (errno=%d %s)", nameroutine, ret, strerror(ret));
	else {
		cs_debug_mask(D_TRACE, "%s thread started", nameroutine);
		pthread_detach(temp);
	}
	pthread_attr_destroy(&attr);
	cs_writeunlock(&system_lock);
}

/* Allows to kill another thread specified through the client cl with locking.
  If the own thread has to be cancelled, cs_exit or cs_disconnect_client has to be used. */
void kill_thread(struct s_client *cl) {
	if (!cl || cl->kill) return;
	if (cl == cur_client()) {
		cs_log("Trying to kill myself, exiting.");
		cs_exit(0);
	}
	add_job(cl, ACTION_CLIENT_KILL, NULL, 0); //add kill job, ...
	cl->kill=1;                               //then set kill flag!
}

static void cs_waitforcardinit(void)
{
	if (cfg.waitforcards)
	{
		cs_log("waiting for local card init");
		int32_t card_init_done;
		do {
			card_init_done = 1;
			struct s_reader *rdr;
			LL_ITER itr = ll_iter_create(configured_readers);
			while((rdr = ll_iter_next(&itr))) {
				if (rdr->enable && !is_cascading_reader(rdr) && (rdr->card_status == CARD_NEED_INIT || rdr->card_status == UNKNOWN)) {
					card_init_done = 0;
					break;
				}
			}

			if (!card_init_done)
				cs_sleepms(300); // wait a little bit
			//alarm(cfg.cmaxidle + cfg.ctimeout / 1000 + 1);
		} while (!card_init_done);
		if (cfg.waitforcards_extra_delay>0)
			cs_sleepms(cfg.waitforcards_extra_delay);
		cs_log("init for all local cards done");
	}
}

static uint32_t auto_timeout(ECM_REQUEST *er, uint32_t timeout) {
        (void)er; // Prevent warning about unused er, when WITH_LB is disabled
#ifdef WITH_LB
        if (cfg.lb_auto_timeout)
                return lb_auto_timeout(er, timeout);
#endif
        return timeout;
}


static void * check_thread(void) {
	int32_t time_to_check, next_check, ecmc_next, msec_wait = 3000;
	struct timeb t_now, tbc, ecmc_time;
	ECM_REQUEST *er = NULL;
	time_t ecm_timeout;
	time_t ecm_mintimeout;
	struct timespec ts;
	struct s_client *cl = create_client(first_client->ip);
	cl->typ = 's';
#ifdef WEBIF
	cl->wihidden = 1;
#endif
	cl->thread = pthread_self();

	timecheck_client = cl;

#ifdef CS_ANTICASC
	int32_t ac_next;
	struct timeb ac_time;
	cs_ftime(&ac_time);
	add_ms_to_timeb(&ac_time, cfg.ac_stime*60*1000);
#endif

	cs_ftime(&ecmc_time);
	add_ms_to_timeb(&ecmc_time, 1000);

	while(1) {
		ts.tv_sec = msec_wait/1000;
		ts.tv_nsec = (msec_wait % 1000) * 1000000L;
		pthread_mutex_lock(&cl->thread_lock);
		cl->thread_active = 2;
		pthread_mutex_unlock(&cl->thread_lock);
		nanosleep(&ts, NULL);
		pthread_mutex_lock(&cl->thread_lock);
		cl->thread_active = 1;
		pthread_mutex_unlock(&cl->thread_lock);

		next_check = 0;
#ifdef CS_ANTICASC
		ac_next = 0;
#endif
		ecmc_next = 0;
		msec_wait = 0;

		cs_ftime(&t_now);
		cs_readlock(&ecmcache_lock);

		for (er = ecmcwcache; er; er = er->next) {
			if (er->rc < E_99 || !er->ecmlen || !er->matching_rdr) //ignore CACHEEX pending ECMs
				continue;

			tbc = er->tps;
#ifdef CS_CACHEEX
			time_to_check = add_ms_to_timeb(&tbc, (er->stage < 2) ? cfg.cacheex_wait_time:((er->stage < 4) ? auto_timeout(er, cfg.ftimeout) : auto_timeout(er, cfg.ctimeout)));
#else
			time_to_check = add_ms_to_timeb(&tbc, ((er->stage < 4) ? auto_timeout(er, cfg.ftimeout) : auto_timeout(er, cfg.ctimeout)));
#endif

			if (comp_timeb(&t_now, &tbc) >= 0) {
				if (er->stage < 4) {
					debug_ecm(D_TRACE, "fallback for %s %s", username(er->client), buf);

					if (er->rc >= E_UNHANDLED) //do not request rc=99
						request_cw_from_readers(er);

					tbc = er->tps;
					time_to_check = add_ms_to_timeb(&tbc, auto_timeout(er, cfg.ctimeout));
				} else {
					if (er->client) {
					        er->selected_reader = NULL;
						debug_ecm(D_TRACE, "timeout for %s %s", username(er->client), buf);
						write_ecm_answer(NULL, er, E_TIMEOUT, 0, NULL, NULL);
					}
#ifdef WITH_LB
					if (!er->ecmcacheptr) { //do not add stat for cache entries:
						//because of lb, send E_TIMEOUT for all readers:
						struct s_ecm_answer *ea_list;

						for(ea_list = er->matching_rdr; ea_list; ea_list = ea_list->next) {
							if ((ea_list->status & (REQUEST_SENT|REQUEST_ANSWERED)) == REQUEST_SENT) //Request send, but no answer!
								send_reader_stat(ea_list->reader, er, NULL, E_TIMEOUT);
						}
					}
#endif

					time_to_check = 0;
				}
			}
			if (!next_check || (time_to_check > 0 && time_to_check < next_check))
				next_check = time_to_check;
		}
		cs_readunlock(&ecmcache_lock);

#ifdef CS_ANTICASC
		if ((ac_next = comp_timeb(&ac_time, &t_now)) <= 10) {
			if (cfg.ac_enabled)
				ac_do_stat();
			cs_ftime(&ac_time);
			ac_next = add_ms_to_timeb(&ac_time, cfg.ac_stime*60*1000);
		}
#endif

		if ((ecmc_next = comp_timeb(&ecmc_time, &t_now)) <= 10) {
			ecm_timeout = t_now.time-cfg.max_cache_time;
			ecm_mintimeout = t_now.time-(cfg.ctimeout/1000+2);
			uint32_t count = 0;

			struct ecm_request_t *ecm, *ecmt=NULL, *prv;
			cs_readlock(&ecmcache_lock);
			for (ecm = ecmcwcache, prv = NULL; ecm; prv = ecm, ecm = ecm->next, count++) {
				if (ecm->tps.time < ecm_timeout || (ecm->tps.time<ecm_mintimeout && count>cfg.max_cache_count)) {
					cs_readunlock(&ecmcache_lock);
					cs_writelock(&ecmcache_lock);
					ecmt = ecm;
					if (prv)
						prv->next = NULL;
					else
						ecmcwcache = NULL;
					cs_writeunlock(&ecmcache_lock);
					break;
				}
			}
			if (!ecmt)
				cs_readunlock(&ecmcache_lock);
			ecmcwcache_size = count;

			while (ecmt) {
				ecm = ecmt->next;
				free_ecm(ecmt);
				ecmt = ecm;
			}

			cs_ftime(&ecmc_time);
			ecmc_next = add_ms_to_timeb(&ecmc_time, 1000);
		}

		msec_wait = next_check;

#ifdef CS_ANTICASC
		if (!msec_wait || (ac_next > 0 && ac_next < msec_wait))
			msec_wait = ac_next;
#endif

		if (!msec_wait || (ecmc_next > 0 && ecmc_next < msec_wait))
			msec_wait = ecmc_next;

		if (!msec_wait)
			msec_wait = 3000;

		cleanup_hitcache();
	}
	add_garbage(cl);
	timecheck_client = NULL;
	return NULL;
}

static uint32_t resize_pfd_cllist(struct pollfd **pfd, struct s_client ***cl_list, uint32_t old_size, uint32_t new_size) {
	if (old_size != new_size) {
		struct pollfd *pfd_new;
		if (!cs_malloc(&pfd_new, new_size * sizeof(struct pollfd))) {
			return old_size;
		}
		struct s_client **cl_list_new;
		if (!cs_malloc(&cl_list_new, new_size * sizeof(cl_list))) {
			free(pfd_new);
			return old_size;
		}
		if (old_size > 0) {
			memcpy(pfd_new, *pfd, old_size*sizeof(struct pollfd));
			memcpy(cl_list_new, *cl_list, old_size*sizeof(cl_list));
			free(*pfd);
			free(*cl_list);
		}
		*pfd = pfd_new;
		*cl_list = cl_list_new;
	}
	return new_size;
}

static uint32_t chk_resize_cllist(struct pollfd **pfd, struct s_client ***cl_list, uint32_t cur_size, uint32_t chk_size) {
	chk_size++;
	if (chk_size > cur_size) {
		uint32_t new_size = ((chk_size % 100)+1) * 100; //increase 100 step
		cur_size = resize_pfd_cllist(pfd, cl_list, cur_size, new_size);
	}
	return cur_size;
}

static void * client_check(void) {
	int32_t i, k, j, rc, pfdcount = 0;
	struct s_client *cl;
	struct s_reader *rdr;
	struct pollfd *pfd;
	struct s_client **cl_list;
	uint32_t cl_size = 0;

	char buf[10];

	if (pipe(thread_pipe) == -1) {
		printf("cannot create pipe, errno=%d\n", errno);
		exit(1);
	}

	cl_size = chk_resize_cllist(&pfd, &cl_list, 0, 100);

	pfd[pfdcount].fd = thread_pipe[0];
	pfd[pfdcount].events = POLLIN | POLLPRI | POLLHUP;
	cl_list[pfdcount] = NULL;

	while (!exit_oscam) {
		pfdcount = 1;

		//connected tcp clients
		for (cl=first_client->next; cl; cl=cl->next) {
			if (cl->init_done && !cl->kill && cl->pfd && cl->typ=='c' && !cl->is_udp) {
				if (cl->pfd && !cl->thread_active) {
					cl_size = chk_resize_cllist(&pfd, &cl_list, cl_size, pfdcount);
					cl_list[pfdcount] = cl;
					pfd[pfdcount].fd = cl->pfd;
					pfd[pfdcount++].events = POLLIN | POLLPRI | POLLHUP;
				}
			}
			//reader:
			//TCP:
			//	- TCP socket must be connected
			//	- no active init thread
			//UDP:
			//	- connection status ignored
			//	- no active init thread
			rdr = cl->reader;
			if (rdr && cl->typ=='p' && cl->init_done) {
				if (cl->pfd && !cl->thread_active && ((rdr->tcp_connected && rdr->ph.type==MOD_CONN_TCP)||(rdr->ph.type==MOD_CONN_UDP))) {
					cl_size = chk_resize_cllist(&pfd, &cl_list, cl_size, pfdcount);
					cl_list[pfdcount] = cl;
					pfd[pfdcount].fd = cl->pfd;
					pfd[pfdcount++].events = POLLIN | POLLPRI | POLLHUP;
				}
			}
		}

		//server (new tcp connections or udp messages)
		for (k=0; k < CS_MAX_MOD; k++) {
			if ( (modules[k].type & MOD_CONN_NET) && modules[k].ptab ) {
				for (j=0; j<modules[k].ptab->nports; j++) {
					if (modules[k].ptab->ports[j].fd) {
						cl_size = chk_resize_cllist(&pfd, &cl_list, cl_size, pfdcount);
						cl_list[pfdcount] = NULL;
						pfd[pfdcount].fd = modules[k].ptab->ports[j].fd;
						pfd[pfdcount++].events = POLLIN | POLLPRI | POLLHUP;

					}
				}
			}
		}

		if (pfdcount >= 1024)
			cs_log("WARNING: too many users!");

		rc = poll(pfd, pfdcount, 5000);

		if (rc<1)
			continue;

		for (i=0; i<pfdcount; i++) {
			//clients
			cl = cl_list[i];
			if (cl && !is_valid_client(cl))
				continue;

			if (pfd[i].fd == thread_pipe[0] && (pfd[i].revents & (POLLIN | POLLPRI))) {
				// a thread ended and cl->pfd should be added to pollfd list again (thread_active==0)
				if(read(thread_pipe[0], buf, sizeof(buf)) == -1){
					cs_debug_mask(D_TRACE, "Reading from pipe failed (errno=%d %s)", errno, strerror(errno));
				}
				continue;
			}

			//clients
			// message on an open tcp connection
			if (cl && cl->init_done && cl->pfd && (cl->typ == 'c' || cl->typ == 'm')) {
				if (pfd[i].fd == cl->pfd && (pfd[i].revents & (POLLHUP | POLLNVAL))) {
					//client disconnects
					kill_thread(cl);
					continue;
				}
				if (pfd[i].fd == cl->pfd && (pfd[i].revents & (POLLIN | POLLPRI))) {
					add_job(cl, ACTION_CLIENT_TCP, NULL, 0);
				}
			}


			//reader
			// either an ecm answer, a keepalive or connection closed from a proxy
			// physical reader ('r') should never send data without request
			rdr = NULL;
			struct s_client *cl2 = NULL;
			if (cl && cl->typ == 'p'){
				rdr = cl->reader;
				if(rdr)
					cl2 = rdr->client;
			}

			if (rdr && cl2 && cl2->init_done) {
				if (cl2->pfd && pfd[i].fd == cl2->pfd && (pfd[i].revents & (POLLHUP | POLLNVAL))) {
					//connection to remote proxy was closed
					//oscam should check for rdr->tcp_connected and reconnect on next ecm request sent to the proxy
					network_tcp_connection_close(rdr, "closed");
					rdr_debug_mask(rdr, D_READER, "connection closed");
				}
				if (cl2->pfd && pfd[i].fd == cl2->pfd && (pfd[i].revents & (POLLIN | POLLPRI))) {
					add_job(cl2, ACTION_READER_REMOTE, NULL, 0);
				}
			}


			//server sockets
			// new connection on a tcp listen socket or new message on udp listen socket
			if (!cl && (pfd[i].revents & (POLLIN | POLLPRI))) {
				for (k=0; k<CS_MAX_MOD; k++) {
					if( (modules[k].type & MOD_CONN_NET) && modules[k].ptab ) {
						for ( j=0; j<modules[k].ptab->nports; j++ ) {
							if ( modules[k].ptab->ports[j].fd && pfd[i].fd == modules[k].ptab->ports[j].fd ) {
								accept_connection(k,j);
							}
						}
					}
				} // if (modules[i].type & MOD_CONN_NET)
			}
		}
		first_client->last=time((time_t *)0);
	}
	free(pfd);
	free(cl_list);
	return NULL;
}

static void * reader_check(void) {
	struct s_client *cl;
	struct s_reader *rdr;
	while (1) {
		for (cl=first_client->next; cl ; cl=cl->next) {
			if (!cl->thread_active)
				client_check_status(cl);
		}
		cs_readlock(&readerlist_lock);
		for (rdr=first_active_reader; rdr; rdr=rdr->next) {
			if (rdr->enable) {
				cl = rdr->client;
				if (!cl || cl->kill)
					restart_cardreader(rdr, 0);
				else if (!cl->thread_active)
					client_check_status(cl);
			}
		}
		cs_readunlock(&readerlist_lock);
		cs_sleepms(1000);
	}
	return NULL;
}

#ifdef WEBIF
pid_t pid;


static void fwd_sig(int32_t sig)
{
    kill(pid, sig);
}

static void restart_daemon(void)
{
  while (1) {

    //start client process:
    pid = fork();
    if (!pid)
      return; //client process=oscam process
    if (pid < 0)
      exit(1);

    //set signal handler for the restart daemon:
    set_signal_handler(SIGTERM, 0, fwd_sig);
    set_signal_handler(SIGQUIT, 0, fwd_sig);
    set_signal_handler(SIGHUP , 0, fwd_sig);
                                                                                                                                                
    //restart control process:
    int32_t res=0;
    int32_t status=0;
    do {
      res = waitpid(pid, &status, 0);
      if (res==-1) {
        if (errno!=EINTR)
          exit(1);
      }
    } while (res!=pid);

    if (cs_restart_mode==2 && WIFSIGNALED(status) && WTERMSIG(status)==SIGSEGV)
      status=99; //restart on segfault!
    else
      status = WEXITSTATUS(status);

    //status=99 restart oscam, all other->terminate
    if (status!=99) {
      exit(status);
    }
  }
}

void cs_restart_oscam(void) {
	exit_oscam=99;
	cs_log("restart oscam requested");
}

int32_t cs_get_restartmode(void) {
	return cs_restart_mode;
}
#endif

void cs_exit_oscam(void) {
	exit_oscam = 1;
	cs_log("exit oscam requested");
}

static void pidfile_create(char *pidfile) {
	FILE *f = fopen(pidfile, "w");
	if (f) {
		pid_t my_pid = getpid();
		cs_log("creating pidfile %s with pid %d", pidfile, my_pid);
		fprintf(f, "%d\n", my_pid);
		fclose(f);
	}
}

int32_t main (int32_t argc, char *argv[])
{
	int32_t i, j;
	prog_name = argv[0];
	if (pthread_key_create(&getclient, NULL)) {
		fprintf(stderr, "Could not create getclient, exiting...");
		exit(1);
	}

  void (*mod_def[])(struct s_module *)=
  {
#ifdef MODULE_MONITOR
           module_monitor,
#endif
#ifdef MODULE_CAMD33
           module_camd33,
#endif
#ifdef MODULE_CAMD35
           module_camd35,
#endif
#ifdef MODULE_CAMD35_TCP
           module_camd35_tcp,
#endif
#ifdef MODULE_NEWCAMD
           module_newcamd,
#endif
#ifdef MODULE_CCCAM
           module_cccam,
#endif
#ifdef MODULE_PANDORA
           module_pandora,
#endif
#ifdef MODULE_GHTTP
           module_ghttp,
#endif
#ifdef CS_CACHEEX
           module_csp,
#endif
#ifdef MODULE_GBOX
           module_gbox,
#endif
#ifdef MODULE_CONSTCW
           module_constcw,
#endif
#ifdef MODULE_RADEGAST
           module_radegast,
#endif
#ifdef MODULE_SERIAL
           module_serial,
#endif
#ifdef HAVE_DVBAPI
	   module_dvbapi,
#endif
           0
  };

  void (*cardsystem_def[])(struct s_cardsystem *)=
  {
#ifdef READER_NAGRA
	reader_nagra,
#endif
#ifdef READER_IRDETO
	reader_irdeto,
#endif
#ifdef READER_CONAX
	reader_conax,
#endif
#ifdef READER_CRYPTOWORKS
	reader_cryptoworks,
#endif
#ifdef READER_SECA
	reader_seca,
#endif
#ifdef READER_VIACCESS
	reader_viaccess,
#endif
#ifdef READER_VIDEOGUARD
	reader_videoguard1,
	reader_videoguard2,
	reader_videoguard12,
#endif
#ifdef READER_DRE
	reader_dre,
#endif
#ifdef READER_TONGFANG
	reader_tongfang,
#endif
#ifdef READER_BULCRYPT
	reader_bulcrypt,
#endif
#ifdef READER_GRIFFIN
	reader_griffin,
#endif
	0
  };

  void (*cardreader_def[])(struct s_cardreader *)=
  {
#ifdef CARDREADER_DB2COM
	cardreader_db2com,
#endif
#if defined(CARDREADER_INTERNAL_AZBOX)
	cardreader_internal_azbox,
#elif defined(CARDREADER_INTERNAL_COOLAPI)
	cardreader_internal_cool,
#elif defined(CARDREADER_INTERNAL_SCI)
	cardreader_internal_sci,
#endif
#ifdef CARDREADER_PHOENIX
	cardreader_mouse,
#endif
#ifdef CARDREADER_MP35
	cardreader_mp35,
#endif
#ifdef CARDREADER_PCSC
	cardreader_pcsc,
#endif
#ifdef CARDREADER_SC8IN1
	cardreader_sc8in1,
#endif
#ifdef CARDREADER_SMARGO
	cardreader_smargo,
#endif
#ifdef CARDREADER_SMART
	cardreader_smartreader,
#endif
#ifdef CARDREADER_STAPI
	cardreader_stapi,
#endif
	0
  };

  parse_cmdline_params(argc, argv);
  init_signal(true);

  if (bg && do_daemon(1,0))
  {
    printf("Error starting in background (errno=%d: %s)", errno, strerror(errno));
    cs_exit(1);
  }

  get_random_bytes_init();

#ifdef WEBIF
  if (cs_restart_mode)
    restart_daemon();
#endif

  memset(&cfg, 0, sizeof(struct s_config));
  cfg.max_pending = max_pending;

  if (cs_confdir[strlen(cs_confdir)]!='/') strcat(cs_confdir, "/");
  init_signal_pre(); // because log could cause SIGPIPE errors, init a signal handler first
  init_first_client();
  cs_lock_create(&system_lock, 5, "system_lock");
  cs_lock_create(&config_lock, 10, "config_lock");
  cs_lock_create(&gethostbyname_lock, 10, "gethostbyname_lock");
  cs_lock_create(&clientlist_lock, 5, "clientlist_lock");
  cs_lock_create(&readerlist_lock, 5, "readerlist_lock");
  cs_lock_create(&fakeuser_lock, 5, "fakeuser_lock");
  cs_lock_create(&ecmcache_lock, 5, "ecmcache_lock");
  cs_lock_create(&readdir_lock, 5, "readdir_lock");
  cs_lock_create(&hitcache_lock, 5, "hitcache_lock");
  coolapi_open_all();
  init_config();
  cs_init_log();
  if (!oscam_pidfile && cfg.pidfile)
    oscam_pidfile = cfg.pidfile;
  if (!oscam_pidfile) {
    snprintf(default_pidfile, sizeof(default_pidfile) - 1, "%s%s", get_tmp_dir(), "/oscam.pid");
    oscam_pidfile = default_pidfile;
  }
  if (oscam_pidfile)
    pidfile_create(oscam_pidfile);
  cs_init_statistics();
  init_check();
  init_stat();

  // These initializations *MUST* be called after init_config()
  // because modules depend on config values.
  for (i=0; mod_def[i]; i++)
  {
	memset(&modules[i], 0, sizeof(struct s_module));
	mod_def[i](&modules[i]);
  }
  for (i=0; cardsystem_def[i]; i++)
  {
	memset(&cardsystems[i], 0, sizeof(struct s_cardsystem));
	cardsystem_def[i](&cardsystems[i]);
  }
  for (i=0; cardreader_def[i]; i++)
  {
	memset(&cardreaders[i], 0, sizeof(struct s_cardreader));
	cardreader_def[i](&cardreaders[i]);
  }

  init_sidtab();
  init_readerdb();
  cfg.account = init_userdb();
  init_signal(false);
  init_srvid();
  init_tierid();
  init_provid();

  start_garbage_collector(gbdb);

  cacheex_init();

  init_len4caid();
  init_irdeto_guess_tab();

  write_versionfile(false);

  led_init();
  led_status_default();

  azbox_init();

  mca_init();

  global_whitelist_read();
  cacheex_load_config_file();

  for (i=0; i<CS_MAX_MOD; i++)
    if( (modules[i].type & MOD_CONN_NET) && modules[i].ptab )
      for(j=0; j<modules[i].ptab->nports; j++)
      {
        start_listener(&modules[i], j);
      }

	//set time for server to now to avoid 0 in monitor/webif
	first_client->last=time((time_t *)0);

	webif_init();

	start_thread((void *) &reader_check, "reader check");
	start_thread((void *) &check_thread, "check");

	lcd_thread_start();

	do_report_emm_support();

	init_cardreader();

	cs_waitforcardinit();

	led_status_starting();

	ac_init();

	for (i=0; i<CS_MAX_MOD; i++)
		if (modules[i].type & MOD_CONN_SERIAL)   // for now: oscam_ser only
			if (modules[i].s_handler)
				modules[i].s_handler(NULL, NULL, i);

	// main loop function
	client_check();

	azbox_close();

	mca_close();

	cs_cleanup();

	stop_garbage_collector();

	return exit_oscam;
}
