/*
 * Program:	Network News Transfer Protocol (NNTP) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 February 1992
 * Last Edited:	20 September 1999
 *
 * Copyright 1999 by the University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notices appear in all copies and that both the
 * above copyright notices and this permission notice appear in supporting
 * documentation, and that the name of the University of Washington not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  This software is made
 * available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


#include <ctype.h>
#include <stdio.h>
#include "mail.h"
#include "osdep.h"
#include "nntp.h"
#include "rfc822.h"
#include "misc.h"
#include "newsrc.h"
#include "netmsg.h"
#include "flstring.h"
#include "utf8.h"

/* Driver dispatch used by MAIL */

DRIVER nntpdriver = {
  "nntp",			/* driver name */
#ifdef INADEQUATE_MEMORY
  DR_LOWMEM |
#endif
				/* driver flags */
  DR_NEWS|DR_READONLY|DR_NOFAST|DR_NAMESPACE|DR_CRLF|DR_RECYCLE,
  (DRIVER *) NIL,		/* next driver */
  nntp_valid,			/* mailbox is valid for us */
  nntp_parameters,		/* manipulate parameters */
  nntp_scan,			/* scan mailboxes */
  nntp_list,			/* find mailboxes */
  nntp_lsub,			/* find subscribed mailboxes */
  nntp_subscribe,		/* subscribe to mailbox */
  nntp_unsubscribe,		/* unsubscribe from mailbox */
  nntp_create,			/* create mailbox */
  nntp_delete,			/* delete mailbox */
  nntp_rename,			/* rename mailbox */
  nntp_status,			/* status of mailbox */
  nntp_mopen,			/* open mailbox */
  nntp_mclose,			/* close mailbox */
  nntp_fetchfast,		/* fetch message "fast" attributes */
  nntp_flags,			/* fetch message flags */
  nntp_overview,		/* fetch overview */
  NIL,				/* fetch message structure */
  nntp_header,			/* fetch message header */
  nntp_text,			/* fetch message text */
  NIL,				/* fetch message */
  NIL,				/* unique identifier */
  NIL,				/* message number from UID */
  NIL,				/* modify flags */
  nntp_flagmsg,			/* per-message modify flags */
  nntp_search,			/* search for message based on criteria */
  nntp_sort,			/* sort messages */
  nntp_thread,			/* thread messages */
  nntp_ping,			/* ping mailbox to see if still alive */
  nntp_check,			/* check for new messages */
  nntp_expunge,			/* expunge deleted messages */
  nntp_copy,			/* copy messages to another mailbox */
  nntp_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM nntpproto = {&nntpdriver};


				/* driver parameters */
static long nntp_maxlogintrials = MAXLOGINTRIALS;
static long nntp_port = 0;
static long nntp_altport = 0;
static char *nntp_altname = NIL;

/* NNTP validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *nntp_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return nntp_isvalid (name,tmp);
}


/* NNTP validate mailbox work routine
 * Accepts: mailbox name
 *	    buffer for returned mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *nntp_isvalid (char *name,char *mbx)
{
  DRIVER *ret = mail_valid_net (name,&nntpdriver,NIL,mbx);
  if (ret && (mbx[0] == '#')) {	/* namespace format name? */
    if ((mbx[1] == 'n') && (mbx[2] == 'e') && (mbx[3] == 'w') &&
	(mbx[4] == 's') && (mbx[5] == '.'))
      memmove (mbx,mbx+6,strlen (mbx+6) + 1);
    else ret = NIL;		/* bogus name */
  }
  return ret;
}

/* News manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *nntp_parameters (long function,void *value)
{
  switch ((int) function) {
  case SET_MAXLOGINTRIALS:
    nntp_maxlogintrials = (unsigned long) value;
    break;
  case GET_MAXLOGINTRIALS:
    value = (void *) nntp_maxlogintrials;
    break;
  case SET_NNTPPORT:
    nntp_port = (long) value;
    break;
  case GET_NNTPPORT:
    value = (void *) nntp_port;
    break;
  case SET_ALTNNTPPORT:
    nntp_altport = (long) value;
    break;
  case GET_ALTNNTPPORT:
    value = (void *) nntp_altport;
    break;
  case SET_ALTNNTPNAME:
    nntp_altname = (char *) value;
    break;
  case GET_ALTNNTPNAME:
    value = (void *) nntp_altname;
    break;
  default:
    value = NIL;		/* error case */
    break;
  }
  return value;
}

/* NNTP mail scan mailboxes for string
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void nntp_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  char tmp[MAILTMPLEN];
  if (nntp_canonicalize (ref,pat,tmp))
    mm_log ("Scan not valid for NNTP mailboxes",ERROR);
}


/* NNTP list newsgroups
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void nntp_list (MAILSTREAM *stream,char *ref,char *pat)
{
  MAILSTREAM *st = stream;
  char *s,*t,*lcl,pattern[MAILTMPLEN],name[MAILTMPLEN];
  int showuppers = pat[strlen (pat) - 1] == '%';
  if (!pat || !*pat) {
    if (nntp_canonicalize (ref,"*",pattern)) {
				/* tie off name at root */
      if ((s = strchr (pattern,'}')) && (s = strchr (s+1,'.'))) *++s = '\0';
      else pattern[0] = '\0';
      mm_list (stream,'.',pattern,NIL);
    }
  }
				/* ask server for open newsgroups */
  else if (nntp_canonicalize (ref,pat,pattern) &&
	   ((stream && LOCAL && LOCAL->nntpstream) ||
	    (stream = mail_open (NIL,pattern,OP_HALFOPEN))) &&
	   ((nntp_send (LOCAL->nntpstream,"LIST","ACTIVE") == NNTPGLIST) ||
	    (nntp_send (LOCAL->nntpstream,"LIST",NIL) == NNTPGLIST))) {
				/* namespace format name? */
    if (*(lcl = strchr (strcpy (name,pattern),'}') + 1) == '#') lcl += 6;
				/* process data until we see final dot */
    while (s = net_getline (LOCAL->nntpstream->netstream)) {
      if ((*s == '.') && !s[1]){/* end of text */
	fs_give ((void **) &s);
	break;
      }
      if (t = strchr (s,' ')) {	/* tie off after newsgroup name */
	*t = '\0';
	strcpy (lcl,s);		/* make full form of name */
				/* report if match */
	if (pmatch_full (name,pattern,'.')) mm_list (stream,'.',name,NIL);
	else while (showuppers && (t = strrchr (lcl,'.'))) {
	  *t = '\0';		/* tie off the name */
	  if (pmatch_full (name,pattern,'.'))
	    mm_list (stream,'.',name,LATT_NOSELECT);
	}
      }
      fs_give ((void **) &s);	/* clean up */
    }
    if (stream != st) mail_close (stream);
  }
}

/* NNTP list subscribed newsgroups
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void nntp_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  void *sdb = NIL;
  char *s,mbx[MAILTMPLEN];
				/* return data from newsrc */
  if (nntp_canonicalize (ref,pat,mbx)) newsrc_lsub (stream,mbx);
  if (*pat == '{') {		/* if remote pattern, must be NNTP */
    if (!nntp_valid (pat)) return;
    ref = NIL;			/* good NNTP pattern, punt reference */
  }
				/* if remote reference, must be valid NNTP */
  if (ref && (*ref == '{') && !nntp_valid (ref)) return;
				/* kludgy application of reference */
  if (ref && *ref) sprintf (mbx,"%s%s",ref,pat);
  else strcpy (mbx,pat);

  if (s = sm_read (&sdb)) do if (nntp_valid (s) && pmatch (s,mbx))
    mm_lsub (stream,NIL,s,NIL);
  while (s = sm_read (&sdb));	/* until no more subscriptions */
}


