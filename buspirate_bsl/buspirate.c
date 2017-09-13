// ==================================================================
// @(#)buspirate.c
//
// @author Bruno Quoitin (bruno.quoitin@umons.ac.be)
// @date 18/09/2010
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <serial.h>

#include <buspirate.h>
//#define DEBUG
#define DEBUG_STREAM stderr
#define DEBUG_ID     "BP:"
#include <debug.h>

#define BUFFER_SIZE         1024
#define TIMEOUT_MS          10
#define DEFAULT_NUM_RETRIES 3
#define BIN_MODE_VERSION    1

struct BP_t {
  struct serial_driver_t * driver;
  unsigned char  * buffer;
  int              buffer_size;
  int              buffer_numc;
  int              buffer_pos;
  int              state;
  int              nretries;
  // Firmware version (high, low) -1 if unknown
  int              fw_vers_high;
  int              fw_vers_low;
  // Bootloader version (high, low) -1 if unknown
  int              bl_vers_high;
  int              bl_vers_low;
} BP_t;

static inline int _bp_flush(BP * bp);

// ------------------------------------------------------------------
/**
 * Utility function to convert a bus pirate version string into
 * integer.
 */ 
static inline const
char * _str2vp(const char * str, int *vp)
{
#define MAX_VP_DIGITS 3
  int ndigit= MAX_VP_DIGITS;
  *vp= 0;
  while (isdigit(*str)) {
    if (ndigit == 0)
      return NULL; // Max. 3 digits (fixed arbitrarily)
    *vp= (*vp * 10) + (*str - '0');
    str++;
    ndigit--;
  }
  if (ndigit == MAX_VP_DIGITS)
    return NULL; // No digit was consumed
  return str;
}

// ------------------------------------------------------------------
/**
 * Utility function to convert a bus pirate version string into
 * high/low integer parts
 */ 
static inline
int _str2v(const char * str, int * vh, int * vl)
{
  // Version string prefixed with 'v'
  if (*str != 'v')
    return -1;
  str++;
  // Parse high part
  str= _str2vp(str, vh);
  if (str == NULL)
    return -1;
  // High/low parts separated with dot
  if (*str != '.')
    return -1;
  str++;
  // Parse low part
  str= _str2vp(str, vl);
  if (str == NULL)
    return -1;
  // Ignore remaining of string
  return 0;
}

// ------------------------------------------------------------------
/**
 * Read a line of characters from the bus pirate. At most buf_size
 * characters are read. Buffer will not be nul-terminated.
 */
static int bp_readline(BP * bp, unsigned char * buf, size_t buf_size)
{
  __debug__("BP_READLINE\n");
  size_t i= 0;

  while (i < buf_size) {
    if (bp_readc(bp, buf) != BP_SUCCESS)
      return BP_FAILURE;

    if (*buf == '\n') {
      // End of line (carriage return)
      break;
    } else if (*buf == '\r') {
      // Ignore "return to beginning of line"
    } else {
      i++;
    }
    buf++;
    i++;
  }
  return BP_SUCCESS;
}

int bp_firmware_version_high(BP * bp)
{
  return bp->fw_vers_high;
}

int bp_firmware_version_low(BP * bp)
{
  return bp->fw_vers_low;
}

// ------------------------------------------------------------------
/**
 * Reset bus pirate into user terminal mode. Extracts version.
 *
 * This one was quite hard to get right !
 * Strategy is as follows. When we start a connection with the bus
 * pirate, it might actually be in many different states: user
 * terminal (text), raw binary mode, another binary mode. The worst
 * case is a multi-byte command in binary mode (e.g. SPI bulk
 * transfer). We need a way to come back to a well-known state.
 *
 * We first try to go to raw binary mode
 *  - as usual send up to 20 times 0x00 (BP_BIN_RESET)
 *    (this should work if we are in user terminal or in the middle
 *     of a multi-byte command - up to 16 bytes)
 *
 * Then, we go back to user terminal
 *  - send 0x0F (BP_BIN_TEXT)
 *
 * Finally, we issue a user terminal reset command
 *  - send "#\n"
 *
 * Then, we try to parse the reset strings to get the firmware
 * (and possibly bootloader versions).
 *
 * BTW, I'm not sure the above procedure will work when the BP is in
 * the middle of a user terminal command (still need to be checked).
 */
