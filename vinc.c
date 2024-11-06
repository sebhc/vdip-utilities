/********************************************************
** vinc.c
**
**  Version 4.0 (beta)
**
** This library provides an Application Programming
** Interface (API) to the FTDI Vinculum Firmware defined at:
**
** http://www.ftdichip.com/Firmware/Precompiled/UM_VinculumFirmware_V205.pdf
**
** With these routines you can write programs to transfer files
** to and from the H-8/H-89 system, equipped with the VDIP1 breakout
** board, using USB memory sticks or other compatible storage devices.
**
** The VDIP1 module contains the VNC1L chip from Future Technology
** Devices International (FTDI):
**
** http://www.ftdichip.com/Support/Documents/DataSheets/Modules/DS_VDIP1.pdf
**
** Access to the chip is through the VDIP1's parallel FIFO mode.
**
** The VDIP1 module handles all aspects of the USB software
** interface, which would otherwise be impossible or impractical
** to implement on simple 8-bit hardware like the H-8.
**
** Usage Notes:
**
** The typical calling sequence is as follows: vinit() is
** called first to ensure communication and put the device
** in a known state, then vfind_disk() is used to ensure that
** a storage device is attached.  Directory level can be
** changed with vcd() and vcdroot(). Files can then be opened
** with vropen() or vwopen(); I/O operations performed with
** vread(), vwrite(), and vseek(); and finally closed with
** vclose();
**
** This code is designed for use with the Software Toolworks C/80
** v. 3.1 compiler with the optional support for floats and longs.
** The compiler should be configured to produce a Microsoft relocatable 
** module (.REL) which can (optionally) be stored in a library
** (.LIB file) using the Microsoft LIB-80 Library Manager.
** The Microsoft LINK-80 loader program is then used to
** link this code, along with any other required modules,
** with the main (calling) program.
**
**  Glenn Roberts
**  31 January 2022
**
********************************************************/
#include "fprintf.h"
#include "vutil.h"
#include "vinc.h"


/********************************************************
**
** str_send
**
** Output a null-terminated string to the VDIP device.
** If you want a carriage return to be sent it must be
** in the string (\r).
**
** Return:
**    0 Success
**    -1  I/O error
**
********************************************************/
int str_send(s)
char *s;
{
  char *c;
  int rc;
  
#ifdef DEBUG
  printf("->str_send %s\n", s);
#endif
  rc = 0;

  for (c=s; ((*c!=0) && (rc!=-1)); c++)
    rc = out_vwait(*c, MAXWAIT);
  
  return rc;
}

/********************************************************
**
** str_rdw
**
** "Read with wait" - Used to read a string from the VDIP
** device but detect hung conditions by monitoring the time.
** Read and consume characters up to and including the character 
** specified in 'tchar', but wait no longer than the globally
** specified maximum time MAXWAIT for each byte. The 'tchar'
** character is replaced with a terminating NUL.
**
** Returns:
**    string length if read was successful
**    -1 if read timed out.
**
********************************************************/
int str_rdw(s, tchar)
char *s;
char tchar;
{
  int c, rc, slen;
  int timedout;
  
  timedout = FALSE;
  rc = 0;
	slen = 0;

  do {
    if((c = in_vwait(MAXWAIT)) == -1)
      timedout = TRUE;
    else {
      /* got a byte, check for end */
      if (c == tchar)
        c = NUL;
      *s++ = c;
			slen++;
    }
  } while ((c != 0) && (!timedout));
  if (timedout) {
    /* terminate the string and exit */
    *s = NUL;
    rc = -1;
  }
	else
		/* don't count the terminating NUL */
		rc = slen - 1;

  return rc;
}

