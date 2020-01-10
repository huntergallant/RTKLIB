/*------------------------------------------------------------------------------
 * str2str.c : console version of stream server
 *
 *          Copyright (C) 2007-2016 by T.TAKASU, All rights reserved.
 *
 * version : $Revision: 1.1 $ $Date: 2008/07/17 21:54:53 $
 * history : 2009/06/17  1.0  new
 *           2011/05/29  1.1  add -f, -l and -x option
 *           2011/11/29  1.2  fix bug on recognize ntrips:// (rtklib_2.4.1_p4)
 *           2012/12/25  1.3  add format conversion functions
 *                            add -msg, -opt and -sta options
 *                            modify -p option
 *           2013/01/25  1.4  fix bug on showing message
 *           2014/02/21  1.5  ignore SIG_HUP
 *           2014/08/10  1.5  fix bug on showing message
 *           2014/08/26  1.6  support input format gw10, binex and rt17
 *           2014/10/14  1.7  use stdin or stdout if option -in or -out omitted
 *           2014/11/08  1.8  add option -a, -i and -o
 *           2015/03/23  1.9  fix bug on parsing of command line options
 *           2016/01/23  1.10 enable septentrio
 *           2016/01/26  1.11 fix bug on station position by -p option (#126)
 *                            add option -px
 *           2016/07/01  1.12 support CMR/CMR+
 *           2016/07/23  1.13 add option -c1 -c2 -c3 -c4
 *           2016/09/03  1.14 support ntrip caster
 *                            add option -ft,-fl
 *           2016/09/06  1.15 add reload soure table by USR2 signal
 *           2016/09/17  1.16 add option -b
 *           2017/05/26  1.17 add input format tersus
 *-----------------------------------------------------------------------------*/
#include "rtklib.h"
#include <signal.h>
#include <unistd.h>

#define PRGNAME "str2str"      /* program name */
#define MAX_STREAMS 9          /* max number of streams */
#define TRFILE "str2str.trace" /* trace file */

/* global variables ----------------------------------------------------------*/
static strsvr_t strsvr;          /* stream server */
static volatile int intrflg = 0; /* interrupt flag */
static char srctbl[1024] = "";   /* source table file */

