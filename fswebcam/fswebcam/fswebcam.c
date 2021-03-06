/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2014 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <gd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fswebcam.h"
#include "log.h"
#include "src.h"
#include "dec.h"
/* #include "effects.h" */
#include "parse.h"

typedef int bool;
#define true 1
#define false 0

#define ALIGN_LEFT   (0)
#define ALIGN_CENTER (1)
#define ALIGN_RIGHT  (2)

#define NO_BANNER     (0)
#define TOP_BANNER    (1)
#define BOTTOM_BANNER (2)

#define FORMAT_JPEG (0)
#define FORMAT_PNG  (1)

#define MODE_DEBUG

enum fswc_options {
	OPT_VERSION = 128,
	OPT_PID,
	OPT_OFFSET,
	OPT_LIST_INPUTS,
	OPT_LIST_TUNERS,
	OPT_LIST_FORMATS,
	OPT_LIST_CONTROLS,
	OPT_LIST_FRAMESIZES,
	OPT_LIST_FRAMERATES,
	OPT_BRIGHTNESS,
	OPT_HUE,
	OPT_COLOUR,
	OPT_CONTRAST,
	OPT_WHITENESS,
	OPT_REVERT,
	OPT_FLIP,
	OPT_CROP,
	OPT_SCALE,
	OPT_ROTATE,
	OPT_DEINTERLACE,
	OPT_INVERT,
	OPT_GREYSCALE,
	OPT_SWAPCHANNELS,
	OPT_NO_BANNER,
	OPT_TOP_BANNER,
	OPT_BOTTOM_BANNER,
	OPT_BG_COLOUR,
	OPT_BL_COLOUR,
	OPT_FG_COLOUR,
	OPT_FONT,
	OPT_NO_SHADOW,
	OPT_SHADOW,
	OPT_TITLE,
	OPT_NO_TITLE,
	OPT_SUBTITLE,
	OPT_NO_SUBTITLE,
	OPT_TIMESTAMP,
	OPT_NO_TIMESTAMP,
	OPT_GMT,
	OPT_INFO,
	OPT_NO_INFO,
	OPT_UNDERLAY,
	OPT_NO_UNDERLAY,
	OPT_OVERLAY,
	OPT_NO_OVERLAY,
	OPT_JPEG,
	OPT_PNG,
	OPT_SAVE,
	OPT_EXEC,
	OPT_DUMPFRAME,
	OPT_FPS,
};

typedef struct {

	/* List of options. */
	char *opts;
	const struct option *long_opts;

	/* When reading from the command line. */
	int opt_index;

	/* When reading from a configuration file. */
	char *filename;
	FILE *f;
	size_t line;

} fswc_getopt_t;

typedef struct {
	uint16_t id;
	char    *options;
} fswebcam_job_t;

typedef struct {

	/* General options. */
	unsigned long loop;
	signed long offset;
	unsigned char background;
	char *pidfile;
	char *logfile;
	char gmt;

	/* Capture start time. */
	time_t start;

	/* Device options. */
	char *device;
	char *input;
	unsigned char tuner;
	unsigned long frequency;
	unsigned long delay;
	char use_read;
	uint8_t list;

	/* Image capture options. */
	int width;
	int height;
	unsigned int frames;
	unsigned int fps;
	unsigned int skipframes;
	int palette;
	src_option_t **option;
	char *dumpframe;

	/* Job queue. */
	uint8_t jobs;
	fswebcam_job_t **job;

	/* Banner options. */
	char banner;
	uint32_t bg_colour;
	uint32_t bl_colour;
	uint32_t fg_colour;
	char *title;
	char *subtitle;
	char *timestamp;
	char *info;
	char *font;
	int fontsize;
	char shadow;

	/* Overlay options. */
	char *underlay;
	char *overlay;

	/* Output options. */
	char *filename;
	char format;
	char compression;

	char mode;

	uint32_t threshold;
	uint32_t multiplier;
	uint32_t lowerscan;

} fswebcam_config_t;


volatile char received_sigusr1 = 0;
volatile char received_sighup  = 0;
volatile char received_sigterm = 0;

void fswc_signal_usr1_handler(int signum)
{
	// Catches SIGUSR1
	INFO("Caught signal SIGUSR1.");
	received_sigusr1 = 1;
}

void fswc_signal_hup_handler(int signum)
{
	// Catches SIGHUP
	INFO("Caught signal SIGHUP.");
	received_sighup = 1;
}

void fswc_signal_term_handler(int signum)
{
	char *signame;

	// Catches SIGTERM and SIGINT
	switch(signum)
	{
	case SIGTERM: signame = "SIGTERM"; break;
	case SIGINT:  signame = "SIGINT"; break;
	default:      signame = "Unknown"; break;
	}

	INFO("Caught signal %s", signame);
	received_sigterm = 1;
}