/********************************************************
**
** in_v
**
** Read a character from the VDIP1 
**
** Returns:
**    Character read if successful
**    -1 if none available
**
** NOTE: Usage is deprecated - use in_vwait to avoid hung
** conditions!
**
********************************************************/
int in_v()
{
  int b;
  
  /* check for Data Ready */
  if ((inp(p_stat) & VRXF) != 0) {
    /* read the character from the port and return it */
    b = inp(p_data);
    return b;
  }
  else
    /* return -1 if no data available */
    return -1;
}

/********************************************************
**
** out_v
**
** Send a character to the VDIP1 via the specified port.
** Performs I/O handshaking with the VDIP1 device.
**
********************************************************/
int out_v(c)
char c;
{
  /* Wait for ok to transmit (VTXE high) */
  while ((inp(p_stat) & VTXE) == 0)
    ;
  /* OK to transmit the character */
  outp(p_data,c);
}


/********************************************************
**
** in_vwait
**
** "Input with Wait" - Input a character from the VDIP1 but 
** detect hung conditions by monitoring the time. Wait no 
** longer than t seconds.
**
** Returns:
**    Character read if successful
**    -1 if timed out
**
********************************************************/
int in_vwait(t)
int t;
{
  /* start short duration timer */
  timer(1, t);
  
  /* now wait for port activity or timeout */
  while (timer(0, 0)) {
    /* check for port activity and if so then read 
    ** the data, break out of the loop and return
    */
    if (inp(p_stat) & VRXF)
      return inp(p_data);
  }

  /* if we fall through it means the number of specified
  ** seconds passed without input activity.  To indicate
  ** this timeout error we return -1
  */
  return -1;
}

/********************************************************
**
** out_vwait
**
** Send a character to the VDIP1 via the specified port,
** but detect hung conditions by monitoring the time.
** Wait no more than t seconds.
**
** Returns:
**    0 if successful
**    -1  if timed out
**
********************************************************/
int out_vwait(c,t)
char c;
int t;
{
  /* start short duration timer */
  timer(1, t);
  
  /* now wait for port activity or timeout */
  while (timer(0, 0)) {
    /* check for ok to transmit (VTXE high) and,
    ** if so, then transmit and break out of the
    ** loop with successful return.
    */
    if (inp(p_stat) & VTXE) {
      outp(p_data,c);
      return 0;
    }
  }
  /* if we fall through it means the number of specified
  ** seconds passed without input activity.  To indicate
  ** this timeout error we return -1
  */
  return -1;
}


/********************************************************
**
** vfind_disk
**
** Determine if there is a flash drive available on the
** USB port. 
**
**
********************************************************/
int vfind_disk()
{
  /* If there is a drive available then a \r will cause 
  ** the prompt to come back, so simply send \r and 
  ** then test for a command prompt...
  */
  str_send("\r");
  
  return vprompt();
}

/********************************************************
**
** vpurge
**
** Read all the pending data from the VDIP device and 
** throw it away.  Wait for up to 1 second each time
** before deciding we're done. 
**
********************************************************/
int vpurge()
{
  int c;
  
  do {
    c = in_vwait(1);
  } while (c != -1);
}

/********************************************************
**
** vhandshake
**
** This routine checks to see if two-way communication with 
** the VDIP Command Monitor is working. It sends an ASCII 'E'
** and waits up to MAXWAIT for the 'E' to be echoed back.
**
** Returns:
**    0 Success
**    -1  Error, timed out or no response
**
********************************************************/
int vhandshake()
{
  int rc;
  
  /* Attempt to send an "E" */
  if (str_send("E\r") == -1)
    /* time out on send! */
    rc = -1;
  else if (str_rdw(linebuff, '\r') == -1)
    /* time out on reading response! */
    rc = -1;
  else if (strcmp(linebuff,"E") != 0)
    /* wrong response! */
    rc = -1;
  else
    /* success! */
    rc = 0;

  return rc;
}