/* help text -----------------------------------------------------------------*/
static const char *help[] = {
    "",
    " usage: str2str [-in stream] [-out stream [-out stream...]] [options]",
    "",
    " Input data from a stream and divide and output them to multiple streams",
    " The input stream can be serial, tcp client, tcp server, ntrip client, or",
    " file. The output stream can be serial, tcp client, tcp server, ntrip "
    "server,",
    " or file. str2str is a resident type application. To stop it, type ctr-c "
    "in",
    " console if run foreground or send signal SIGINT for background process.",
    " if run foreground or send signal SIGINT for background process.",
    " if both of the input stream and the output stream follow #format, the",
    " format of input messages are converted to output. To specify the output",
    " messages, use -msg option. If the option -in or -out omitted, stdin for",
    " input or stdout for output is used. If the stream in the option -in or "
    "-out",
#if defined(__GNUC__) && defined(__MINGW32__)
    " is null, stdin or stdout is used as well.",
#else
    " is null, stdin or stdout is used as well. To reload ntrip source table",
    " specified by the option -ft, send SIGUSR2 to the process",
#endif
    " Command options are as follows.",
    "",
    " -in  stream[#format] input  stream path and format",
    " -out stream[#format] output stream path and format",
    "",
    "  stream path",
    "    serial       : serial://port[:brate[:bsize[:parity[:stopb[:fctr]]]]]",
    "    tcp server   : tcpsvr://:port",
    "    tcp client   : tcpcli://addr[:port]",
    "    ntrip client : ntrip://[user[:passwd]@]addr[:port][/mntpnt]",
    "    ntrip server : ntrips://[:passwd@]addr[:port]/mntpnt[:str] (only out)",
    "    ntrip caster server: ntripc_s://[:passwd@][:port] (only in)",
    "    ntrip caster client: ntripc_c://[user:passwd@][:port]/mntpnt (only "
    "out)",
    "    file         : [file://]path[::T][::+start][::xseppd][::S=swap]",
    "",
    "  format",
    "    rtcm2        : RTCM 2 (only in)",
    "    rtcm3        : RTCM 3",
    "    nov          : NovAtel OEMV/4/6,OEMStar (only in)",
    "    oem3         : NovAtel OEM3 (only in)",
    "    ubx          : ublox LEA-4T/5T/6T (only in)",
    "    sbp          : Swift Navigation SBP",
    "    json         : Swift Navigation SBP-JSON",
    "    stq          : SkyTraq S1315F (only in)",
    "    gw10         : Furuno GW10 (only in)",
    "    javad        : Javad (only in)",
    "    nvs          : NVS BINR (only in)",
    "    binex        : BINEX (only in)",
    "    rt17         : Trimble RT17 (only in)",
    "    sbf          : Septentrio SBF (only in)",
    "    cmr          : CMR/CMR+ (only in)",
    "    tersus       : TERSUS (only in)",
    "",
    " -msg \"type[(tint)][,type[(tint)]...]\"",
    "                   rtcm message types and output intervals (s)",
    " -sta sta          station id",
    " -opt opt          receiver dependent options",
    " -s  msec          timeout time (ms) [10000]",
    " -r  msec          reconnect interval (ms) [10000]",
    " -n  msec          nmea request cycle (ms) [0]",
    " -f  sec           file swap margin (s) [30]",
    " -c  file          input commands file [no]",
    " -c1 file          output 1 commands file [no]",
    " -c2 file          output 2 commands file [no]",
    " -c3 file          output 3 commands file [no]",
    " -c4 file          output 4 commands file [no]",
    " -p  lat lon hgt   station position (latitude/longitude/height) (deg,m)",
    " -px x y z         station position (x/y/z-ecef) (m)",
    " -a  antinfo       antenna info (separated by ,)",
    " -i  rcvinfo       receiver info (separated by ,)",
    " -o  e n u         antenna offset (e,n,u) (m)",
    " -l  local_dir     ftp/http local directory []",
    " -x  proxy_addr    http/ntrip proxy address [no]",
    " -b  str_no        relay back messages from output str to input str [no]",
    " -t  level         trace level [0]",
    " -ft file          ntrip souce table file []",
    " -fl file          log file [str2str.trace]",
    " -h                print help",
};
/* print help ----------------------------------------------------------------*/
static void printhelp(void) {
  int i;
  for (i = 0; i < sizeof(help) / sizeof(*help); i++)
    fprintf(stderr, "%s\n", help[i]);
  exit(0);
}
/* signal handler ------------------------------------------------------------*/
static void sigfunc(int sig) { intrflg = 1; }
/* reload source table by SIGUSR2 --------------------------------------------*/
static void reload_srctbl(int sig) {
  strsvrsetsrctbl(&strsvr, srctbl);
#if defined(__GNUC__) && defined(__MINGW32__)
  signal(SIGUSR2, reload_srctbl);
#endif
}
/* decode format -------------------------------------------------------------*/
static void decodefmt(char *path, int *fmt) {
  char *p;

  *fmt = -1;

  if ((p = strrchr(path, '#'))) {
    if (!strcmp(p, "#rtcm2"))
      *fmt = STRFMT_RTCM2;
    else if (!strcmp(p, "#rtcm3"))
      *fmt = STRFMT_RTCM3;
    else if (!strcmp(p, "#nov"))
      *fmt = STRFMT_OEM4;
    else if (!strcmp(p, "#oem3"))
      *fmt = STRFMT_OEM3;
    else if (!strcmp(p, "#ubx"))
      *fmt = STRFMT_UBX;
    else if (!strcmp(p, "#sbp"))
      *fmt = STRFMT_SBP;
    else if (!strcmp(p, "#json"))
      *fmt = STRFMT_SBPJSON;
    else if (!strcmp(p, "#stq"))
      *fmt = STRFMT_STQ;
    else if (!strcmp(p, "#gw10"))
      *fmt = STRFMT_GW10;
    else if (!strcmp(p, "#javad"))
      *fmt = STRFMT_JAVAD;
    else if (!strcmp(p, "#nvs"))
      *fmt = STRFMT_NVS;
    else if (!strcmp(p, "#binex"))
      *fmt = STRFMT_BINEX;
    else if (!strcmp(p, "#rt17"))
      *fmt = STRFMT_RT17;
    else if (!strcmp(p, "#sbf"))
      *fmt = STRFMT_SEPT;
    else if (!strcmp(p, "#cmr"))
      *fmt = STRFMT_CMR;
    else if (!strcmp(p, "#tersus"))
      *fmt = STRFMT_TERSUS;
    else
      return;
    *p = '\0';
  }
}
/* decode stream path --------------------------------------------------------*/
static int decodepath(const char *path, int *type, char *strpath, int *fmt) {
  char buff[1024], *p;

  strcpy(buff, path);

  /* decode format */
  decodefmt(buff, fmt);

  /* decode type */
  if (!(p = strstr(buff, "://"))) {
    strcpy(strpath, buff);
    *type = STR_FILE;
    return 1;
  }
  if (!strncmp(path, "serial", 6))
    *type = STR_SERIAL;
  else if (!strncmp(path, "tcpsvr", 6))
    *type = STR_TCPSVR;
  else if (!strncmp(path, "tcpcli", 6))
    *type = STR_TCPCLI;
  else if (!strncmp(path, "ntripc_s", 8))
    *type = STR_NTRIPC_S;
  else if (!strncmp(path, "ntripc_c", 8))
    *type = STR_NTRIPC_C;
  else if (!strncmp(path, "ntrips", 6))
    *type = STR_NTRIPSVR;
  else if (!strncmp(path, "ntrip", 5))
    *type = STR_NTRIPCLI;
  else if (!strncmp(path, "file", 4))
    *type = STR_FILE;
  else {
    fprintf(stderr, "stream path error: %s\n", buff);
    return 0;
  }
  strcpy(strpath, p + 3);
  return 1;
}
/* read receiver commands ----------------------------------------------------*/
static void readcmd(const char *file, char *cmd, int type) {
  FILE *fp;
  char buff[MAX_STREAMS], *p = cmd;
  int i = 0;

  *p = '\0';

  if (!(fp = fopen(file, "r")))
    return;

  while (fgets(buff, sizeof(buff), fp)) {
    if (*buff == '@')
      i++;
    else if (i == type && p + strlen(buff) + 1 < cmd + MAXRCVCMD) {
      p += sprintf(p, "%s", buff);
    }
  }
  fclose(fp);
}