int fswc_setup_signals()
{
	signal(SIGUSR1, fswc_signal_usr1_handler);
	signal(SIGHUP,  fswc_signal_hup_handler);
	signal(SIGTERM, fswc_signal_term_handler);
	signal(SIGINT,  fswc_signal_term_handler);

	return(0);
}

char *fswc_strftime(char *dst, size_t max, char *src,
                    time_t timestamp, int gmt)
{
	struct tm tm_timestamp;

	/* Clear target string, and verify source is set */
	*dst = '\0';
	if(!src) return(dst);

	/* Set the time structure. */
	if(gmt) gmtime_r(&timestamp, &tm_timestamp);
	else localtime_r(&timestamp, &tm_timestamp);

	/* Create the string */
	strftime(dst, max, src, &tm_timestamp);

	return(dst);
}

char *fswc_strduptime(char *src, time_t timestamp, int gmt)
{
	struct tm tm_timestamp;
	char *dst;
	size_t l;

	if(!src) return(NULL);

	/* Set the time structure. */
	if(gmt) gmtime_r(&timestamp, &tm_timestamp);
	else localtime_r(&timestamp, &tm_timestamp);

	dst = NULL;
	l = strlen(src) * 2;

	while(1)
	{
		size_t r;
		char *t = realloc(dst, l);

		if(!t)
		{
			free(dst);
			return(NULL);
		}

		dst = t;

		*dst = 1;
		r = strftime(dst, l, src, &tm_timestamp);

		if(r > 0 && r < l) return(dst);
		if(r == 0 && *dst == '\0') return(dst);

		l *= 2;
	}

	return(NULL);
}

int fswc_exec(fswebcam_config_t *config, char *cmd)
{
	char *cmdline;
	FILE *p;

	cmdline = fswc_strduptime(cmd, config->start, config->gmt);
	if(!cmdline) return(-1);

	MSG("Executing '%s'...", cmdline);

	p = popen(cmdline, "r");
	free(cmdline);

	if(p)
	{
		while(!feof(p))
		{
			char *n;
			char line[1024];

			if(!fgets(line, 1024, p)) break;

			while((n = strchr(line, '\n'))) *n = '\0';
			MSG("%s", line);
		}

		pclose(p);
	}
	else
	{
		ERROR("popen: %s", strerror(errno));
		return(-1);
	}

	return(0);
}