/********************************************************
**
** vinit
**
** Initialize the VDIP connection.
**
** This routine tests for the presence of the device, does
** a synchronization to a known condition, ensures that
** the device is in ASCII I/O mode and (optionally) issues 
** any setup commands to initialize the desired settings.
**
** Returns:
**    0: Normal
**    -1: Error
**
********************************************************/
int vinit()
{
  int rc;

  rc = 0;
  
  /*first try to talk to the device */
  if (vsync() == -1)
    rc = -1;
  else {
    /* initialization commands */
    
    /* ASCII mode (more friendly) */
    rc = vipa();
    /* Close any open file */
    if (rc == 0)
      rc = vclf();
  }

  return rc;
}

/********************************************************
**
** vsync
**
** Flush the input buffer and attempt to handshake with
** the VDIP1 device.  This should put things in a known
** state.
**
** Returns:
**    0   successful synchronization
**    -1  can't sync with device
**
********************************************************/
int vsync()
{
  int i, rc;
  
  rc = -1;
  
  /* try up to 3 times to sync */
  for (i=0; i<3; i++) {
    /* first purge any waiting data */
    vpurge();

    /* now attempt two-way communication */
    if (vhandshake()==0) {
      /* we're talking! */
      rc = 0;
      break;
    }
  }
  
  return rc;
}

/********************************************************
**
** vdirf
**
** This is an interface to the Vinculum "DIR" command
** (Directory) for a specified file.
**
** Issue a "DIR" command for the specified file name. The
** USB drive will be searched for a matching file name and
** will return the file size, which is returned as a long
** in 'len'.
**
** Returns:
**    0: Normal
**    -1: Error (most likely means file not found)
**
********************************************************/
int vdirf(s, len)
char *s;
long *len;
{
  int rc;
  char *c;
  static union u_fil flen;

  rc = 0;
  
  str_send("dir ");
  str_send(s);
  str_send("\r");
  
  /* first line is always blank, just read it */
  str_rdw(linebuff, '\r');
  
  /* the result will either be the file name or
  ** "Command Failed". if the latter then return error.
  */
  str_rdw(linebuff, '\r');

  if (strcmp(linebuff, CFERROR) == 0) {
    /* flag an error! */
    rc = -1;
  }
  else {
    /* skip over file name (to first blank) */
    for (c=linebuff; ((*c!=' ') && (*c!=0)); c++)
      ;
    /* read file length as 4 hex values */
    gethexvals(c, 4, &flen.b[0]);

    /* return file size */
    *len = flen.l;
    
    /* success - gobble up the prompt */
    str_rdw(linebuff, '\r');
  }
  
  return rc;
}

/********************************************************
**
** vdird
**
** This is an interface to the Vinculum "DIRT" command
** (Directory) for a specified file.
**
** Issue a "DIRT" command for the specified file name. The
** USB drive will be searched for a matching file name and
** will return the last modified date and time in the
** specified locations, which are passed by reference.
**
** Returns:
**    0: Normal
**    -1: Error (most likely means file not found)
**
********************************************************/
int vdird(s, udate, utime)
char *s;
unsigned *udate, *utime;
{
  int i, rc;
  char *c;
  static union u_fil fdate;
  static char dates[10];
  
  rc = 0;
  
  str_send("dirt ");
  str_send(s);
  str_send("\r");
  
	/* Note: there is a difference in how the "DIRT" command
	** responds between the older 03.69 VDAP and the newer
	** VDAP2 (2.0.2-SP3) firmware. The older firmware first
	** responds with a blank line here, the newer one does not.
  ** So we need to handle either case by checking for an
	** empty string and then reading another if need be.
	*/
  if (str_rdw(linebuff, '\r') == 0)
		str_rdw(linebuff, '\r');
  
  /* result will either be the file name followed
  ** by 10 bytes, or "Command Failed".
  */

  if (strcmp(linebuff, CFERROR) == 0) {
    /* flag an error! */
    rc = -1;
  }
  else {
    /* skip over the file name (to first blank) */
    for (c=linebuff; ((*c!=' ') && (*c!=0)); c++)
      ;
    /* read all 3 date fields */
    gethexvals(c, 10, dates);

    /* last 4 bytes are the modification date */
    for (i=0; i<4; i++)
      fdate.b[i] = dates[i+6];

    /* return date and time */
    *utime = fdate.i[0];
    *udate = fdate.i[1];
    
    /* success - gobble up the prompt */
    str_rdw(linebuff, '\r');
  }
  return rc;
}