/* NNTP canonicalize newsgroup name
 * Accepts: reference
 *	    pattern
 *	    returned single pattern
 * Returns: T on success, NIL on failure
 */

long nntp_canonicalize (char *ref,char *pat,char *pattern)
{
  if (ref && *ref) {		/* have a reference */
    if (!nntp_valid (ref)) return NIL;
    strcpy (pattern,ref);	/* copy reference to pattern */
				/* # overrides mailbox field in reference */
    if (*pat == '#') strcpy (strchr (pattern,'}') + 1,pat);
				/* pattern starts, reference ends, with . */
    else if ((*pat == '.') && (pattern[strlen (pattern) - 1] == '.'))
      strcat (pattern,pat + 1);	/* append, omitting one of the period */
    else strcat (pattern,pat);	/* anything else is just appended */
  }
  else strcpy (pattern,pat);	/* just have basic name */
  return nntp_valid (pattern) ? T : NIL;
}

/* NNTP subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long nntp_subscribe (MAILSTREAM *stream,char *mailbox)
{
  char mbx[MAILTMPLEN];
  return nntp_isvalid (mailbox,mbx) ? newsrc_update (stream,mbx,':') : NIL;
}


/* NNTP unsubscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to delete from subscription list
 * Returns: T on success, NIL on failure
 */

long nntp_unsubscribe (MAILSTREAM *stream,char *mailbox)
{
  char mbx[MAILTMPLEN];
  return nntp_isvalid (mailbox,mbx) ? newsrc_update (stream,mbx,'!') : NIL;
}

/* NNTP create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long nntp_create (MAILSTREAM *stream,char *mailbox)
{
  return NIL;			/* never valid for NNTP */
}


/* NNTP delete mailbox
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long nntp_delete (MAILSTREAM *stream,char *mailbox)
{
  return NIL;			/* never valid for NNTP */
}


/* NNTP rename mailbox
 * Accepts: mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long nntp_rename (MAILSTREAM *stream,char *old,char *newname)
{
  return NIL;			/* never valid for NNTP */
}

/* NNTP status
 * Accepts: mail stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long nntp_status (MAILSTREAM *stream,char *mbx,long flags)
{
  MAILSTATUS status;
  NETMBX mb;
  unsigned long i;
  long ret = NIL;
  char *s,*name,*state,tmp[MAILTMPLEN];
  char *old = (stream && !stream->halfopen) ? LOCAL->name : NIL;
  MAILSTREAM *tstream = NIL;
  if (!(mail_valid_net_parse (mbx,&mb) && *mb.mailbox &&
	((mb.mailbox[0] != '#') ||
	 ((mb.mailbox[1] == 'n') && (mb.mailbox[2] == 'e') &&
	  (mb.mailbox[3] == 'w') && (mb.mailbox[4] == 's') &&
	  (mb.mailbox[5] == '.'))))) {
    sprintf (tmp,"Invalid NNTP name %s",mbx);
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* note mailbox name */
  name = (*mb.mailbox == '#') ? mb.mailbox+6 : mb.mailbox;
				/* stream to reuse? */
  if (!(stream && LOCAL->nntpstream &&
	mail_usable_network_stream (stream,mbx)) &&
      !(tstream = stream = mail_open (NIL,mbx,OP_HALFOPEN|OP_SILENT)))
    return NIL;			/* can't reuse or make a new one */

  if (nntp_send (LOCAL->nntpstream,"GROUP",name) == NNTPGOK) {
    status.flags = flags;	/* status validity flags */
    status.messages = strtoul (LOCAL->nntpstream->reply + 4,&s,10);
    i = strtoul (s,&s,10);
    status.uidnext = strtoul (s,NIL,10) + 1;
				/* initially empty */
    status.recent = status.unseen = 0;
				/* don't bother if empty or don't want this */
    if (status.messages && (flags & (SA_RECENT | SA_UNSEEN))) {
      if (state = newsrc_state (stream,name)) {
				/* in case have to do XHDR Date */
	sprintf (tmp,"%lu-%lu",i,status.uidnext - 1);
				/* only bother if there appear to be holes */
	if ((status.messages < (status.uidnext - i)) &&
	    ((nntp_send (LOCAL->nntpstream,"LISTGROUP",name) == NNTPGOK)
	     || (nntp_send (LOCAL->nntpstream,"XHDR Date",tmp) == NNTPHEAD))) {
	  while ((s = net_getline (LOCAL->nntpstream->netstream)) &&
		 strcmp (s,".")) {
	    newsrc_check_uid (state,strtoul (s,NIL,10),&status.recent,
			      &status.unseen);
	    fs_give ((void **) &s);
	  }
	  if (s) fs_give ((void **) &s);
	}
				/* assume c-client/NNTP map is entire range */
	else while (i < status.uidnext)
	  newsrc_check_uid (state,i++,&status.recent,&status.unseen);
	fs_give ((void **) &state);
      }
				/* no .newsrc state, all messages new */
      else status.recent = status.unseen = status.messages;
    }
				/* UID validity is a constant */
    status.uidvalidity = stream->uid_validity;
				/* pass status to main program */
    mm_status (stream,mbx,&status);
    ret = T;			/* succes */
  }
				/* flush temporary stream */
  if (tstream) mail_close (tstream);
				/* else reopen old newsgroup */
  else if (old && nntp_send (LOCAL->nntpstream,"GROUP",old) != NNTPGOK) {
    mm_log (LOCAL->nntpstream->reply,ERROR);
    stream->halfopen = T;	/* go halfopen */
  }
  return ret;			/* success */
}