int fswc_grab(fswebcam_config_t *config)
{
	uint32_t frame;
	uint32_t x, y, h, w, t;
	avgbmp_t *abitmap, *pbitmap, *currBitMap, *prevBitMap, *baseBitMap;
	avgbmp_t *p_currBitMap, *p_prevBitMap, *p_baseBitMap;
	gdImage *image, *original;
	uint8_t modified;
	src_t src;

	uint32_t curr, prev, currSum, prevSum;


	uint16_t scale = 10;

	char buffer[71];


	/* Record the start time. */
	config->start = time(NULL);

	/* Set source options... */
	memset(&src, 0, sizeof(src));
	src.input      = config->input;
	src.tuner      = config->tuner;
	src.frequency  = config->frequency;
	src.delay      = config->delay;
	src.timeout    = 10; /* seconds */
	src.use_read   = config->use_read;
	src.list       = config->list;
	src.palette    = config->palette;
	src.width      = config->width;
	src.height     = config->height;
	src.fps        = config->fps;
	src.option     = config->option;


	#ifdef MODE_DEBUG
		HEAD("--- Opening %s...", config->device);
	#endif

	if(src_open(&src, config->device) == -1) return(-1);

	/* The source may have adjusted the width and height we passed
	 * to it. Update the main config to match. */
	config->width  = src.width;
	config->height = src.height;


	/* Allocate memory for the reduced bitmap buffer. */
	//currBitMap = calloc((config->width / scale) * (config->height / scale), sizeof(avgbmp_t));
	int reducedWidth = ((config->width / scale) + 1);
	int reducedHeight = ((config->height / scale) + 1);

	currBitMap = calloc( reducedWidth * reducedHeight, sizeof(avgbmp_t));
	prevBitMap = calloc( reducedWidth * reducedHeight, sizeof(avgbmp_t));
	baseBitMap = calloc( reducedWidth * reducedHeight, sizeof(avgbmp_t));

	p_currBitMap = currBitMap;
	p_prevBitMap = prevBitMap;
	p_baseBitMap = baseBitMap;

	if(!currBitMap || !prevBitMap || !baseBitMap)
	{
		ERROR("Out of memory.");
		return(-1);
	}

// every frame
// grab current frame
// reduce current frame


// scan from starting pixel...
// compare to previous frame pixels
// record total difference

// if there is a current baseline\
// compare to baseline - if there is a y difference greater than B-threshold
// then

// if total difference has remained less than T-threshold for 20 frames
// then use this as a baseline


// determine whether pixels have changed beyond some threshold

	bool debugout = false;
	bool diffout = false;

	if ( config->mode == 'x' ) {
		debugout = true;
	}
	if ( config->mode == 'z' ) {
		debugout = true;
		diffout = true;
	}

//	for(frame = 0; frame < config->frames; frame++)
	while ( 1 == 1 )
	{

		if(received_sigterm)
		{
			MSG("Received TERM signal... exiting.");
			break;
		}

		// copy currBitMap to prevBitMap

		prevBitMap = p_prevBitMap;
		currBitMap = p_currBitMap;

		int maxCurr = 0;
		int maxPrev = 0;
		int diffSum = 0;

		for ( t=0; t < reducedWidth * reducedHeight; t++ ){
			*(p_prevBitMap + t) = *(p_currBitMap + t);
		}

		if(src_grab(&src) == -1) break;

		reduce_img(&src, currBitMap, scale);

		currSum = 0;
		prevSum = 0;

		if ( debugout ) {
			printf("\033[%d;%dH", 0, 0);
		}

		for ( t=0; t < 71; t++ ){ buffer[t] = 0; }

		int cutoff = config->lowerscan;

		for ( h=0; h < ( reducedHeight - cutoff ); h++ ){

			for ( w=0; w < reducedWidth; w++ ){

				curr = *(currBitMap++);
				prev = *(prevBitMap++);

				currSum += curr;
				prevSum += prev;

				int diff = ( abs(curr - prev) * config->multiplier );
				diffSum += diff;

				if ( diff > config->threshold ) {
					if ( h > 253 ) { h = 253; }
					if ( diff > 253 ) { diff = 253; }

					buffer[w*2] = h;
					buffer[(w*2) + 1] = diff;
				}

				if ( debugout ) {
					if ( diffout ) {
						print_graphic( diff );
					} else {
						print_graphic( curr );
					}
				}
			}

			if ( debugout ) {
				printf("\n",0);
			}
		}

		buffer[70] = 254;

		if ( debugout ) {
			printf("\033[%d;%dH", 10, 40);
			printf("diffSum/lower- %d %d", diffSum / (reducedWidth * ( reducedHeight - cutoff )), config->lowerscan );
		} else {
			if( fwrite(buffer, 1, 71, stdout) != 71 ) {
			    perror("fwrite");
			};
			fflush( stdout );
		}
	}

	/* We are now finished with the capture card. */
	src_close(&src);

	free(p_currBitMap);
	free(p_prevBitMap);
	free(p_baseBitMap);

	return(0);
}




int reduce_img(src_t *src, avgbmp_t *rbitmap, uint16_t scale )
{
  uint32_t x, y, xs, ys, hlength;
	uint8_t *himg = NULL;
  uint8_t rv, gv, bv;
	gdImage *im;
	int i;

	/* MJPEG data may lack the DHT segment required for decoding... */
	i = verify_jpeg_dht(src->img, src->length, &himg, &hlength);

	im = gdImageCreateFromJpegPtr(hlength, himg);
	if(i == 1) free(himg);

	if(!im) return(-1);

  for( y = 0; y < src->height; y += scale ) {
    for (x = 0; x < src->width; x += scale ) {
      int val = 0;
      int avgVal = 0;

      for ( ys = 0; ys < scale; ys++ ) {
        for ( xs = 0; xs < scale; xs++ ) {
          int c = gdImageGetPixel(im, x+xs, y+ys);

          rv = (c & 0xFF0000) >> 16;
          gv = (c & 0x00FF00) >> 8;
          bv = (c & 0x0000FF);

          val += (( rv + gv + bv ) / 3 );
        }
      }

      avgVal = val / ( scale * scale );

      *(rbitmap++) = avgVal;
    }
  }
	gdImageDestroy(im);

  return(0);
}


int print_aligned( int input )
{
  if ( input < 10 ){
    printf("%d   ", input);
  } else if ( input < 100 ){
    printf("%d  ", input);
  } else {
    printf("%d ", input);
  }

  return(0);
}

int print_graphic( int input )
{

  if ( input < 25 ){
    printf(".");
  } else if ( input < 50 ){
    printf("-");
  } else if ( input < 75 ){
    printf(":");
  } else if ( input < 100 ){
    printf("*");
  } else if ( input < 125 ){
    printf("o");
  } else if ( input < 150 ){
    printf("=");
  } else if ( input < 175 ){
    printf("&");
  } else if ( input < 200 ){
    printf("%");
  } else if ( input < 225 ){
    printf("#");
  } else {
    printf("@");
  }

/*
	if ( input < 50 ){
	  printf(".");
	} else if ( input < 100 ){
	  printf(":");
	} else if ( input < 150 ){
	  printf("=");
	} else if ( input < 200 ){
	  printf("H");
	} else {
	  printf("#");
	}
	*/
  return(0);
}




