/* str2str -------------------------------------------------------------------*/
int main(int argc, char **argv) {

  static char paths_strs[MAX_STREAMS][MAXSTRPATH] = {[0 ... (MAX_STREAMS-1)] = ""};
  char *paths[MAX_STREAMS] = {NULL};

  static char cmd_strs[MAX_STREAMS][MAXRCVCMD] = {[0 ... (MAX_STREAMS-1)] = ""};
  char *cmds[MAX_STREAMS] ={NULL};

  static char cmd_periodic_strs[MAX_STREAMS][MAXRCVCMD] = {[0 ... (MAX_STREAMS-1)] = ""};
  char *cmds_periodic[MAX_STREAMS] = {NULL};

  const char ss[] = {'E', '-', 'W', 'C', 'C'};
  char *cmdfile[MAX_STREAMS] = {NULL};
  strconv_t *conv[MAX_STREAMS] = {NULL};
  double pos[3] = {0}, stapos[3] = {0}, stadel[3] = {0};
  char *local = "", *proxy = "", *msg = "1004,1019", *opt = "", buff[256], *p;
  char strmsg[MAXSTRMSG] = "", *antinfo = "", *rcvinfo = "";
  char *ant[] = {"", "", ""}, *rcv[] = {"", "", ""}, *logfile = "";
  int i, j, dispint = 5000, trlevel = 0;
  int opts[] = {10000, 10000, 2000, 32768, 10, 0, 30, 0};
  int nout = 0, sta = 0;
  int types[MAX_STREAMS] = {STR_FILE};
  int stat[MAX_STREAMS] = {0};
  int byte[MAX_STREAMS] = {0};
  int bps[MAX_STREAMS] = {0};
  int fmts[MAX_STREAMS] = {0};

  for (i = 0; i < MAX_STREAMS; i++) {
    paths[i] = paths_strs[i];
    cmds[i] = cmd_strs[i];
    cmds_periodic[i] = cmd_periodic_strs[i];
  }
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-in") && i + 1 < argc) {
      if (!decodepath(argv[++i], types, paths[0], fmts)) {
        return -1;
      }
    } else if (!strcmp(argv[i], "-out") && ((i + 1) < argc) &&
               (nout < (MAX_STREAMS - 1))) {
      if (!decodepath(argv[++i], types + nout + 1, paths[nout + 1],
                      fmts + nout + 1)) {
        return -1;
      }
      nout++;
    } else if (!strcmp(argv[i], "-p") && i + 3 < argc) {
      pos[0] = atof(argv[++i]) * D2R;
      pos[1] = atof(argv[++i]) * D2R;
      pos[2] = atof(argv[++i]);
      pos2ecef(pos, stapos);
    } else if (!strcmp(argv[i], "-px") && i + 3 < argc) {
      stapos[0] = atof(argv[++i]);
      stapos[1] = atof(argv[++i]);
      stapos[2] = atof(argv[++i]);
    } else if (!strcmp(argv[i], "-o") && i + 3 < argc) {
      stadel[0] = atof(argv[++i]);
      stadel[1] = atof(argv[++i]);
      stadel[2] = atof(argv[++i]);
    } else if (!strcmp(argv[i], "-msg") && i + 1 < argc)
      msg = argv[++i];
    else if (!strcmp(argv[i], "-opt") && i + 1 < argc)
      opt = argv[++i];
    else if (!strcmp(argv[i], "-sta") && i + 1 < argc)
      sta = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-d") && i + 1 < argc)
      dispint = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-s") && i + 1 < argc)
      opts[0] = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-r") && i + 1 < argc)
      opts[1] = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-n") && i + 1 < argc)
      opts[5] = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-f") && i + 1 < argc)
      opts[6] = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-c") && i + 1 < argc)
      cmdfile[0] = argv[++i];
    else if (!strcmp(argv[i], "-c1") && i + 1 < argc)
      cmdfile[1] = argv[++i];
    else if (!strcmp(argv[i], "-c2") && i + 1 < argc)
      cmdfile[2] = argv[++i];
    else if (!strcmp(argv[i], "-c3") && i + 1 < argc)
      cmdfile[3] = argv[++i];
    else if (!strcmp(argv[i], "-c4") && i + 1 < argc)
      cmdfile[4] = argv[++i];
    else if (!strcmp(argv[i], "-a") && i + 1 < argc)
      antinfo = argv[++i];
    else if (!strcmp(argv[i], "-i") && i + 1 < argc)
      rcvinfo = argv[++i];
    else if (!strcmp(argv[i], "-l") && i + 1 < argc)
      local = argv[++i];
    else if (!strcmp(argv[i], "-x") && i + 1 < argc)
      proxy = argv[++i];
    else if (!strcmp(argv[i], "-b") && i + 1 < argc)
      opts[7] = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-ft") && i + 1 < argc)
      strcpy(srctbl, argv[++i]);
    else if (!strcmp(argv[i], "-fl") && i + 1 < argc)
      logfile = argv[++i];
    else if (!strcmp(argv[i], "-t") && i + 1 < argc)
      trlevel = atoi(argv[++i]);
    else if (*argv[i] == '-')
      printhelp();
  }
  if (nout == 0) {
    /* use stdout as only output */
    fprintf(stderr, "using stdout as output\n");
    nout = 1;
    decodepath("/dev/stdout", types + nout, paths[nout], fmts + nout);
  }

  for (i = 0; i < nout; i++) {
    if (fmts[i + 1] <= 0)
      continue;
    if (fmts[i + 1] != STRFMT_RTCM3) {
      fprintf(stderr, "unsupported output format\n");
      return -1;
    }
    /* if an output format was specified check the input format being valid */
    if (fmts[0] < 0) {
      fprintf(stderr, "specify input format\n");
      return -1;
    }
    conv[i] = strconvnew(fmts[0], fmts[i + 1], msg, sta, sta != 0, opt);
    if (NULL == conv[i]) {
      fprintf(stderr, "stream conversion error\n");
      return -1;
    }
    strcpy(buff, antinfo);
    for (p = strtok(buff, ","), j = 0; p && j < 3; p = strtok(NULL, ","))
      ant[j++] = p;
    strcpy(conv[i]->out.sta.antdes, ant[0]);
    strcpy(conv[i]->out.sta.antsno, ant[1]);
    conv[i]->out.sta.antsetup = atoi(ant[2]);
    strcpy(buff, rcvinfo);
    for (p = strtok(buff, ","), j = 0; p && j < 3; p = strtok(NULL, ","))
      rcv[j++] = p;
    strcpy(conv[i]->out.sta.rectype, rcv[0]);
    strcpy(conv[i]->out.sta.recver, rcv[1]);
    strcpy(conv[i]->out.sta.recsno, rcv[2]);
    matcpy(conv[i]->out.sta.pos, stapos, 3, 1);
    matcpy(conv[i]->out.sta.del, stadel, 3, 1);
  }
  signal(SIGTERM, sigfunc);
  signal(SIGINT, sigfunc);