/********************************************************
**
** vprompt
**
** check for "D:\>" prompt. 
**
** Returns:
**    0 Normal
**    -1 on error (no prompt or timeout)
**
********************************************************/
int vprompt()
{
#ifdef DEBUG
  printf("->vprompt\n");
#endif

  /* check for normal prompt return (return if timeout) */
  if (str_rdw(linebuff, '\r') == -1)
    return -1;
  else if (strcmp(linebuff, PROMPT) != 0 )
    return -1;
  else
    return 0;
}

/********************************************************
**
** vropen
**
** This is an interface to the Vinculum "OPR" command
** (Open File For Read).
**
** Open a file for reading.
**
** Returns:
**    0 normal
**    -1 on error
**
********************************************************/
int vropen(s)
char *s;
{
  /* as a safety measure, close any open file */
  vclf();
  
  str_send("opr ");
  str_send(s);
  str_send("\r");
  return vprompt();
}

/********************************************************
**
** vwopen
**
** This is an interface to the Vinculum "OPW" command
** (Open File For Write).
**
** Open a file for writing, including a user-specified
** time/date stamp.
**
** Returns:
**    0 normal
**    -1 on error
**
********************************************************/
int vwopen(s)
char *s;
{
  /* as a safety measure, close any open file */
  vclf();
  
  str_send("opw ");
  str_send(s);
  str_send(td_string);
  str_send("\r");
  
  /* allow a little extra time if new file */
  return vprompt();
}

/********************************************************
**
** vseek
**
** This is an interface to the Vinculum "SEK" command
** (Seek).
**
** This routine seeks to an absolute offset position
** in an open file. 
**
********************************************************/
int vseek(p)
int p;
{
  static char fpos[7];
  
  str_send("sek ");
  str_send(itoa(p, fpos));
  str_send("\r");
  return vprompt();
}

/********************************************************
**
** vclose
**
** This is an interface to the Vinculum "CLF" command
** (Close File).
**
** This closes the specified file on the USB device.
** This should be called after a vropen() and vread()
** sequence, or a vwopen() and vwrite() sequence of calls.
**
********************************************************/
int vclose(s)
char *s;
{
  str_send("clf ");
  str_send(s);
  str_send("\r");
  return vprompt();
}

/********************************************************
**
** vclf
**
** This is an interface to the Vinculum "CLF" command
** (Close File).
**
** This closes the currently open file on the USB device.
** This should be called after a vropen() and vread()
** sequence, or a vwopen() and vwrite() sequence of calls.
**
********************************************************/
int vclf()
{
  str_send("clf\r");
  return vprompt();
}

/********************************************************
**
** vipa
**
** This is an interface to the Vinculum "IPA" command
** (Input In ASCII).
**
** This routine ensures that the monitor’s mode for 
** inputting and displaying values is printable ASCII
** characters. 
**
********************************************************/
int vipa()
{
  str_send("ipa\r");
  return vprompt();
}