int fswc_openlog(fswebcam_config_t *config)
{
	char *s;
	int r;

	/* Get the first part of the filename. */
	s = argdup(config->logfile, ":", 0, 0);
	if(!s)
	{
		ERROR("Out of memory.");
		return(-1);
	}

	if(!strcasecmp(s, "file"))
	{
		free(s);

		s = argdup(config->logfile, ":", 1, 0);
		if(!s)
		{
			ERROR("No log file was specified.");
			return(-1);
		}
	}
	else if(!strcasecmp(s, "syslog"))
	{
		free(s);
		log_syslog(1);
		return(0);
	}

	r = log_open(s);
	free(s);

	return(r);
}

int fswc_background(fswebcam_config_t *config)
{
	pid_t pid, sid;

	/* Silence the output if not logging to a file. */
	if(!config->logfile) log_set_fd(-1);

	/* Fork into the background. */
	pid = fork();

	if(pid < 0)
	{
		ERROR("Error going into the background.");
		ERROR("fork: %s", strerror(errno));
		return(-1);
	}

	/* Is this the parent process? If so, end it. */
	if(pid > 0) exit(0);

	umask(0);

	/* Create a new SID for the child process. */
	sid = setsid();
	if(sid < 0)
	{
		ERROR("Error going into the background.");
		ERROR("setsid: %s", strerror(errno));
		return(-1);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	return(0);
}

int fswc_savepid(fswebcam_config_t *config)
{
	FILE *fpid = fopen(config->pidfile, "wt");

	if(!fpid)
	{
		ERROR("Error saving PID to file '%s'",config->pidfile);
		ERROR("fopen: %s", strerror(errno));

		return(-1);
	}

	fprintf(fpid, "%i\n", getpid());
	fclose(fpid);

	return(0);
}

int fswc_find_palette(char *name)
{
	int i;

	/* Scan through the palette table until a match is found. */
	for(i = 0; src_palette[i].name != NULL; i++)
	{
		/* If a match was found, return the index. */
		if(!strcasecmp(src_palette[i].name, name)) return(i);
	}

	/* No match was found. */
	ERROR("Unrecognised palette format \"%s\". Supported formats:", name);

	for(i = 0; src_palette[i].name != NULL; i++)
		ERROR("%s", src_palette[i].name);

	return(-1);
}

int fswc_usage()
{
	printf("Usage: fswebcam [<options>] <filename> [[<options>] <filename> ... ]\n"
	       "\n"
	       " Options:\n"
	       "\n"
	       " -?, --help                   Display this help page and exit.\n"
				 " -X, --debug-curr             Debug output current frame out.\n"
				 " -Z, --debug-diff             Debug output difference out.\n"
				 " -N, --threshold              Threshold for difference.\n"
				 " -M, --multiplier             Multiplier for difference.\n"
				 " -B, --lowerscan              Lower Scan Limit.\n"
	       " -c, --config <filename>      Load configuration from file.\n"
	       " -q, --quiet                  Hides all messages except for errors.\n"
	       " -v, --verbose                Displays extra messages while capturing\n"
	       "     --version                Displays the version and exits.\n"
	       " -l, --loop <seconds>         Run in loop mode.\n"
	       "     --offset <seconds>       Sets the capture time offset in loop mode.\n"
	       " -b, --background             Run in the background.\n"
	       "     --pid <filename>         Saves background process PID to filename.\n"
	       " -L, --log [file/syslog:]<filename> Redirect log messages to a file or syslog.\n"
	       " -o, --output <filename>      Output the log to a file.\n"
	       " -d, --device <name>          Sets the source to use.\n"
	       " -i, --input <number/name>    Selects the input to use.\n"
	       "     --list-inputs            Displays available inputs.\n"
	       " -t, --tuner <number>         Selects the tuner to use.\n"
	       "     --list-tuners            Displays available tuners.\n"
	       " -f, --frequency <number>     Selects the frequency use.\n"
	       " -p, --palette <name>         Selects the palette format to use.\n"
	       " -D, --delay <number>         Sets the pre-capture delay time. (seconds)\n"
	       " -r, --resolution <size>      Sets the capture resolution.\n"
	       "     --fps <framerate>        Sets the capture frame rate.\n"
	       "     --list-framesizes        Displays the available frame sizes.\n"
	       "     --list-framerates        Displays the available frame rates.\n"
	       " -F, --frames <number>        Sets the number of frames to capture.\n"
	       " -S, --skip <number>          Sets the number of frames to skip.\n"
	       "     --dumpframe <filename>   Dump a raw frame to file.\n"
	       " -R, --read                   Use read() to capture images.\n"
	       "     --list-formats           Displays the available capture formats.\n"
	       " -s, --set <name>=<value>     Sets a control value.\n"
	       "     --list-controls          Displays the available controls.\n"
	       "     --revert                 Restores original captured image.\n"
	       "     --flip <direction>       Flips the image. (h, v)\n"
	       "     --crop <size>[,<offset>] Crop a part of the image.\n"
	       "     --scale <size>           Scales the image.\n"
	       "     --rotate <angle>         Rotates the image in right angles.\n"
	       "     --deinterlace            Reduces interlace artifacts.\n"
	       "     --invert                 Inverts the images colours.\n"
	       "     --greyscale              Removes colour from the image.\n"
	       "     --swapchannels <c1c2>    Swap channels c1 and c2.\n"
	       "     --no-banner              Hides the banner.\n"
	       "     --top-banner             Puts the banner at the top.\n"
	       "     --bottom-banner          Puts the banner at the bottom. (Default)\n"
	       "     --banner-colour <colour> Sets the banner colour. (#AARRGGBB)\n"
	       "     --line-colour <colour>   Sets the banner line colour.\n"
	       "     --text-colour <colour>   Sets the text colour.\n"
	       "     --font <[name][:size]>   Sets the font and/or size.\n"
	       "     --no-shadow              Disables the text shadow.\n"
	       "     --shadow                 Enables the text shadow.\n"
	       "     --title <text>           Sets the main title. (top left)\n"
	       "     --no-title               Clears the main title.\n"
	       "     --subtitle <text>        Sets the sub-title. (bottom left)\n"
	       "     --no-subtitle            Clears the sub-title.\n"
	       "     --timestamp <format>     Sets the timestamp format. (top right)\n"
	       "     --no-timestamp           Clears the timestamp.\n"
	       "     --gmt                    Use GMT instead of local timezone.\n"
	       "     --info <text>            Sets the info text. (bottom right)\n"
	       "     --no-info                Clears the info text.\n"
	       "     --underlay <PNG image>   Sets the underlay image.\n"
	       "     --no-underlay            Clears the underlay.\n"
	       "     --overlay <PNG image>    Sets the overlay image.\n"
	       "     --no-overlay             Clears the overlay.\n"
	       "     --jpeg <factor>          Outputs a JPEG image. (-1, 0 - 95)\n"
	       "     --png <factor>           Outputs a PNG image. (-1, 0 - 9)\n"
	       "     --save <filename>        Save image to file.\n"
	       "     --exec <command>         Execute a command and wait for it to complete.\n"
	       "\n");

	return(0);
}

int fswc_add_job(fswebcam_config_t *config, uint16_t id, char *options)
{
	fswebcam_job_t *job;
	void *n;

	job = malloc(sizeof(fswebcam_job_t));
	if(!job)
	{
		ERROR("Out of memory.");
		return(-1);
	}

	job->id = id;
	if(options) job->options = strdup(options);
	else job->options = NULL;

	/* Increase the size of the job queue. */
	n = realloc(config->job, sizeof(fswebcam_job_t *) * (config->jobs + 1));
	if(!n)
	{
		ERROR("Out of memory.");

		if(job->options) free(job->options);
		free(job);

		return(-1);
	}

	config->job = n;

	/* Add the new job to the queue. */
	config->job[config->jobs++] = job;

	return(0);
}

int fswc_free_jobs(fswebcam_config_t *config)
{
	int i;

	/* Scan through all the jobs and free the memory. */
	for(i = 0; i < config->jobs; i++)
	{
		if(config->job[i]->options) free(config->job[i]->options);
		free(config->job[i]);
	}

	/* Free the job queue, set settings to zero. */
	free(config->job);
	config->job = NULL;
	config->jobs = 0;

	return(0);
}

int fswc_set_option(fswebcam_config_t *config, char *option)
{
	char *name, *value;

	if(!option) return(-1);

	name = strdup(option);
	if(!name)
	{
		ERROR("Out of memory.");
		return(-1);
	}

	value = strchr(name, '=');
	if(value)
	{
		*(value++) = '\0';
		if(*value == '\0') value = NULL;
	}

	src_set_option(&config->option, name, value);

	free(name);

	return(0);
}

int fswc_getopt_file(fswc_getopt_t *s)
{
	char line[1024];
	char *arg, *val;
	struct option *opt;

	while(fgets(line, 1024, s->f))
	{
		s->line++;
		strtrim(line, WHITESPACE);
		arg = argdup(line, WHITESPACE, 0, 0);

		if(!arg) continue;
		if(*arg == '#') continue;

		/* Find argument in the list. */
		opt = (struct option *) s->long_opts;
		while(opt->name)
		{
			if(!strcasecmp(opt->name, arg)) break;
			opt++;
		}

		if(!opt->name)
		{
			ERROR("Unknown argument: %s", arg);
			WARN("%s,%i: %s", s->filename, s->line, line);
			free(arg);
			return(-2);
		}

		if(opt->val == 'c')
		{
			ERROR("You can't use config from a configuration file.");
			WARN("%s,%i: %s", s->filename, s->line, line);
			free(arg);
			return(-2);
		}

		if(opt->has_arg)
		{
			val = argdup(line, WHITESPACE, 1, 0);
			optarg = val;
		}

		free(arg);

		return(opt->val);
	}

	/* Have we reached the end of the file? */
	if(feof(s->f)) return(-1);

	/* No.. there has been an error. */
	ERROR("fread: %s", strerror(errno));

	return(-2);
}

int fswc_getopt(fswc_getopt_t *s, int argc, char *argv[])
{
	int c;

	/* If a conf file is opened, read from that. */
	if(s->f)
	{
		/* Read until we find an argument, error or EOF. */
		c = fswc_getopt_file(s);

		if(c < 0)
		{
			/* EOF or error. */
			fclose(s->f);
			s->f = NULL;

			if(c == -2) return('!');
		}
		else return(c);
	}

	/* Read from the command line. */
	c = getopt_long(argc, argv, s->opts, s->long_opts, &s->opt_index);

	return(c);
}

int fswc_getopts(fswebcam_config_t *config, int argc, char *argv[])
{
	int c;
	fswc_getopt_t s;
	static struct option long_opts[] =
	{
		{"threshold",       required_argument, 0, 'N'},
		{"multiplier",      required_argument, 0, 'M'},
		{"lowerscan",       required_argument, 0, 'B'},
		{"debug-diff",      no_argument,       0, 'Z'},
		{"debug-curr",      no_argument,       0, 'X'},
		{"help",            no_argument,       0, '?'},
		{"config",          required_argument, 0, 'c'},
		{"quiet",           no_argument,       0, 'q'},
		{"verbose",         no_argument,       0, 'v'},
		{"version",         no_argument,       0, OPT_VERSION},
		{"loop",            required_argument, 0, 'l'},
		{"offset",          required_argument, 0, OPT_OFFSET},
		{"background",      no_argument,       0, 'b'},
		{"pid",             required_argument, 0, OPT_PID},
		{"log",             required_argument, 0, 'L'},
		{"device",          required_argument, 0, 'd'},
		{"input",           required_argument, 0, 'i'},
		{"list-inputs",     no_argument,       0, OPT_LIST_INPUTS},
		{"tuner",           required_argument, 0, 't'},
		{"list-tuners",     no_argument,       0, OPT_LIST_TUNERS},
		{"frequency",       required_argument, 0, 'f'},
		{"delay",           required_argument, 0, 'D'},
		{"resolution",      required_argument, 0, 'r'},
		{"fps",	            required_argument, 0, OPT_FPS},
		{"list-framesizes", no_argument,       0, OPT_LIST_FRAMESIZES},
		{"list-framerates", no_argument,       0, OPT_LIST_FRAMERATES},
		{"frames",          required_argument, 0, 'F'},
		{"skip",            required_argument, 0, 'S'},
		{"palette",         required_argument, 0, 'p'},
		{"dumpframe",       required_argument, 0, OPT_DUMPFRAME},
		{"read",            no_argument,       0, 'R'},
		{"list-formats",    no_argument,       0, OPT_LIST_FORMATS},
		{"set",             required_argument, 0, 's'},
		{"list-controls",   no_argument,       0, OPT_LIST_CONTROLS},
		{"revert",          no_argument,       0, OPT_REVERT},
		{"flip",            required_argument, 0, OPT_FLIP},
		{"crop",            required_argument, 0, OPT_CROP},
		{"scale",           required_argument, 0, OPT_SCALE},
		{"rotate",          required_argument, 0, OPT_ROTATE},
		{"deinterlace",     no_argument,       0, OPT_DEINTERLACE},
		{"invert",          no_argument,       0, OPT_INVERT},
		{"greyscale",       no_argument,       0, OPT_GREYSCALE},
		{"swapchannels",    required_argument, 0, OPT_SWAPCHANNELS},
		{"no-banner",       no_argument,       0, OPT_NO_BANNER},
		{"top-banner",      no_argument,       0, OPT_TOP_BANNER},
		{"bottom-banner",   no_argument,       0, OPT_BOTTOM_BANNER},
		{"banner-colour",   required_argument, 0, OPT_BG_COLOUR},
		{"line-colour",     required_argument, 0, OPT_BL_COLOUR},
		{"text-colour",     required_argument, 0, OPT_FG_COLOUR},
		{"font",            required_argument, 0, OPT_FONT},
		{"no-shadow",       no_argument,       0, OPT_NO_SHADOW},
		{"shadow",          no_argument,       0, OPT_SHADOW},
		{"title",           required_argument, 0, OPT_TITLE},
		{"no-title",        no_argument,       0, OPT_NO_TITLE},
		{"subtitle",        required_argument, 0, OPT_SUBTITLE},
		{"no-subtitle",     no_argument,       0, OPT_NO_SUBTITLE},
		{"timestamp",       required_argument, 0, OPT_TIMESTAMP},
		{"no-timestamp",    no_argument,       0, OPT_NO_TIMESTAMP},
		{"gmt",             no_argument,       0, OPT_GMT},
		{"info",            required_argument, 0, OPT_INFO},
		{"no-info",         no_argument,       0, OPT_NO_INFO},
		{"underlay",        required_argument, 0, OPT_UNDERLAY},
		{"no-underlay",     no_argument,       0, OPT_NO_UNDERLAY},
		{"overlay",         required_argument, 0, OPT_OVERLAY},
		{"no-overlay",      no_argument,       0, OPT_NO_OVERLAY},
		{"jpeg",            required_argument, 0, OPT_JPEG},
		{"png",             required_argument, 0, OPT_PNG},
		{"save",            required_argument, 0, OPT_SAVE},
		{"exec",            required_argument, 0, OPT_EXEC},
		{0, 0, 0, 0}
	};
	char *opts = "-qc:vl:bL:d:i:t:f:D:r:F:s:S:p:R:X:Z:M:N:B:";

	s.opts      = opts;
	s.long_opts = long_opts;
	s.opt_index = 0;
	s.filename  = NULL;
	s.f         = NULL;
	s.line      = 0;

	/* Set the defaults. */
	config->loop = 0;
	config->offset = 0;
	config->background = 0;
	config->pidfile = NULL;
	config->logfile = NULL;
	config->gmt = 0;
	config->start = 0;
	config->device = strdup("/dev/video0");
	config->input = NULL;
	config->tuner = 0;
	config->frequency = 0;
	config->delay = 0;
	config->use_read = 0;
	config->list = 0;
	config->width = 384;
	config->height = 288;
	config->fps = 0;
	config->frames = 1;
	config->skipframes = 0;
	config->palette = SRC_PAL_ANY;
	config->option = NULL;
	config->dumpframe = NULL;
	config->jobs = 0;
	config->job = NULL;
	config->mode = 'c';
	config->threshold = 10;
	config->multiplier = 1;
	config->lowerscan = 0;

	/* Don't report errors. */
	opterr = 0;

	/* Reset getopt to ensure parsing begins at the first argument. */
	optind = 0;

	/* Parse the command line and any config files. */
	while((c = fswc_getopt(&s, argc, argv)) != -1)
	{
		printf("lowerscan - first%c", c);
		//exit(0);
		switch(c)
		{
		case '?': fswc_usage(); /* Command line error. */
		case '!': return(-1);   /* Conf file error. */
		case 'X': config->mode = 'x'; break;
		case 'Z': config->mode = 'z'; break;

		case 'N':
			config->threshold = atoi(optarg);
			break;
		case 'M':
			config->multiplier = atoi(optarg);
			break;
		case 'B':
			config->lowerscan = atoi(optarg);
			break;


		case 'c':
			INFO("Reading configuration from '%s'...", optarg);
			s.f = fopen(optarg, "rt");
			if(!s.f)
			{
				ERROR("fopen: %s", strerror(errno));
				return(-1);
			}
			s.line = 0;
			free(s.filename);
			s.filename = strdup(optarg);
			break;
		case OPT_VERSION:
			fprintf(stderr, "fswebcam %s\n", PACKAGE_VERSION);
			return(-1);
		case 'q':
			log_quiet(-1);
			log_verbose(0);
			break;
		case 'v':
			log_quiet(0);
			log_verbose(-1);
			break;
		case 'l':
			config->loop = atol(optarg);
			break;
		case OPT_OFFSET:
			config->offset = atol(optarg);
			break;
		case 'b':
			config->background = -1;
			break;
		case OPT_PID:
			if(config->pidfile) free(config->pidfile);
			config->pidfile = strdup(optarg);
			break;
		case 'L':
			if(config->logfile) free(config->logfile);
			config->logfile = strdup(optarg);
			break;
		case 'd':
			if(config->device) free(config->device);
			config->device = strdup(optarg);
			break;
		case 'i':
			if(config->input) free(config->input);
			config->input = strdup(optarg);
			break;
		case OPT_LIST_INPUTS:
			config->list |= SRC_LIST_INPUTS;
			break;
		case 't':
			config->tuner = atoi(optarg);
			break;
		case OPT_LIST_TUNERS:
			config->list |= SRC_LIST_TUNERS;
			break;
		case 'f':
			config->frequency = atof(optarg) * 1000;
			break;
		case 'D':
			config->delay = atoi(optarg);
			break;
		case 'r':
	 		config->width  = argtol(optarg, "x ", 0, 0, 10);
			config->height = argtol(optarg, "x ", 1, 0, 10);
			break;
		case OPT_FPS:
			config->fps = atoi(optarg);
			break;
		case 'F':
			config->frames = atoi(optarg);
			printf("frames - %d", config->frames );
			break;
		case 'S':
			config->skipframes = atoi(optarg);
			break;
		case 's':
			fswc_set_option(config, optarg);
			break;
		case OPT_LIST_CONTROLS:
			config->list |= SRC_LIST_CONTROLS;
			break;
		case 'p':
			config->palette = fswc_find_palette(optarg);
			if(config->palette == -1) return(-1);
			break;
		case 'R':
			config->use_read = -1;
			break;
		case OPT_LIST_FORMATS:
			config->list |= SRC_LIST_FORMATS;
			break;
		case OPT_GMT:
			config->gmt = -1;
			break;
		case OPT_DUMPFRAME:
			free(config->dumpframe);
			config->dumpframe = strdup(optarg);
			break;
		default:
			/* All other options are added to the job queue. */
			fswc_add_job(config, c, optarg);
			break;
		}
	}

	/* Do a sanity check on the options. */
	if(config->frequency < 0)       config->frequency = 0;
	if(config->width < 1)           config->width = 1;
	if(config->height < 1)          config->height = 1;
	if(config->frames < 1)          config->frames = 1;
	if(config->frames > MAX_FRAMES)
	{
		WARN("Requested %u frames, maximum is %u. Using that.",
		   config->frames, MAX_FRAMES);

		config->frames = MAX_FRAMES;
	}

	/* Correct offset if negative or out of range. */
	if(config->offset && (config->offset %= (signed long) config->loop) < 0)
		config->offset += config->loop;

	/* Free the config filename if set. */
	free(s.filename);

	return(0);
}

int fswc_free_config(fswebcam_config_t *config)
{
	free(config->pidfile);
	free(config->logfile);
	free(config->device);
	free(config->input);

	free(config->dumpframe);
  free(config->title);
	free(config->subtitle);
	free(config->timestamp);
	free(config->info);
	free(config->font);
	free(config->underlay);
	free(config->overlay);
	free(config->filename);

	src_free_options(&config->option);
	fswc_free_jobs(config);

	memset(config, 0, sizeof(fswebcam_config_t));

	return(0);
}

int main(int argc, char *argv[])
{
	fswebcam_config_t *config;

	/* Set the locale to the system default */
	setlocale(LC_ALL, "");

	/* Prepare the configuration structure. */
	config = calloc(sizeof(fswebcam_config_t), 1);
	if(!config)
	{
		WARN("Out of memory.");
		return(-1);
	}

	/* Set defaults and parse the command line. */
	if(fswc_getopts(config, argc, argv)) return(-1);

	/* Open the log file if one was specified. */
	if(config->logfile && fswc_openlog(config)) return(-1);

	/* Go into the background if requested. */
	if(config->background && fswc_background(config)) return(-1);

	/* Save PID of requested. */
	if(config->pidfile && fswc_savepid(config)) return(-1);

	/* Setup signal handlers. */
	if(fswc_setup_signals()) return(-1);

	/* Enable FontConfig support in GD */
	if(!gdFTUseFontConfig(1)) DEBUG("gd has no fontconfig support");

	/* Capture the image(s). */
	if(!config->loop) fswc_grab(config);
	else
	{
		/* Loop mode ... keep capturing images until terminated. */
		while(1 == 1)
		{
			time_t capturetime = time(NULL);
			char timestamp[32];

			/* Calculate when the next image is due. */
			capturetime -= (capturetime % config->loop);
			capturetime += config->loop + config->offset;

			/* Correct the capturetime if the offset pushes
			 * it to far into the future. */
			if(capturetime - time(NULL) > config->loop)
				capturetime -= config->loop;

			fswc_strftime(timestamp, 32, "%Y-%m-%d %H:%M:%S (%Z)",
			              capturetime, config->gmt);

			MSG(">>> Next image due: %s", timestamp);

			/* Wait until that time comes. */
			while(time(NULL) < capturetime)
			{
				usleep(250000);
				if(received_sighup)
				{
					/* Reload configuration. */
					MSG("Received HUP signal... reloading configuration.");
					fswc_free_config(config);
					fswc_getopts(config, argc, argv);

					/* Clear hup signal. */
					received_sighup = 0;
				}

				if(received_sigusr1)
				{
					MSG("Received USR1 signal... Capturing image.");
					break;
				}

				if(received_sigterm)
				{
					MSG("Received TERM signal... exiting.");
					break;
				}
			}

			if(received_sigterm) break;

			/* Clear usr1 signal. */
			received_sigusr1 = 0;

			/* Capture the image. */
			fswc_grab(config);
		}
	}

	/* Close the log file. */
	if(config->logfile) log_close();

	/* Free all used memory. */
	fswc_free_config(config);
	free(config);

	return(0);
}
