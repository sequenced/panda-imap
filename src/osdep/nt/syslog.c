/*
 * Program:	Syslog emulation routines for NT
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	8 February 1996
 * Last Edited:	8 February 1996
 *
 * Copyright 1996 by the University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appears in all copies and that both the
 * above copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the University of Washington not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  This software is made
 * available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


/* Local storage */

static char *loghdr;		/* log file header string */
static HANDLE loghdl = NIL;	/* handle of event source */

/* Emulator for BSD syslog() routine
 * Accepts: priority
 *	    message
 *	    parameters
 */

void syslog (int priority,const char *message,...)
{
  va_list args;
  LPTSTR strs[2];
  char tmp[MAILTMPLEN];		/* callers must be careful not to pop this */
  unsigned short etype;
				/* default event source */
  if (!loghdl) openlog ("c-client",LOG_PID,LOG_MAIL);
  switch (priority) {		/* translate UNIX type into NT type */
  case LOG_ALERT:
    etype = EVENTLOG_ERROR_TYPE;
    break;
  case LOG_INFO:
    etype = EVENTLOG_INFORMATION_TYPE;
    break;
  default:
    etype = EVENTLOG_WARNING_TYPE;
  }
  va_start (args,message);	/* initialize vararg mechanism */
  vsprintf (tmp,message,args);	/* build message */
  strs[0] = loghdr;		/* write header */
  strs[1] = tmp;		/* then the message */
				/* report the event */
  ReportEvent (loghdl,etype,(unsigned short) priority,2000,NIL,2,0,strs,NIL);
  va_end (args);
}


/* Emulator for BSD openlog() routine
 * Accepts: identity
 *	    options
 *	    facility
 */

void openlog (const char *ident,int logopt,int facility)
{
  char tmp[MAILTMPLEN];
  if (loghdl) fatal ("Duplicate openlog()!");
  loghdl = RegisterEventSource (NIL,ident);
  sprintf (tmp,(logopt & LOG_PID) ? "%s[%d]" : "%s",ident,getpid ());
  loghdr = cpystr (tmp);	/* save header for later */
  setmode (0,O_BINARY);		/* make the stdio mode be binary */
  setmode (1,O_BINARY);		/* make the stdio mode be binary */
  setmode (2,O_BINARY);		/* make the stdio mode be binary */
}