/* NNTP open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *nntp_mopen (MAILSTREAM *stream)
{
  unsigned long i,j,k,nmsgs;
  char *s,*mbx,tmp[MAILTMPLEN];
  NETMBX mb;
  SENDSTREAM *nstream = NIL;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &nntpproto;
  if (!(mail_valid_net_parse (stream->mailbox,&mb) &&
	(stream->halfopen ||	/* insist upon valid name */
	 (*mb.mailbox && ((mb.mailbox[0] != '#') ||
			  ((mb.mailbox[1] == 'n') && (mb.mailbox[2] == 'e') &&
			   (mb.mailbox[3] == 'w') && (mb.mailbox[4] == 's') &&
			   (mb.mailbox[5] == '.'))))))) {
    sprintf (tmp,"Invalid NNTP name %s",stream->mailbox);
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* note mailbox anme */
  mbx = (*mb.mailbox == '#') ? mb.mailbox+6 : mb.mailbox;
  if (LOCAL) {			/* recycle stream */
    sprintf (tmp,"Reusing connection to %s",LOCAL->host);
    if (!stream->silent) mm_log (tmp,(long) NIL);
    nstream = LOCAL->nntpstream;
    LOCAL->nntpstream = NIL;	/* keep nntp_mclose() from punting it */
    nntp_mclose (stream,NIL);	/* do close action */
    stream->dtb = &nntpdriver;	/* reattach this driver */
  }
  if (mb.secflag) {		/* in case /secure switch given */
    mm_log ("Secure NNTP login not available",ERROR);
    return NIL;
  }
				/* in case /debug switch given */
  if (mb.dbgflag) stream->debug = T;
  mb.tryaltflag = stream->tryalt;
  if (!nstream) {		/* open NNTP now if not already open */
    char *hostlist[2];
    hostlist[0] = strcpy (tmp,mb.host);
    if (mb.port || nntp_port)
      sprintf (tmp + strlen (tmp),":%lu",mb.port ? mb.port : nntp_port);
    if (mb.altflag) sprintf (tmp + strlen (tmp),"/%s",(char *)
			     mail_parameters (NIL,GET_ALTDRIVERNAME,NIL));
    if (mb.user[0]) sprintf (tmp + strlen (tmp),"/user=\"%s\"",mb.user);
    hostlist[1] = NIL;
    nstream = nntp_open (hostlist,OP_READONLY+(stream->debug ? OP_DEBUG:NIL));
  }
  if (!nstream) return NIL;	/* didn't get an NNTP session */

				/* always zero messages if halfopen */
  if (stream->halfopen) i = j = k = nmsgs = 0;
				/* otherwise open the newsgroup */
  else if (nntp_send (nstream,"GROUP",mbx) == NNTPGOK) {
    k = strtoul (nstream->reply + 4,&s,10);
    i = strtoul (s,&s,10);
    stream->uid_last = j = strtoul (s,&s,10);
    nmsgs = (i | j) ? 1 + j - i : 0;
  }
  else {			/* no such newsgroup */
    mm_log (nstream->reply,ERROR);
    nntp_close (nstream);	/* punt stream */
    return NIL;
  }
				/* newsgroup open, instantiate local data */
  stream->local = memset (fs_get (sizeof (NNTPLOCAL)),0,sizeof (NNTPLOCAL));
  LOCAL->nntpstream = nstream;
  LOCAL->name = cpystr (mbx);	/* copy newsgroup name */
  if (mb.user[0]) LOCAL->user = cpystr (mb.user);
  stream->sequence++;		/* bump sequence number */
  stream->rdonly = stream->perm_deleted = T;
				/* UIDs are always valid */
  stream->uid_validity = 0xbeefface;
				/* copy host name */
  LOCAL->host = cpystr (net_host (nstream->netstream));
  sprintf (tmp,"{%s:%lu/nntp",LOCAL->host,net_port (nstream->netstream));
  if (mb.altflag) sprintf (tmp + strlen (tmp),"/%s",(char *)
			   mail_parameters (NIL,GET_ALTDRIVERNAME,NIL));
  if (mb.secflag) strcat (tmp,"/secure");
  if (LOCAL->user) sprintf (tmp + strlen (tmp),"/user=\"%s\"",LOCAL->user);
  if (stream->halfopen) strcat (tmp,"}<no_mailbox>");
  else sprintf (tmp + strlen (tmp),"}#news.%s",mbx);
  fs_give ((void **) &stream->mailbox);
  stream->mailbox = cpystr (tmp);

  if (nmsgs) {			/* if any messages exist */
    short silent = stream->silent;
    stream->silent = T;		/* don't notify main program yet */
    mail_exists (stream,nmsgs);	/* silently set the cache to the guesstimate */
    sprintf (tmp,"%lu-%lu",i,j);/* in case have to do XHDR Date */
    if ((k < nmsgs) &&	/* only bother if there appear to be holes */
	((nntp_send (nstream,"LISTGROUP",mbx) == NNTPGOK) ||
	 (nntp_send (nstream,"XHDR Date",tmp) == NNTPHEAD))) {
      nmsgs = 0;		/* have holes, calculate true count */
      while ((s = net_getline (nstream->netstream)) && strcmp (s,".")) {
	mail_elt (stream,++nmsgs)->private.uid = atol (s);
	fs_give ((void **) &s);
      }
      if (s) fs_give ((void **) &s);
    }
				/* assume c-client/NNTP map is entire range */
    else for (k = 1; k <= nmsgs; k++)
      mail_elt (stream,k)->private.uid = i++;
    stream->nmsgs = 0;		/* whack it back down */
    stream->silent = silent;	/* restore old silent setting */
    mail_exists (stream,nmsgs);	/* notify upper level that messages exist */
				/* read .newsrc entries */
    mail_recent (stream,newsrc_read (mbx,stream));
  }
  else {			/* empty newsgroup or halfopen */
    if (!(stream->silent || stream->halfopen)) {
      sprintf (tmp,"Newsgroup %s is empty",mbx);
      mm_log (tmp,WARN);
    }
    mail_exists (stream,(long) 0);
    mail_recent (stream,(long) 0);
  }
  return stream;		/* return stream to caller */
}

/* NNTP close
 * Accepts: MAIL stream
 *	    option flags
 */