#if defined(__GNUC__) && defined(__MINGW32__)
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
#endif

  strsvrinit(&strsvr, nout);

  if (trlevel > 0) {
    traceopen(*logfile ? logfile : TRFILE);
    tracelevel(trlevel);
  }
  fprintf(stderr, "stream server start\n");

  strsetdir(local);
  strsetproxy(proxy);

  for (i = 0; i < MAX_STREAMS; i++) {
    if (cmdfile[i]) {
      readcmd(cmdfile[i], cmds[i], 0);
    }
    if (cmdfile[i]) {
      readcmd(cmdfile[i], cmds_periodic[i], 2);
    }
  }
  /* start stream server */
  if (!strsvrstart(&strsvr, opts, types, paths, conv, cmds, cmds_periodic,
                   stapos)) {
    fprintf(stderr, "stream server start error\n");
    return -1;
  }
  /* read and set ntrip source table */
  if (*srctbl) {
    strsvrsetsrctbl(&strsvr, srctbl);
#if defined(__GNUC__) && defined(__MINGW32__)
    signal(SIGUSR2, reload_srctbl);
#endif
  }
  for (intrflg = 0; !intrflg;) {

    /* get stream server status */
    strsvrstat(&strsvr, stat, byte, bps, strmsg);

    /* show stream server status */
    for (i = 0, p = buff; i < MAX_STREAMS; i++)
      p += sprintf(p, "%c", ss[stat[i] + 1]);

    fprintf(stderr, "%s [%s] %10d B %7d bps %s\n",
            time_str(utc2gpst(timeget()), 0), buff, byte[0], bps[0], strmsg);

    sleepms(dispint);
  }
  for (i = 0; i < MAX_STREAMS; i++) {
    if (cmdfile[i])
      readcmd(cmdfile[i], cmds[i], 1);
  }
  /* stop stream server */
  strsvrstop(&strsvr, cmds);

  for (i = 0; i < nout; i++) {
    strconvfree(conv[i]);
  }
  if (trlevel > 0) {
    traceclose();
  }
  fprintf(stderr, "stream server stop\n");
  return 0;
}