int bp_reset(BP * bp)
{
  __debug__("BP_RESET\n");
#define MAX_LINE_CHARS     80
#define MAX_LINES          10
#define STR_FIRMWARE       "Firmware"
#define STR_BOOTLOADER     "Bootloader"
#define STR_FIRMWARE_LEN   strlen(STR_FIRMWARE)
#define STR_BOOTLOADER_LEN strlen(STR_BOOTLOADER)

  unsigned char rbuf[MAX_LINE_CHARS+1];

  // Go back to raw binary mode (bitbang)
  if (bp_bin_init(bp, NULL) != BP_SUCCESS)
    return BP_FAILURE;

  usleep(1000000);

  if (_bp_flush(bp) != BP_SUCCESS)
    return BP_FAILURE;

  // Go back to user terminal mode
  rbuf[0]= BP_BIN_TEXT;
  if (bp_write(bp, rbuf, 1) != BP_SUCCESS)
    return BP_FAILURE;

  usleep(100000);

  if (_bp_flush(bp) != BP_SUCCESS)
    return BP_FAILURE;

  // Reset bus pirate to obtain version
  strcpy((char *) rbuf, "#\n");
  if (bp_write(bp, rbuf, strlen((char *) rbuf)) != BP_SUCCESS)
    return BP_FAILURE;


  usleep(100000);

  int j= 0;
  while (j < MAX_LINES) {

    if (bp_readline(bp, rbuf, MAX_LINE_CHARS) != BP_SUCCESS)
      return BP_FAILURE;

    if (!strncmp((char *) rbuf, STR_FIRMWARE, STR_FIRMWARE_LEN)) {
      char * c= strchr((char *) rbuf+STR_FIRMWARE_LEN+1, ' ');
      char * firmware= NULL, * bootloader= NULL;
      if (c == NULL) {
	firmware= strdup((char *) rbuf+STR_FIRMWARE_LEN+1);
      } else {
	*c= '\0';
	firmware= strdup((char *) rbuf+STR_FIRMWARE_LEN+1);
	c++;
	if (!strncmp(c, STR_BOOTLOADER, STR_BOOTLOADER_LEN)) {
	  bootloader= strdup(c+STR_BOOTLOADER_LEN+1);
	}
      }
      if (_str2v(firmware, &bp->fw_vers_high, &bp->fw_vers_low) < 0)
	return BP_FAILURE;
      printf("-->Firmware=[%d.%d]\n", bp->fw_vers_high, bp->fw_vers_low);
      if (bootloader != NULL) {
	if (_str2v(bootloader, &bp->bl_vers_high, &bp->bl_vers_low) < 0)
	  return BP_FAILURE;
	printf("-->Bootloader=[%d.%d]\n", bp->bl_vers_high, bp->bl_vers_low);
      }
      break;
    }

    j++;
  }

  // Flush input
  if (_bp_flush(bp) != BP_SUCCESS)
    return BP_FAILURE;

  bp->state= BP_STATE_TEXT;

  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Open communication with bus pirate.
 *
 * \retval NULL in case of failure
 * \retval a BP object in case of success
 */
BP * bp_open(const char * filename)
{
  struct serial_driver_t * driver;
  BP * bp= NULL;
  
  if ((driver= serial_open(filename, TIMEOUT_MS)) == NULL) {
    __debug__("driver::open failed");
    goto fail;
  }
  
  bp= malloc(sizeof(BP));
  assert(bp != NULL);

  bp->driver= driver;
  bp->buffer_numc= 0;
  bp->buffer_pos= 0;
  bp->nretries= DEFAULT_NUM_RETRIES;

  bp->buffer_size= BUFFER_SIZE;
  bp->buffer= malloc(BUFFER_SIZE);

  bp->fw_vers_high= -1;
  bp->fw_vers_low= -1;
  bp->bl_vers_high= -1;
  bp->bl_vers_low= -1;

  if (bp_reset(bp) != BP_SUCCESS)
    goto fail;

  return bp;

 fail:
  if (bp != NULL) {
    serial_close(bp->driver);
    free(bp);
  }
  return NULL;
}

// ------------------------------------------------------------------
/**
 * Close communication with bus pirate. Restore terminal parameters.
 * Free allocated resources.
 *
 * \retval BP_SUCCESS in case of success
 * \retval BP_FAILURE in case of failure
 */
int bp_close(BP * bp)
{
  assert(bp != NULL);
  
  assert(bp->driver != NULL);
  
  assert(bp->buffer != NULL);

  free(bp->buffer);
  
  serial_close(bp->driver);

  free(bp);
  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Check current state.
 */
void _bp_check_state(BP * bp, int state)
{
  assert(bp != NULL);
  assert(bp->state == state);
}

// ------------------------------------------------------------------
/**
 * Flush the device buffers and the reception buffer.
 */
static inline int _bp_flush(BP * bp)
{
  __debug__("BP_FLUSH\n");
  unsigned char c;
  
  /*
  if (tcflush(bp->fd, TCIOFLUSH) < 0)
    return BP_FAILURE;
  */

  // Eat as much as we can until a timeout occurs
  while (bp_readc(bp, &c) == BP_SUCCESS) {
    bp->buffer_numc= 0;
    bp->buffer_pos= 0;
  }
  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Read data from device if data is available and there is space in
 * the reception buffer.
 *
 * \retval
 *  \li BP_SUCCESS otherwise (even if no data was read)
 *  \li BP_FAILURE if an error occured
 */
static int _bp_read_avail(BP * bp)
{
#define BUFFER_SIZE 1024
  unsigned char buffer[BUFFER_SIZE];

  if (bp->buffer_numc >= 1)
    return BP_SUCCESS;

  bp->buffer_pos= 0;
  bp->buffer_numc= 0;

  int numc_to_read= bp->buffer_size - bp->buffer_numc;
  if (numc_to_read > BUFFER_SIZE)
    numc_to_read= BUFFER_SIZE;
  
  int error= serial_readc(bp->driver, buffer);
  if (error < 0) {
    __debug__("driver::read");
	return BP_FAILURE;
  }
  
  int numc_read= error;
  __debug__("  read n=%d\n", numc_read);
  if (numc_read == 0)
    return BP_SUCCESS;
  assert(numc_read != 0);
  __debug__("  read data=");
  int i;
  for (i= 0; i < numc_read; i++) {
    bp->buffer[bp->buffer_pos + bp->buffer_numc + i]= buffer[i];
    __debug_more__(" 0x%.2X", buffer[i]);
  }
  __debug_more__("\n");
  bp->buffer_numc+= numc_read;

  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Read a single byte from bus pirate. This is a non-blocking call.
 *
 * \retval
 *   \li BP_SUCCESS if a character was available
 *   \li BP_FAILURE if no character was available or if an error
 *       occurred
 */
int bp_readc(BP * bp, unsigned char * c)
{
  assert(bp != NULL);
  assert(bp->driver != NULL);

  __debug__("READC numc=%d, pos=%d\n", bp->buffer_numc, bp->buffer_pos);

  int nretries= bp->nretries;
  do {

    if (_bp_read_avail(bp) != BP_SUCCESS)
      return BP_FAILURE;
    
    if (bp->buffer_numc > 0) {
      *c= bp->buffer[bp->buffer_pos % bp->buffer_size];
      bp->buffer_pos= (bp->buffer_pos + 1) % bp->buffer_size;
      bp->buffer_numc-= 1;
      __debug__("  -> return value = %u (%c)\n", *c, (isalnum(*c)?*c:'.'));
      return BP_SUCCESS;
    }

    nretries--;
  } while (nretries > 0);

  return BP_FAILURE;
}

// ------------------------------------------------------------------
/**
 * Read N bytes from bus pirate.
 *
 * \retval
 *  \li BP_SUCCESS if 'nbyte' characters were read
 *  \li BP_FAILURE if less than 'nbyte' characters were read or if an
 *      error occurred.
 */
int bp_read(BP * bp, unsigned char * buf, size_t nbyte)
{
  __debug__("BP_READ(%zu)\n", nbyte);
  int i;
  for (i= 0; i < nbyte; i++) {
    if (bp_readc(bp, buf+i) != BP_SUCCESS)
      return BP_FAILURE;
  }
  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Write to bus pirate.
 *
 * \retval
 *  \li BP_SUCCESS if 'nbyte' characters were written
 *  \li BP_FAILURE if less then 'nbyte' characters were written or if
 *      an error occured
 */
int bp_write(BP * bp, const void * buf, size_t nbyte)
{
  int i;

  __debug__("BP_WRITE(");
  for (i= 0; i < nbyte; i++)
    __debug_more__(" 0x%.2X", ((unsigned char *) buf)[i]);
  __debug_more__(")\n");
  assert(bp != NULL);
  assert(bp->driver != NULL);

  int error= serial_write(bp->driver, (unsigned char *) buf, nbyte);
  if (error < 0) {
    __debug__("driver::write");
    return BP_FAILURE;
  }

  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Write a single command then read an answer.
 *
 * \retval
 *  \li < 0 if an error occurred (return value is error code)
 *  \li BP_SUCCESS if 'buflen' characters were read
 *  \li BP_FAILURE otherwise
 */
int _bp_bin_write_read(BP * bp, unsigned char cmd,
		       unsigned char * buf, size_t buflen)
{
  if (bp_write(bp, &cmd, 1) != BP_SUCCESS)
    return BP_FAILURE;
  return bp_read(bp, buf, buflen);
}

// ------------------------------------------------------------------
/**
 * Reset binary mode: all output in HiZ.
 */
int bp_bin_reset(BP * bp, unsigned char * version)
{
  __debug__("BP_BIN_RESET\n");
  unsigned char rbuf[5];
  assert(bp != NULL);

  if (_bp_bin_write_read(bp, BP_BIN_RESET, rbuf, 5) != BP_SUCCESS)
    return BP_FAILURE;
  if (memcmp(rbuf, "BBIO", 4) != 0)
    return BP_FAILURE;
  if ((rbuf[4] < '1') || (rbuf[4] > '9'))
    return BP_FAILURE;
  if (version != NULL)
    *version= rbuf[4]-'0';
  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Switch to binary mode.
 *
 * Currently checks that returned Binary I/O version is 1.
 */
int bp_bin_init(BP * bp, unsigned char * version)
{
  __debug__("BP_BIN_INIT\n");

  assert(bp != NULL);
  unsigned char vers= 0;
  int nretries= 20; /* Send >=20 times binary reset */

  if (_bp_flush(bp) != BP_SUCCESS)
    return BP_FAILURE;

  do {
    if (bp_write(bp, "\0", 1) != BP_SUCCESS)
      return BP_FAILURE;
    
    unsigned char buf[5];
    if (bp_read(bp, buf, sizeof(buf)) == BP_SUCCESS) {
      if (memcmp(buf, "BBIO", 4) != 0)
	return BP_FAILURE;
      if ((buf[4] < '1') || (buf[4] > '9'))
	return BP_FAILURE;
      vers= buf[4]-'0';
      
      if (vers != BIN_MODE_VERSION) {
	__debug__("unsupported version (%u)\n", vers);
	return BP_FAILURE;
      }
      
      break;
    }
    
    nretries--;
  } while (nretries > 0);
  
  _bp_flush(bp);

  if (version != NULL)
    *version= vers;
  bp->state= BP_STATE_BIN;
  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Setup I/O pins.
 *
 *   1=input
 *   0=output
 */
int bp_bin_pins_setup(BP * bp, int aux, int mosi, int clk, int miso, int cs,
		      unsigned char * value)
{
  __debug__("BP_BIN_PINS_SETUP\n");
  assert(bp != NULL);
  assert(bp->state == BP_STATE_BIN);

  unsigned char data= BP_BIN_PINS_SETUP;
  if (cs)   data|= BP_PIN_CS;
  if (miso) data|= BP_PIN_MISO;
  if (clk)  data|= BP_PIN_CLK;
  if (mosi) data|= BP_PIN_MOSI;
  if (aux)  data|= BP_PIN_AUX;

  if (bp_write(bp, &data, 1) != BP_SUCCESS)
    return BP_FAILURE;

  unsigned char buf;
  if (bp_readc(bp, &buf) != BP_SUCCESS)
    return BP_FAILURE;

  /* Check that 3 MSB of returned value correspond to 3 MSB of sent
     value. */
  assert((buf & 0xC0) == 0x40);

  if (value != NULL)
    *value= buf & 0x1F;

  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Setup I/O pins.
 *
 *   1=input
 *   2=output
 */
int bp_bin_pins_set(BP * bp, int power, int pullup, int aux, int mosi, int clk, int miso, int cs, unsigned char * value)
{
  __debug__("BP_BIN_PINS_SET\n");
  assert(bp != NULL);
  assert(bp->state == BP_STATE_BIN);

  unsigned char data= BP_BIN_PINS_SET;
  if (cs)     data|= BP_PIN_CS;
  if (miso)   data|= BP_PIN_MISO;
  if (clk)    data|= BP_PIN_CLK;
  if (mosi)   data|= BP_PIN_MOSI;
  if (aux)    data|= BP_PIN_AUX;
  if (pullup) data|= BP_PIN_PULLUP;
  if (power)  data|= BP_PIN_POWER;

  if (bp_write(bp, &data, 1) != BP_SUCCESS)
    return BP_FAILURE;

  unsigned char buf;
  if (bp_readc(bp, &buf) != BP_SUCCESS)
    return BP_FAILURE;

  /* Check that returned value corresponds for 'power' and 'pullup'.
     The value of other pins might differ if they were configured as
     input (see bp_bin_pins_setup). */
  assert((buf & 0xE0) == (data & 0xE0));

  if (value != NULL)
    *value= buf & 0x7F;

  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Read ADC voltage, in millivolts.
 */
int bp_bin_read_voltage(BP * bp, int * mV)
{
  unsigned char cmd = BP_BIN_VOLT_PROBE;
  unsigned char buf[2];

  if (bp_write(bp, &cmd, 1) != BP_SUCCESS)
    return BP_FAILURE;

  if (bp_read(bp, buf, 2) != BP_SUCCESS)
    return BP_FAILURE;
  
  *mV= (int) rint(((buf[0] << 8) + buf[1]) * 3.3 * 2);
  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Enter binary mode
 */
static int _bp_bin_mode(BP * bp, unsigned char mode,
			const char * response,
			unsigned char * version)
{
  assert(bp != NULL);
  assert(bp->state == BP_STATE_BIN);

  assert((mode >= 1) && (mode <= 5));
  unsigned char data= mode;

  if (bp_write(bp, &data, 1) != BP_SUCCESS)
    return BP_FAILURE;

  unsigned char rbuf[4];
  if (bp_read(bp, rbuf, 4) != BP_SUCCESS)
    return BP_FAILURE;
  if (memcmp(rbuf, response, 3) != 0)
    return BP_FAILURE;
  if ((rbuf[3] < '1') || (rbuf[3] > '9'))
    return BP_FAILURE;
  if (version != NULL)
    *version= rbuf[3]-'0';

  return BP_SUCCESS;
}

// ------------------------------------------------------------------
/**
 * Setup I2C mode.
 */
int  bp_bin_mode_i2c(BP * bp, unsigned char * version)
{
  __debug__("BP_BIN_MODE_I2C\n");
  int result= _bp_bin_mode(bp, BP_BIN_I2C, "I2C", version);
  if (result == BP_SUCCESS)
    bp->state= BP_STATE_BIN_I2C;
  return result;
}


// ------------------------------------------------------------------
/**
 * Setup SPI mode.
 */
int bp_bin_mode_spi(BP * bp, unsigned char * version)
{
  __debug__("BP_BIN_MODE_SPI\n");
  int result= _bp_bin_mode(bp, BP_BIN_SPI, "SPI", version);
  if (result == BP_SUCCESS)
    bp->state= BP_STATE_BIN_SPI;
  return result;
}

// ------------------------------------------------------------------
/**
 * Setup raw-wire mode.
 */
int bp_bin_mode_raw(BP * bp, unsigned char * version)
{
  __debug__("BP_BIN_MODE_RAW\n");
  int result= _bp_bin_mode(bp, BP_BIN_RAW_WIRE, "RAW", version);
  if (result == BP_SUCCESS)
    bp->state= BP_STATE_BIN_RAW;
  return result;
}