void nntp_mclose (MAILSTREAM *stream,long options)
{
  unsigned long i;
  MESSAGECACHE *elt;
  if (LOCAL) {			/* only if a file is open */
    nntp_check (stream);	/* dump final checkpoint */
    if (LOCAL->name) fs_give ((void **) &LOCAL->name);
    if (LOCAL->host) fs_give ((void **) &LOCAL->host);
    if (LOCAL->user) fs_give ((void **) &LOCAL->user);
    if (LOCAL->txt) fclose (LOCAL->txt);
				/* close NNTP connection */
    if (LOCAL->nntpstream) nntp_close (LOCAL->nntpstream);
    for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->private.data)
	fs_give ((void **) &elt->private.data);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* NNTP fetch fast information
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 * This is ugly and slow
 */

void nntp_fetchfast (MAILSTREAM *stream,char *sequence,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
				/* get sequence */
  if (stream && LOCAL && ((flags & FT_UID) ?
			  mail_uid_sequence (stream,sequence) :
			  mail_sequence (stream,sequence)))
    for (i = 1; i <= stream->nmsgs; i++) {
      if ((elt = mail_elt (stream,i))->sequence &&
	  !(elt->day && !elt->rfc822_size)) {
	ENVELOPE **env = NIL;
	ENVELOPE *e = NIL;
	if (!stream->scache) env = &elt->private.msg.env;
	else if (stream->msgno == i) env = &stream->env;
	else env = &e;
	if (!*env || !elt->rfc822_size) {
	  STRING bs;
	  unsigned long hs;
	  char *ht = (*stream->dtb->header) (stream,i,&hs,NIL);
				/* need to make an envelope? */
	  if (!*env) rfc822_parse_msg (env,NIL,ht,hs,NIL,BADHOST,
				       stream->dtb->flags);
				/* need message size too, ugh */
	  if (!elt->rfc822_size) {
	    (*stream->dtb->text) (stream,i,&bs,FT_PEEK);
	    elt->rfc822_size = hs + SIZE (&bs) - GETPOS (&bs);
	  }
	}
				/* if need date, have date in envelope? */
	if (!elt->day && *env && (*env)->date)
	  mail_parse_date (elt,(*env)->date);
				/* sigh, fill in bogus default */
	if (!elt->day) elt->day = elt->month = 1;
	mail_free_envelope (&e);
      }
    }
}

/* NNTP fetch flags
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 */

void nntp_flags (MAILSTREAM *stream,char *sequence,long flags)
{
  unsigned long i;
  if ((flags & FT_UID) ?	/* validate all elts */
      mail_uid_sequence (stream,sequence) : mail_sequence (stream,sequence))
    for (i = 1; i <= stream->nmsgs; i++) mail_elt (stream,i)->valid = T;
}

/* NNTP fetch overview
 * Accepts: MAIL stream
 *	    UID sequence (may be NIL for nntp_search() call)
 *	    overview return function
 * Returns: T if successful, NIL otherwise
 */

long nntp_overview (MAILSTREAM *stream,char *sequence,overview_t ofn)
{
  unsigned long i,j,k,uid;
  char c,*s,*t,*v,tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  OVERVIEW ov;
				/* parse the sequence */
  if (!sequence || mail_uid_sequence (stream,sequence)) {
				/* scan sequence to load cache */
    for (i = 1; i <= stream->nmsgs; i++)
				/* have cached overview yet? */
      if ((elt = mail_elt (stream,i))->sequence && !elt->private.data) {
	for (j = i + 1;		/* no, find end of cache gap range */
	     (j <= stream->nmsgs) && (elt = mail_elt (stream,j))->sequence &&
	     !elt->private.data; j++);
				/* make NNTP range */
	sprintf (tmp,(i == (j - 1)) ? "%lu" : "%lu-%lu",mail_uid (stream,i),
		 mail_uid (stream,j - 1));
	i = j;			/* advance beyond gap */
				/* ask server for overview data to cache */
	if (nntp_send (LOCAL->nntpstream,"XOVER",tmp) == NNTPOVER) {
	  while ((s = net_getline (LOCAL->nntpstream->netstream)) &&
		 strcmp (s,".")) {
				/* death to embedded newlines */
	    for (t = v = s; c = *v++;)
	      if ((c != '\012') && (c != '\015')) *t++ = c;
	    *t++ = '\0';	/* tie off string in case it was shortened */
				/* cache the overview if found its sequence */
	    if ((uid = atol (s)) && (k = mail_msgno (stream,uid)) &&
		(t = strchr (s,'\t'))) {
	      if ((elt = mail_elt (stream,k))->private.data)
		fs_give ((void **) &elt->private.data);
	      elt->private.data = (unsigned long) cpystr (t + 1);
	    }
	    else {		/* shouldn't happen, snarl if it does */
	      sprintf (tmp,"Server returned data for unknown UID %lu",uid);
	      mm_log (tmp,WARN);
	    }
				/* flush the overview */
	    fs_give ((void **) &s);
	  }
				/* flush the terminating dot */
	  if (s) fs_give ((void **) &s);
	}
	else i = stream->nmsgs;	/* XOVER failed, punt cache load */
      }

				/* now scan sequence to return overviews */
    if (ofn) for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->sequence) {
				/* UID for this message */
	uid = mail_uid (stream,i);
				/* parse cached overview */
	if (nntp_parse_overview (&ov,s = (char *) elt->private.data))
	  (*ofn) (stream,uid,&ov);
	else {			/* parse failed */
	  (*ofn) (stream,uid,NIL);
	  if (s && *s) {	/* unusable cached entry? */
	    sprintf (tmp,"Unable to parse overview for UID %lu: %.500s",uid,s);
	    mm_log (tmp,WARN);
				/* erase it from the cache */
	    fs_give ((void **) &s);
	  }
				/* insert empty cached text as necessary */
	  if (!s) elt->private.data = (unsigned long) cpystr ("");
	}
				/* clean up overview data */
	if (ov.from) mail_free_address (&ov.from);
	if (ov.subject) fs_give ((void **) &ov.subject);
      }
  }
  return T;
}

/* Parse OVERVIEW struct from cached NNTP XOVER response
 * Accepts: struct to load
 *	    cached XOVER response
 * Returns: T if success, NIL if fail
 */

long nntp_parse_overview (OVERVIEW *ov,char *text)
{
  char *t;
				/* nothing in overview yet */
  memset ((void *) ov,0,sizeof (OVERVIEW));
				/* no cached data */
  if (!(text && *text)) return NIL;
  ov->subject = cpystr (text);	/* make hackable copy of overview */
				/* find end of Subject */
  if (t = strchr (ov->subject,'\t')) {
    *t++ = '\0';		/* tie off Subject, point to From */
				/* find end of From */
    if (ov->date = strchr (t,'\t')) {
      *ov->date++ = '\0';	/* tie off From, point to Date */
				/* parse From */
      rfc822_parse_adrlist (&ov->from,t,BADHOST);
				/* find end of Date */
      if (ov->message_id = strchr (ov->date,'\t')) {
				/* tie off Date, point to Message-ID */
	*ov->message_id++ = '\0';
				/* find end of Message-ID */
	if (ov->references = strchr (ov->message_id,'\t')) {
				/* tie off Message-ID, point to References */
	  *ov->references++ = '\0';
				/* fine end of References */
	  if (t = strchr (ov->references,'\t')) {
	    *t++ = '\0';	/* tie off References, point to octet size */
				/* parse size of message in octets */
	    ov->optional.octets = atol (t);
				/* find end of size */
	    if (t = strchr (t,'\t')) {
				/* parse size of message in lines */
	      ov->optional.lines = atol (++t);
				/* find Xref */
	      if (ov->optional.xref = strchr (t,'\t'))
		*ov->optional.xref++ = '\0';
	    }
	  }
	}
      }
    }
  }
  return ov->references ? T : NIL;
}

/* NNTP fetch header as text
 * Accepts: mail stream
 *	    message number
 *	    pointer to return size
 *	    flags
 * Returns: header text
 */

char *nntp_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *size,
		   long flags)
{
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  FILE *f;
  if ((flags & FT_UID) && !(msgno = mail_msgno (stream,msgno))) return "";
				/* have header text? */
  if (!(elt = mail_elt (stream,msgno))->private.msg.header.text.data) {
    sprintf (tmp,"%lu",mail_uid (stream,msgno));
				/* get header text */
    if ((nntp_send (LOCAL->nntpstream,"HEAD",tmp) == NNTPHEAD) &&
	(f = netmsg_slurp (LOCAL->nntpstream->netstream,size,NIL))) {
      fread (elt->private.msg.header.text.data =
	     (unsigned char *) fs_get ((size_t) *size + 3),
	     (size_t) 1,(size_t) *size,f);
      fclose (f);		/* flush temp file */
				/* tie off header with extra CRLF and NUL */
      elt->private.msg.header.text.data[*size] = '\015';
      elt->private.msg.header.text.data[++*size] = '\012';
      elt->private.msg.header.text.data[++*size] = '\0';
      elt->private.msg.header.text.size = *size;
      elt->valid = T;		/* make elt valid now */
    }
    else {			/* failed, mark as deleted and empty */
      elt->valid = elt->deleted = T;
      *size = elt->private.msg.header.text.size = 0;
    }
  }
				/* just return size of text */
  else *size = elt->private.msg.header.text.size;
  return elt->private.msg.header.text.data ?
    (char *) elt->private.msg.header.text.data : "";
}

/* NNTP fetch body
 * Accepts: mail stream
 *	    message number
 *	    pointer to stringstruct to initialize
 *	    flags
 * Returns: T if successful, else NIL
 */

long nntp_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  INIT (bs,mail_string,(void *) "",0);
  if ((flags & FT_UID) && !(msgno = mail_msgno (stream,msgno))) return NIL;
  elt = mail_elt (stream,msgno);
				/* different message, flush cache */
  if (LOCAL->txt && (LOCAL->msgno != msgno)) {
    fclose (LOCAL->txt);
    LOCAL->txt = NIL;
  }
  LOCAL->msgno = msgno;		/* note cached message */
  if (!LOCAL->txt) {		/* have file for this message? */
    sprintf (tmp,"%lu",elt->private.uid);
    if (nntp_send (LOCAL->nntpstream,"BODY",tmp) != NNTPBODY)
      elt->deleted = T;		/* failed mark as deleted */
    else LOCAL->txt = netmsg_slurp (LOCAL->nntpstream->netstream,
				    &LOCAL->txtsize,NIL);
  }
  if (!LOCAL->txt) return NIL;	/* error if don't have a file */
  if (!(flags & FT_PEEK)) {	/* mark seen if needed */
    elt->seen = T;
    mm_flags (stream,elt->msgno);
  }
  INIT (bs,file_string,(void *) LOCAL->txt,LOCAL->txtsize);
  return T;
}