/********************************************************
**
** vread
**
** This is an interface to the Vinculum "RDF" command
** (Read From File).
**
** This routine should only be called after a successful
** call to vropen(), which opens a file on the device
** for reading.  This routine reads n bytes from the file
** on the USB device, storing them in the provided buffer.
** Once all bytes have been written it then waits for the
** VDIP to reply with the D:\> prompt which is read and
** discarded.
**
** The bytes are read one at a time and a timeout value
** (MAXWAIT) determines how long to wait before assuming
** the command failed.
**
** Returns:
**    0 on Success
**    -1 on Error
**
********************************************************/
int vread(buff, n)
char *buff;
int n;
{
  int i;
  char *nxt;
  static char fsize[7];
  
#ifdef DEBUG
  printf("->vread\n");
#endif
  /* send read from file (RDF) command */
  str_send("rdf ");
  str_send(itoa(n, fsize));
  str_send("\r");
  
  /* immediately capture the result in the buffer */
  nxt=buff;
  for (i=0; i<n ; i++) {
    /* wait for RX flag, then read the byte */
    while ((inp(p_stat) & VRXF) == 0)
      ; /* wait... */
    *nxt++ = inp(p_data);
  }
#ifdef DEBUG
    printf("%d bytes read\n", n);
#endif

  return vprompt();
}

/********************************************************
**
** vwrite
**
** This is an interface to the Vinculum "WRF" command
** (Write To File).
**
** This routine should only be called after a successful
** call to vwopen(), which opens a file on the device
** for writing.  This routine writes n bytes from the
** provided buffer to the file on the USB device.  Once
** all bytes have been written it then waits for the
** VDIP to reply with the D:\> prompt which is read and
** discarded
**
** Returns:
**    0 on Success
**    -1 on Error
**
********************************************************/
int vwrite(buff, n)
char *buff;
int n;
{
  int i, rc;
  static char wsize[7];

  rc = 0;
  
  /* write to file (WRF) command */
  str_send("wrf ");
  str_send(itoa(n, wsize));
  str_send("\r");
  
  /* now output the n bytes to the device */
  for (i=0; i<n; i++) {
    out_v(*buff++);
  }

  return vprompt();
}

/********************************************************
**
** vcd
**
** Change to the specified directory, relative to the
** current location.  Levels can be changed only one
** at a time.
**
** Return:
**  0 = success
**  -1  = fail
**
********************************************************/
int vcd(dir)
char *dir;
{
  int rc;
  
  rc = 0;
  
  str_send("cd ");
  str_send(dir);
  str_send("\r");
  
  /* The result will either be the Prompt or an error
  ** message. If the latter then return error.
  */
  str_rdw(linebuff, '\r');

  if (strcmp(linebuff, PROMPT) != 0) {
    printf("CD %s: %s\n", dir, linebuff);
    rc = -1;
  }
  
  return rc;
}

/********************************************************
**
** vcdroot
**
** Change the directory to be root ('/').  This is done
** by issuing successive "CD .." commands until the command
** fails.
**
********************************************************/
int vcdroot()
{
  while(vcdup() == 0)
    ;
}

/********************************************************
**
** vcdup
**
** Change the directory up one level by issuing 
** the "CD .." command
**
** Returns:
**    0 if success
**    -1 if fail (i.e. at "root" level)
**
********************************************************/
int vcdup()
{
  int rc;
  
  rc = 0;
  
  str_send("cd ..\r");
  
  /* The result will either be the Prompt or
  ** "Command Failed". If the latter then return error.
  */
  str_rdw(linebuff, '\r');

  if (strcmp(linebuff, CFERROR) == 0) {
    /* flag an error! */
    rc = -1;
  }
  
  return rc;
}

/********************************************************
**
** vmkd
**
** Create the specified sub-directory in the current
** directory. Sets the time/date stamp based on current
** system time/date.
**
** Return:
**  0 = success
**  -1  = fail
**
********************************************************/
int vmkd(dir)
char *dir;
{
  int rc;
  
  rc = 0;
  
  /* first set up the file date for MKD command */
	settd(FALSE);
	
  str_send("mkd ");
  str_send(dir);
  str_send(td_string);
  str_send("\r");
  
  /* The result will either be the Prompt or an error
  ** message. If the latter then return error.
  */
  str_rdw(linebuff, '\r');

  if (strcmp(linebuff, PROMPT) != 0) {
    printf("MKD %s: %s\n", dir, linebuff);
    rc = -1;
  }
  
  return rc;
}
