// ==================================================================
// @(#)serial.c
//
// @author Bruno Quoitin (bruno.quoitin@umons.ac.be)
// @date 17/06/2014
// $Id$
//
// libbuspirate
// Copyright (C) 2010 Bruno Quoitin
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
// 02111-1307  USA
// ==================================================================

#include "serial.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG
#define DEBUG_STREAM stderr
#define DEBUG_ID 
#include "debug.h"

#ifdef _WIN32

#include <windows.h>

struct serial_driver_t {
  HANDLE handle;
};

struct serial_driver_t * serial_open(const char * port, long timeout)
{
  HANDLE handle=
    CreateFile(port, GENERIC_READ | GENERIC_WRITE,
            	0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (handle == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      fprintf(stderr, "Serial port %s not found.\n", port);
    } else {
      fprintf(stderr, "Could not open serial port %s.\n", port);
    }
    goto fail;
  }

  DCB params= { 0 };
  params.DCBlength= sizeof(params);
  if (!GetCommState(handle, &params)) {
    fprintf(stderr, "Could not get configuration of serial port\n");
    goto fail;
  }
  params.BaudRate= CBR_115200;
  params.ByteSize= 8;
  params.StopBits= ONESTOPBIT;
  params.Parity= NOPARITY;
  if (!SetCommState(handle, &params)) {
    fprintf(stderr, "Could not set configuration of serial port\n");
    goto fail;
  }

  COMMTIMEOUTS timeouts;
  timeouts.ReadIntervalTimeout = timeout;
  timeouts.ReadTotalTimeoutMultiplier = 10;
  timeouts.ReadTotalTimeoutConstant = timeout;
  timeouts.WriteTotalTimeoutMultiplier = 10;
  timeouts.WriteTotalTimeoutConstant = timeout;
  if (!SetCommTimeouts(handle, &timeouts)) {
    fprintf(stderr, "Could not set timeout of serial port\n");
    goto fail;
  }

  struct serial_driver_t * drv=
    (struct serial_driver_t *) malloc(sizeof(struct serial_driver_t));
  drv->handle= handle;
  return drv;

 fail:
  if (handle != INVALID_HANDLE_VALUE)
    CloseHandle(handle);
  return NULL;
} 

int serial_readc(struct serial_driver_t * drv, unsigned char * c)
{
  DWORD dwBytesRead= 0;
  if (!ReadFile(drv->handle, c, 1, &dwBytesRead, NULL)) {
     printf("Error when reading from serial port\n");
	 return -1;
  }
  //printf("read = %lu\n", dwBytesRead);
  return dwBytesRead;
}

int serial_writec(struct serial_driver_t * drv, unsigned char c)
{
  DWORD dwBytesWritten= 0;
  if (!WriteFile(drv->handle, &c, 1, &dwBytesWritten, NULL)) {
    printf("Error when writing to serial port\n");
    return -1;
  }
  //printf("written = %lu\n", dwBytesWritten);
  return dwBytesWritten;
}

int serial_write(struct serial_driver_t * drv, unsigned char * buf,
		 int nbytes)
{
  int i;
  for (i= 0; i < nbytes; i++)
    if (serial_writec(drv, *(buf++)) != 1)
      return -1;
  return nbytes;
}

void serial_close(struct serial_driver_t * drv)
{
  if (drv->handle != INVALID_HANDLE_VALUE)
    CloseHandle(drv->handle);
  drv->handle= INVALID_HANDLE_VALUE;
}

#else /* CYGWIN */

#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <unistd.h>

struct serial_driver_t {
  int            fd;
  struct termios saved_tios;
  long           timeout;
};

struct serial_driver_t * serial_open(const char * port, long timeout)
{
  struct termios tios, saved_tios;
  int fd;

  fd= open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    __debug_perror("open");
    goto fail;
  }

  if (tcgetattr(fd, &tios) < 0) {
    __debug_perror("tcgetattr");
    goto fail;
  }
  if (tcgetattr(fd, &saved_tios) < 0) {
    __debug_perror("tcgetattr");
    goto fail;
  }
  tios.c_cflag= CS8 | CLOCAL | CREAD;
  tios.c_iflag= IGNPAR | BRKINT;
  tios.c_oflag= 0;
  tios.c_lflag= 0;
  if (cfsetspeed(&tios, B115200) < 0) {
    __debug_perror("cfsetspeed");
    goto fail;
  }
  if (tcsetattr(fd, TCSANOW, &tios) < 0) {
    __debug_perror("tcsetattr");
    goto fail;
  }

  struct serial_driver_t * drv=
    (struct serial_driver_t *) malloc(sizeof(struct serial_driver_t));
  drv->fd= fd;
  drv->timeout= timeout;
  memcpy(&drv->saved_tios, &saved_tios, sizeof(saved_tios));
  return drv;

 fail:
  if (fd >= 0)
    close(fd);
  return NULL;
}

int serial_readc(struct serial_driver_t * drv, unsigned char * c)
{
  fd_set rset;
  struct timeval tv;

  __debug__("  select\n");
  FD_ZERO(&rset);
  FD_SET(drv->fd, &rset);
  tv.tv_sec= 0;
  tv.tv_usec= drv->timeout*1000;
  int error= select(drv->fd+1, &rset, NULL, NULL, &tv);
  if (error < 0) {
    __debug_perror("select");
    return 0;
  }

  __debug__("  select result=%d\n", error);
  
  __debug__("  FD_ISSET\n");
  if (!FD_ISSET(drv->fd, &rset)) {
    __debug__("  FD_ISSET:nothing to read\n");
    return 0;
  }

  /*error= read(drv->fd, buffer, numc_to_read);*/
  error= read(drv->fd, c, 1);
  if (error < 0) {
    __debug_perror("  read");
    return -1;
  }

  return error;
}

int serial_writec(struct serial_driver_t * drv, unsigned char c)
{
  int error= write(drv->fd, &c, 1);
  if (error < 0) {
    __debug_perror("write");
    return -1;
  }
  /*if (error != nbyte)
    return BP_FAILURE;*/

  /*tcdrain(drv->fd);
    __debug__("tcdrain finished\n");*/

  return 1;
}

int serial_write(struct serial_driver_t * drv, unsigned char * buf,
		 int nbytes)
{
  int error= write(drv->fd, buf, nbytes);
  if (error < 0) {
    __debug_perror("write");
    return -1;
  }
  if (error != nbytes) {
    __debug__("write error (%d bytes written, %d requested)\n", error, nbytes);
    return -1;
  }

  /*tcdrain(drv->fd);
    __debug__("tcdrain finished\n");*/
  
  return nbytes;
}


void serial_close(struct serial_driver_t * drv)
{
  assert(drv->fd >= 0);
  
  if (tcsetattr(drv->fd, TCSAFLUSH, &drv->saved_tios) < 0)
    __debug_perror("tcsetattr");

  if (close(drv->fd) < 0)
    __debug_perror("close");

  drv->fd= -1;
}

#endif