/* NNTP per-message modify flag
 * Accepts: MAIL stream
 *	    message cache element
 */

void nntp_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt)
{
  if (!LOCAL->dirty) {		/* only bother checking if not dirty yet */
    if (elt->valid) {		/* if done, see if deleted changed */
      if (elt->sequence != elt->deleted) LOCAL->dirty = T;
      elt->sequence = T;	/* leave the sequence set */
    }
				/* note current setting of deleted flag */
    else elt->sequence = elt->deleted;
  }
}

/* NNTP search messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    option flags
 */

void nntp_search (MAILSTREAM *stream,char *charset,SEARCHPGM *pgm,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
  OVERVIEW ov;
  if (charset && *charset &&	/* convert if charset not US-ASCII or UTF-8 */
      !(((charset[0] == 'U') || (charset[0] == 'u')) &&
	((((charset[1] == 'S') || (charset[1] == 's')) &&
	  (charset[2] == '-') &&
	  ((charset[3] == 'A') || (charset[3] == 'a')) &&
	  ((charset[4] == 'S') || (charset[4] == 's')) &&
	  ((charset[5] == 'C') || (charset[5] == 'c')) &&
	  ((charset[6] == 'I') || (charset[6] == 'i')) &&
	  ((charset[7] == 'I') || (charset[7] == 'i')) && !charset[8]) ||
	 (((charset[1] == 'T') || (charset[1] == 't')) &&
	  ((charset[2] == 'F') || (charset[2] == 'f')) &&
	  (charset[3] == '-') && (charset[4] == '8') && !charset[5])))) {
    if (utf8_text (NIL,charset,NIL,T)) utf8_searchpgm (pgm,charset);
    else return;		/* charset unknown */
  }
  if (flags & SO_OVERVIEW) {	/* only if specified to use overview */
				/* identify messages that will be searched */
    for (i = 1; i <= stream->nmsgs; ++i)
      mail_elt (stream,i)->sequence = nntp_search_msg (stream,i,pgm,NIL);
				/* load the overview cache */
    nntp_overview (stream,NIL,NIL);
  }
				/* init in case no overview at cleanup */
  memset ((void *) &ov,0,sizeof (OVERVIEW));
				/* otherwise do default search */
  for (i = 1; i <= stream->nmsgs; ++i) {
    if (((flags & SO_OVERVIEW) && ((elt = mail_elt (stream,i))->sequence) &&
	 nntp_parse_overview (&ov,(char *) elt->private.data)) ?
	nntp_search_msg (stream,i,pgm,&ov) :
	mail_search_msg (stream,i,NIL,pgm)) {
      if (flags & SE_UID) mm_searched (stream,mail_uid (stream,i));
      else {			/* mark as searched, notify mail program */
	mail_elt (stream,i)->searched = T;
	if (!stream->silent) mm_searched (stream,i);
      }
    }
				/* clean up overview data */
    if (ov.from) mail_free_address (&ov.from);
    if (ov.subject) fs_give ((void **) &ov.subject);
  }
}

/* NNTP search message
 * Accepts: MAIL stream
 *	    message number
 *	    search program
 *	    overview to search (NIL means preliminary pass)
 * Returns: T if found, NIL otherwise
 */

long nntp_search_msg (MAILSTREAM *stream,unsigned long msgno,SEARCHPGM *pgm,
		      OVERVIEW *ov)
{
  unsigned short d;
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  SEARCHHEADER *hdr;
  SEARCHOR *or;
  SEARCHPGMLIST *not;
  if (pgm->msgno || pgm->uid) {	/* message set searches */
    SEARCHSET *set;
				/* message sequences */
    if (set = pgm->msgno) {	/* must be inside this sequence */
      while (set) {		/* run down until find matching range */
	if (set->last ? ((msgno < set->first) || (msgno > set->last)) :
	    msgno != set->first) set = set->next;
	else break;
      }
      if (!set) return NIL;	/* not found within sequence */
    }
    if (set = pgm->uid) {	/* must be inside this sequence */
      unsigned long uid = mail_uid (stream,msgno);
      while (set) {		/* run down until find matching range */
	if (set->last ? ((uid < set->first) || (uid > set->last)) :
	    uid != set->first) set = set->next;
	else break;
      }
      if (!set) return NIL;	/* not found within sequence */
    }
  }

  /* Fast data searches */
				/* message flags */
  if ((pgm->answered && !elt->answered) ||
      (pgm->unanswered && elt->answered) ||
      (pgm->deleted && !elt->deleted) ||
      (pgm->undeleted && elt->deleted) ||
      (pgm->draft && !elt->draft) ||
      (pgm->undraft && elt->draft) ||
      (pgm->flagged && !elt->flagged) ||
      (pgm->unflagged && elt->flagged) ||
      (pgm->recent && !elt->recent) ||
      (pgm->old && elt->recent) ||
      (pgm->seen && !elt->seen) ||
      (pgm->unseen && elt->seen)) return NIL;
				/* keywords */
  if ((pgm->keyword && !mail_search_keyword (stream,elt,pgm->keyword)) ||
      (pgm->unkeyword && mail_search_keyword (stream,elt,pgm->unkeyword)))
    return NIL;
  if (ov) {			/* only do this if real searching */
    MESSAGECACHE delt;
				/* size ranges */
    if ((pgm->larger && (ov->optional.octets <= pgm->larger)) ||
	(pgm->smaller && (ov->optional.octets >= pgm->smaller))) return NIL;
				/* date ranges */
    if ((pgm->sentbefore || pgm->senton || pgm->sentsince ||
	 (pgm->before || pgm->on || pgm->since)) &&
	(!mail_parse_date (&delt,ov->date) ||
	 !(d = (delt.year << 9) + (delt.month << 5) + delt.day) ||
	 (pgm->sentbefore && (d >= pgm->sentbefore)) ||
	 (pgm->senton && (d != pgm->senton)) ||
	 (pgm->sentsince && (d < pgm->sentsince)) ||
	 (pgm->before && (d >= pgm->before)) ||
	 (pgm->on && (d != pgm->on)) ||
	 (pgm->since && (d < pgm->since)))) return NIL;
    if ((pgm->from && !mail_search_addr (ov->from,pgm->from)) ||
	(pgm->subject && !mail_search_header_text (ov->subject,pgm->subject))||
	(pgm->message_id &&
	 !mail_search_header_text (ov->message_id,pgm->message_id)) ||
	(pgm->references &&
	 !mail_search_header_text (ov->references,pgm->references)))
      return NIL;


				/* envelope searches */
    if (pgm->bcc || pgm->cc || pgm->to || pgm->return_path || pgm->sender ||
	pgm->reply_to || pgm->in_reply_to || pgm->newsgroups ||
	pgm->followup_to) {
      ENVELOPE *env = mail_fetchenvelope (stream,msgno);
      if (!env) return NIL;	/* no envelope obtained */
				/* search headers */
      if ((pgm->bcc && !mail_search_addr (env->bcc,pgm->bcc)) ||
	  (pgm->cc && !mail_search_addr (env->cc,pgm->cc)) ||
	  (pgm->to && !mail_search_addr (env->to,pgm->to)))
	return NIL;
      /* These criteria are not supported by IMAP and have to be emulated */
      if ((pgm->return_path &&
	   !mail_search_addr (env->return_path,pgm->return_path)) ||
	  (pgm->sender && !mail_search_addr (env->sender,pgm->sender)) ||
	  (pgm->reply_to && !mail_search_addr (env->reply_to,pgm->reply_to)) ||
	  (pgm->in_reply_to &&
	   !mail_search_header_text (env->in_reply_to,pgm->in_reply_to)) ||
	  (pgm->newsgroups &&
	   !mail_search_header_text (env->newsgroups,pgm->newsgroups)) ||
	  (pgm->followup_to &&
	   !mail_search_header_text (env->followup_to,pgm->followup_to)))
	return NIL;
    }

				/* search header lines */
    for (hdr = pgm->header; hdr; hdr = hdr->next) {
      char *t,*e,*v;
      SIZEDTEXT s;
      STRINGLIST sth,stc;
      sth.next = stc.next = NIL;/* only one at a time */
      sth.text.data = hdr->line.data;
      sth.text.size = hdr->line.size;
				/* get the header text */
      if ((t = mail_fetch_header (stream,msgno,NIL,&sth,&s.size,
				  FT_INTERNAL | FT_PEEK)) && strchr (t,':')) {
	if (hdr->text.size) {	/* anything matches empty search string */
				/* non-empty, copy field data */
	  s.data = (unsigned char *) fs_get (s.size + 1);
				/* for each line */
	  for (v = (char *) s.data, e = t + s.size; t < e;) switch (*t) {
	  default:		/* non-continuation, skip leading field name */
	    while ((t < e) && (*t++ != ':'));
	    if ((t < e) && (*t == ':')) t++;
	  case '\t': case ' ':	/* copy field data  */
	    while ((t < e) && (*t != '\015') && (*t != '\012')) *v++ = *t++;
	    *v++ = '\n';	/* tie off line */
	    while (((*t == '\015') || (*t == '\012')) && (t < e)) t++;
	  }
				/* calculate true size */
	  s.size = v - (char *) s.data;
	  *v = '\0';		/* tie off results */
	  stc.text.data = hdr->text.data;
	  stc.text.size = hdr->text.size;
				/* search header */
	  if (mail_search_header (&s,&stc)) fs_give ((void **) &s.data);
	  else {		/* search failed */
	    fs_give ((void **) &s.data);
	    return NIL;
	  }
	}
      }
      else return NIL;		/* no matching header text */
    }
				/* search strings */
    if ((pgm->text &&
	 !mail_search_text (stream,msgno,NIL,pgm->text,LONGT))||
	(pgm->body && !mail_search_text (stream,msgno,NIL,pgm->body,NIL)))
      return NIL;
  }
				/* logical conditions */
  for (or = pgm->or; or; or = or->next)
    if (!(nntp_search_msg (stream,msgno,or->first,ov) ||
	  nntp_search_msg (stream,msgno,or->second,ov))) return NIL;
  for (not = pgm->not; not; not = not->next)
    if (nntp_search_msg (stream,msgno,not->pgm,ov)) return NIL;
  return T;
}

