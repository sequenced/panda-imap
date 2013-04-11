# Program:	MS client Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		11 May 1989
# Last Edited:	11 October 1993
#
# Copyright 1993 by the University of Washington
#
#  Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided
# that the above copyright notice appears in all copies and that both the
# above copyright notice and this permission notice appear in supporting
# documentation, and that the name of the University of Washington not be
# used in advertising or publicity pertaining to distribution of the software
# without specific, written prior permission.  This software is made
# available "as is", and
# THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
# WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
# NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
# INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
# (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


C = ../c-client
CCLIENTLIB = $C/c-client.a

CCMD = ../../../ccmd

# Get local CFLAGS and LFLAGS definitions from c-client directory

CFLAGS = -I$C -I$(CCMD) `cat $C/CFLAGS`
LDFLAGS = $(CCMD)/ccmd.a $(CCLIENTLIB) -ltermlib `cat $C/LDFLAGS`

ms: $(CCLIENTLIB) ms.o $(CCMD)/ccmd.a
	$(CC) $(CFLAGS) -o ms ms.o $(LDFLAGS)

ms.o: $C/mail.h $C/smtp.h $C/rfc822.h $C/misc.h $C/osdep.h

$(CCLIENTLIB):
	cd $C;make

clean:
	rm -f *.o ms

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'