/*
 *   buffer.c
 *
 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
 *   Mon Apr 14 03:53:18 MET DST 1997
 */

#include <stdlib.h>
#include <errno.h>

#include "mpg123.h"

int outburst = MAXOUTBURST;
int preload;

static int intflag = FALSE;
static int usr1flag = FALSE;
static int stopped = FALSE;

static void catch_interrupt (void)
{
	intflag = TRUE;
}

static void catch_usr1 (void)
{
	usr1flag = TRUE;
}

static void catch_usr2 (void)
{
  stopped = TRUE;
  
  while(stopped)
    sleep(60000);
}

static void catch_cont(void)
{
  stopped = FALSE;
}

#if !defined(OS2) && !defined(GENERIC) && !defined(WIN32)

void buffer_loop(struct audio_info_struct *ai, sigset_t *oldsigset)
{
	int bytes;
	int my_fd = buffermem->fd[XF_READER];
	txfermem *xf = buffermem;
	int done = FALSE;

	catchsignal (SIGINT, catch_interrupt);
	catchsignal (SIGUSR1, catch_usr1);
	catchsignal (SIGUSR2, catch_usr2);
	catchsignal (SIGCONT, catch_cont);
	
	sigprocmask (SIG_SETMASK, oldsigset, NULL);
	if (param.outmode == DECODE_AUDIO) {
		if (audio_open(ai) < 0) {
			perror("audio");
			exit(1);
		}
	}

	for (;;) {
		if (intflag) {
			intflag = FALSE;
#if defined(SOLARIS) || defined(__NetBSD__)
			if (param.outmode == DECODE_AUDIO)
				audio_queueflush (ai);
#endif
			xf->readindex = xf->freeindex;
		}
		if (usr1flag) {
			usr1flag = FALSE;
			/*   close and re-open in order to flush
			 *   the device's internal buffer before
			 *   changing the sample rate.   [OF]
			 */
			xf->readindex = xf->freeindex;
			if (param.outmode == DECODE_AUDIO) {
				audio_close (ai);
				ai->rate = xf->buf[0]; 
				ai->channels = xf->buf[1]; 
				ai->format = xf->buf[2];
				if (audio_open(ai) < 0) {
					perror("audio");
					exit(1);
				}
			}
		}
		if ( (bytes = xfermem_get_usedspace(xf)) < outburst ) {
			/* if we got a buffer underrun we first
			 * fill 1/8 of the buffer before continue/start
			 * playing */
			preload = xf->size>>3;
			if(preload < outburst)
				preload = outburst;
		}
		if(bytes < preload) {
			int cmd;
			if (done) {
				break;
			}
			cmd = xfermem_block(XF_READER, xf);
			if (cmd == XF_CMD_WAKEUP_INFO)
				continue;
			if (cmd != XF_CMD_WAKEUP) {
				done = TRUE;
				continue;
			}
		}
		preload = outburst; /* set preload to lower mark */
		if (bytes > xf->size - xf->readindex)
			bytes = xf->size - xf->readindex;
		if (bytes > outburst)
			bytes = outburst;

		if (param.outmode == DECODE_STDOUT)
			bytes = write(1, xf->data + xf->readindex, bytes);
		else if (param.outmode == DECODE_AUDIO)
			bytes = audio_play_samples(ai,
				(unsigned char *) (xf->data + xf->readindex), bytes);

		if(bytes < 0) {
			bytes = 0;
			if(errno != EINTR) {
				perror("Ouch ... error while writing audio data: ");
				done = TRUE;
			}
		}

		xf->readindex = (xf->readindex + bytes) % xf->size;
		if (xf->wakeme[XF_WRITER])
			xfermem_putcmd(my_fd, XF_CMD_WAKEUP);
	}

	if (param.outmode == DECODE_AUDIO)
		audio_close (ai);
}

#endif

/* EOF */