/* NNTP sort messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    sort program
 *	    option flags
 * Returns: vector of sorted message sequences or NIL if error
 */

unsigned long *nntp_sort (MAILSTREAM *stream,char *charset,SEARCHPGM *spg,
			  SORTPGM *pgm,long flags)
{
  unsigned long i,start,last;
  SORTCACHE **sc;
  mailcache_t mailcache = (mailcache_t) mail_parameters (NIL,GET_CACHE,NIL);
  unsigned long *ret = NIL;
  sortresults_t sr = (sortresults_t) mail_parameters (NIL,GET_SORTRESULTS,NIL);
  if (spg) {			/* only if a search needs to be done */
    int silent = stream->silent;
    stream->silent = T;		/* don't pass up mm_searched() events */
				/* search for messages */
    mail_search_full (stream,charset,spg,NIL);
    stream->silent = silent;	/* restore silence state */
  }
				/* initialize progress counters */
  pgm->nmsgs = pgm->progress.cached = 0;
				/* pass 1: count messages to sort */
  for (i = 1,start = last = 0; i <= stream->nmsgs; ++i)
    if (mail_elt (stream,i)->searched) {
      pgm->nmsgs++;
				/* have this in the sortcache already? */
      if (!((SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE))->date) {
				/* no, record as last message */
	last = mail_uid (stream,i);
				/* and as first too if needed */
	if (!start) start = last;
      }
    }
  if (pgm->nmsgs) {		/* pass 2: load sort cache */
    sc = nntp_sort_loadcache (stream,pgm,start,last,flags);
				/* pass 3: sort messages */
    if (!pgm->abort) ret = mail_sort_cache (stream,pgm,sc,flags);
    fs_give ((void **) &sc);	/* don't need sort vector any more */
  }
				/* empty sort results */
  else ret = (unsigned long *) memset (fs_get (sizeof (unsigned long)),0,
				       sizeof (unsigned long));
				/* also return via callback if requested */
  if (sr) (*sr) (stream,ret,pgm->nmsgs);
  return ret;
}

/* Mail load sortcache
 * Accepts: mail stream, already searched
 *	    sort program
 *	    first UID to XOVER
 *	    last UID to XOVER
 *	    option flags
 * Returns: vector of sortcache pointers matching search
 */

SORTCACHE **nntp_sort_loadcache (MAILSTREAM *stream,SORTPGM *pgm,
				 unsigned long start,unsigned long last,
				 long flags)
{
  unsigned long i;
  char c,*s,*t,*v,tmp[MAILTMPLEN];
  SORTPGM *pg;
  SORTCACHE **sc,*r;
  MESSAGECACHE telt;
  ADDRESS *adr = NIL;
  mailcache_t mailcache = (mailcache_t) mail_parameters (NIL,GET_CACHE,NIL);
				/* verify that the sortpgm is OK */
  for (pg = pgm; pg; pg = pg->next) switch (pg->function) {
  case SORTARRIVAL:		/* sort by arrival date */
  case SORTSIZE:		/* sort by message size */
  case SORTDATE:		/* sort by date */
  case SORTFROM:		/* sort by first from */
  case SORTSUBJECT:		/* sort by subject */
    break;
  case SORTTO:			/* sort by first to */
    mm_notify (stream,"[NNTPSORT] Can't do To-field sorting in NNTP",WARN);
    break;
  case SORTCC:			/* sort by first cc */
    mm_notify (stream,"[NNTPSORT] Can't do cc-field sorting in NNTP",WARN);
    break;
  default:
    fatal ("Unknown sort function");
  }

  if (start) {			/* messages need to be loaded in sortcache? */
				/* yes, build range */
    if (start != last) sprintf (tmp,"%lu-%lu",start,last);
    else sprintf (tmp,"%lu",start);
				/* get it from the NNTP server */
    if (!nntp_send (LOCAL->nntpstream,"XOVER",tmp) == NNTPOVER)
				/* ugh, have to get it the slow way */
      return mail_sort_loadcache (stream,pgm);
    while ((s = net_getline (LOCAL->nntpstream->netstream)) && strcmp (s,".")){
				/* death to embedded newlines */
      for (t = v = s; c = *v++;) if ((c != '\012') && (c != '\015')) *t++ = c;
      *t++ = '\0';		/* tie off resulting string */
				/* parse XOVER response */
      if ((i = mail_msgno (stream,atol (s))) &&
	  (t = strchr (s,'\t')) && (v = strchr (++t,'\t'))) {
	*v++ = '\0';		/* tie off subject */
				/* put stripped subject in sortcache */
	(r = (SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE))->subject =
	  mail_strip_subject (t);
	if (t = strchr (v,'\t')) {
	  *t++ = '\0';		/* tie off from */
	  if (adr = rfc822_parse_address (&adr,adr,&v,BADHOST,0)) {
	    r->from = adr->mailbox;
	    adr->mailbox = NIL;
	    mail_free_address (&adr);
	  }
	  if (v = strchr (t,'\t')) {
	    *v++ = '\0';	/* tie off date */
	    if (mail_parse_date (&telt,t)) r->date = mail_longdate (&telt);
	    if ((v = strchr (v,'\t')) && (v = strchr (++v,'\t')))
	      r->size = atol (++v);
	  }
	}
      }
      fs_give ((void **) &s);
    }
    if (s) fs_give ((void **) &s);
  }

				/* calculate size of sortcache index */
  i = pgm->nmsgs * sizeof (SORTCACHE *);
				/* instantiate the index */
  sc = (SORTCACHE **) memset (fs_get ((size_t) i),0,(size_t) i);
				/* see what needs to be loaded */
  for (i = 1; !pgm->abort && (i <= stream->nmsgs); i++)
    if ((mail_elt (stream,i))->searched) {
      sc[pgm->progress.cached++] =
	r = (SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE);
      r->pgm = pgm;	/* note sort program */
      r->num = (flags & SE_UID) ? mail_uid (stream,i) : i;
      if (!r->date) r->date = r->num;
      if (!r->arrival) r->arrival = mail_uid (stream,i);
      if (!r->size) r->size = 1;
      if (!r->from) r->from = cpystr ("");
      if (!r->to) r->to = cpystr ("");
      if (!r->cc) r->cc = cpystr ("");
      if (!r->subject) r->subject = cpystr ("");
    }
  return sc;
}


/* NNTP thread messages
 * Accepts: mail stream
 *	    thread type
 *	    character set
 *	    search program
 *	    option flags
 * Returns: thread node tree
 */

THREADNODE *nntp_thread (MAILSTREAM *stream,char *type,char *charset,
			 SEARCHPGM *spg,long flags)
{
  return mail_thread_msgs (stream,type,charset,spg,flags,nntp_sort);
}

/* NNTP ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long nntp_ping (MAILSTREAM *stream)
{
  return (nntp_send (LOCAL->nntpstream,"STAT",NIL) != NNTPSOFTFATAL);
}


/* NNTP check mailbox
 * Accepts: MAIL stream
 */

void nntp_check (MAILSTREAM *stream)
{
				/* never do if no updates */
  if (LOCAL->dirty) newsrc_write (LOCAL->name,stream);
  LOCAL->dirty = NIL;
}


/* NNTP expunge mailbox
 * Accepts: MAIL stream
 */

void nntp_expunge (MAILSTREAM *stream)
{
  if (!stream->silent) mm_log ("Expunge ignored on readonly mailbox",NIL);
}

/* NNTP copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    option flags
 * Returns: T if copy successful, else NIL
 */

long nntp_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (pc) return (*pc) (stream,sequence,mailbox,options);
  mm_log ("Copy not valid for NNTP",ERROR);
  return NIL;
}


/* NNTP append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    stringstruct of messages to append
 * Returns: T if append successful, else NIL
 */

long nntp_append (MAILSTREAM *stream,char *mailbox,char *flags,char *date,
		  STRING *message)
{
  mm_log ("Append not valid for NNTP",ERROR);
  return NIL;
}

/* NNTP open connection
 * Accepts: network driver
 *	    service host list
 *	    port number
 *	    service name
 *	    NNTP open options
 * Returns: SEND stream on success, NIL on failure
 */

SENDSTREAM *nntp_open_full (NETDRIVER *dv,char **hostlist,char *service,
			    unsigned long port,long options)
{
  SENDSTREAM *stream = NIL;
  NETSTREAM *netstream;
  NETMBX mb;
  char tmp[MAILTMPLEN];
  long reply;
  if (!(hostlist && *hostlist)) mm_log ("Missing NNTP service host",ERROR);
  else do {			/* try to open connection */
    sprintf (tmp,"{%.1000s/%.20s}",*hostlist,service ? service : "nntp");
    if (!mail_valid_net_parse (tmp,&mb) || mb.anoflag || mb.secflag) {
      sprintf (tmp,"Invalid host specifier: %.80s",*hostlist);
      mm_log (tmp,ERROR);
    }
    else {
				/* light tryalt flag if requested */
      mb.tryaltflag = (options & OP_TRYALT) ? T : NIL;
      if (netstream =		/* try to open ordinary connection */
	  net_open (&mb,dv,nntp_port ? nntp_port : port,
		    (NETDRIVER *) mail_parameters (NIL,GET_ALTDRIVER,NIL),
		    (char *) mail_parameters (NIL,GET_ALTNNTPNAME,NIL),
		    (unsigned long)mail_parameters(NIL,GET_ALTNNTPPORT,NIL))) {
	stream = (SENDSTREAM *) fs_get (sizeof (SENDSTREAM));
				/* initialize stream */
	memset ((void *) stream,0,sizeof (SENDSTREAM));
	stream->netstream = netstream;
	stream->debug = (mb.dbgflag || (options & OP_DEBUG)) ? T : NIL;
				/* get server greeting */
	switch ((int) (reply = nntp_reply (stream))) {
	case NNTPGREET:		/* allow posting */
	  NNTP.post = T;
	  mm_notify (NIL,stream->reply + 4,(long) NIL);
	  break;
	case NNTPGREETNOPOST:	/* posting not allowed, must be readonly */
	  if (options & OP_READONLY) {
	    mm_notify (NIL,stream->reply + 4,(long) NIL);
	    break;
	  }
				/* falls through */
	default:		/* anything else is an error */
	  mm_log (stream->reply,ERROR);
	  stream = nntp_close (stream);
	}
      }
    }
  } while (!stream && *++hostlist);

				/* have a session, log in if have user name */
  if (mb.user[0] && !nntp_send_auth_work (stream,&mb,tmp)) {
    nntp_close (stream);	/* punt stream */
    return NIL;
  }
				/* in case server demands MODE READER */
  if (stream) switch ((int) nntp_send_work (stream,"MODE","READER")) {
  case NNTPWANTAUTH:		/* server wants auth first, do so and retry */
  case NNTPWANTAUTH2:
    if (nntp_send_auth_work(stream,&mb,tmp)) nntp_send(stream,"MODE","READER");
    else stream = nntp_close (stream);
    break;
  default:			/* only authenticate if requested */
    if (mb.user[0] && !nntp_send_auth_work (stream,&mb,tmp))
      stream = nntp_close (stream);
    break;
  }
  return stream;
}

/* NNTP close connection
 * Accepts: SEND stream
 * Returns: NIL always
 */

SENDSTREAM *nntp_close (SENDSTREAM *stream)
{
  if (stream) {			/* send "QUIT" */
    if (stream->netstream) {	/* only if a living stream */
      nntp_send (stream,"QUIT",NIL);
				/* close NET connection */
      net_close (stream->netstream);
    }
    if (stream->reply) fs_give ((void **) &stream->reply);
    fs_give ((void **) &stream);/* flush the stream */
  }
  return NIL;
}

/* NNTP deliver news
 * Accepts: SEND stream
 *	    message envelope
 *	    message body
 * Returns: T on success, NIL on failure
 */

long nntp_mail (SENDSTREAM *stream,ENVELOPE *env,BODY *body)
{
  long ret;
  char *s,path[MAILTMPLEN],tmp[8*MAILTMPLEN];
  /* Gabba gabba hey, we need some brain damage to send netnews!!!
   *
   * First, we give ourselves a frontal lobotomy, and put in some UUCP
   *  syntax.  It doesn't matter that it's completely bogus UUCP, and
   *  that UUCP has nothing to do with anything we're doing.  It's been
   *  alleged that "Path: not-for-mail" is all that's needed, but without
   *  proof, it's probably not safe to make assumptions.
   *
   * Second, we bop ourselves on the head with a ball-peen hammer.  How
   *  dare we be so presumptious as to insert a *comment* in a Date:
   *  header line.  Why, we were actually trying to be nice to a human
   *  by giving a symbolic timezone (such as PST) in addition to a
   *  numeric timezone (such as -0800).  But the gods of news transport
   *  will have none of this.  Unix weenies, tried and true, rule!!!
   *
   * Third, Netscape Collabra server doesn't give the NNTPWANTAUTH error
   *  until after requesting and receiving the entire message.  So we can't
   *  call rely upon nntp_send()'s to do the auth retry.
   */
				/* RFC-1036 requires this cretinism */
  sprintf (path,"Path: %s!%s\015\012",net_localhost (stream->netstream),
	   env->sender ? env->sender->mailbox :
	   (env->from ? env->from->mailbox : "not-for-mail"));
				/* here's another cretinism */
  if (s = strstr (env->date," (")) *s = NIL;
  do if ((ret = nntp_send_work (stream,"POST",NIL)) == NNTPREADY)
				/* output data, return success status */
    ret = (net_soutr (stream->netstream,path) &&
	   rfc822_output (tmp,env,body,nntp_soutr,stream->netstream,T)) ?
	     nntp_send_work (stream,".",NIL) :
	       nntp_fake (stream,NNTPSOFTFATAL,
			  "NNTP connection broken (message text)");
  while (((ret == NNTPWANTAUTH) || (ret == NNTPWANTAUTH2)) &&
	 nntp_send_auth (stream));
  if (s) *s = ' ';		/* put the comment in the date back */
  return ret;
}

/* NNTP send command
 * Accepts: SEND stream
 *	    text
 * Returns: reply code
 */

long nntp_send (SENDSTREAM *stream,char *command,char *args)
{
  long ret;
  switch ((int) (ret = nntp_send_work (stream,command,args))) {
  case NNTPWANTAUTH:		/* authenticate and retry */
  case NNTPWANTAUTH2:
    if (nntp_send_auth (stream)) ret = nntp_send_work (stream,command,args);
    else {			/* we're probably hosed, nuke the session */
      nntp_send (stream,"QUIT",NIL);
				/* close net connection */
      net_close (stream->netstream);
      stream->netstream = NIL;
    }
  default:			/* all others just return */
    break;
  }
  return ret;
}


/* NNTP send command worker routine
 * Accepts: SEND stream
 *	    text
 * Returns: reply code
 */

long nntp_send_work (SENDSTREAM *stream,char *command,char *args)
{
  char tmp[MAILTMPLEN];
				/* build the complete command */
  if (args) sprintf (tmp,"%s %s",command,args);
  else strcpy (tmp,command);
  if (stream->debug) mm_dlog (tmp);
  strcat (tmp,"\015\012");
				/* send the command */
  return net_soutr (stream->netstream,tmp) ? nntp_reply (stream) :
    nntp_fake (stream,NNTPSOFTFATAL,"NNTP connection broken (command)");
}

/* NNTP send authentication if needed
 * Accepts: SEND stream
 * Returns: T if need to redo command, NIL otherwise
 */

long nntp_send_auth (SENDSTREAM *stream)
{
  NETMBX mb;
  char tmp[MAILTMPLEN];
  sprintf (tmp,"{%s/nntp}<none>",net_host (stream->netstream));
  mail_valid_net_parse (tmp,&mb);
  return nntp_send_auth_work (stream,&mb,tmp);
}


/* NNTP send authentication worker routine
 * Accepts: SEND stream
 *	    NETMBX structure
 *	    temporary buffer
 * Returns: T if authenticated, NIL otherwise
 */

long nntp_send_auth_work (SENDSTREAM *stream,NETMBX *mb,char *tmp)
{
  long i;
  long trial = 0;
  do {				/* get user name and password */
    mm_login (mb,mb->user,tmp,trial++);
    if (!*tmp) {		/* abort if he refused to give password */
      mm_log ("Login aborted",ERROR);
      break;
    }
				/* do the authentication */
    if ((i = nntp_send_work (stream,"AUTHINFO USER",mb->user)) == NNTPWANTPASS)
      i = nntp_send_work (stream,"AUTHINFO PASS",tmp);
				/* successful authentication */
    if (i == NNTPAUTHED) return T;
    mm_log (stream->reply,WARN);
  }
  while ((i != NNTPSOFTFATAL) && (trial < nntp_maxlogintrials));
  mm_log ("Too many NNTP authentication failures",ERROR);
  return NIL;			/* authentication failed */
}

/* NNTP get reply
 * Accepts: SEND stream
 * Returns: reply code
 */

long nntp_reply (SENDSTREAM *stream)
{
				/* flush old reply */
  if (stream->reply) fs_give ((void **) &stream->reply);
  				/* get reply */
  if (!(stream->reply = net_getline (stream->netstream)))
    return nntp_fake(stream,NNTPSOFTFATAL,"NNTP connection broken (response)");
  if (stream->debug) mm_dlog (stream->reply);
				/* handle continuation by recursion */
  if (stream->reply[3] == '-') return nntp_reply (stream);
				/* return response code */
  return stream->replycode = atol (stream->reply);
}


/* NNTP set fake error
 * Accepts: SEND stream
 *	    NNTP error code
 *	    error text
 * Returns: error code
 */

long nntp_fake (SENDSTREAM *stream,long code,char *text)
{
				/* flush any old reply */
  if (stream->reply) fs_give ((void **) &stream->reply);
  				/* set up pseudo-reply string */
  stream->reply = (char *) fs_get (20+strlen (text));
  sprintf (stream->reply,"%ld %s",code,text);
  return code;			/* return error code */
}

/* NNTP filter mail
 * Accepts: stream
 *	    string
 * Returns: T on success, NIL on failure
 */

long nntp_soutr (void *stream,char *s)
{
  char c,*t;
				/* "." on first line */
  if (s[0] == '.') net_soutr (stream,".");
				/* find lines beginning with a "." */
  while (t = strstr (s,"\015\012.")) {
    c = *(t += 3);		/* remember next character after "." */
    *t = '\0';			/* tie off string */
				/* output prefix */
    if (!net_soutr (stream,s)) return NIL;
    *t = c;			/* restore delimiter */
    s = t - 1;			/* push pointer up to the "." */
  }
				/* output remainder of text */
  return *s ? net_soutr (stream,s) : T;
}