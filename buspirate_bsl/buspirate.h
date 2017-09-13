// ==================================================================
// @(#)buspirate.h
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

#ifndef __BUSPIRATE_H__
#define __BUSPIRATE_H__

#include <stdlib.h>

typedef struct BP_t BP;

#define BP_BIN_RESET           0x00
#define BP_BIN_SPI             0x01
#define BP_BIN_I2C             0x02
#define BP_BIN_UART            0x03
#define BP_BIN_1WIRE           0x04
#define BP_BIN_RAW_WIRE        0x05
#define BP_BIN_TEXT            0x0F /* Return to user terminal */
#define BP_BIN_SHORT_SELF_TEST 0x10
#define BP_BIN_LONG_SELF_TEST  0x11
#define BP_BIN_PWM_SETUP       0x12
#define BP_BIN_PWM_CLEAR       0x13
#define BP_BIN_VOLT_PROBE      0x14
#define BP_BIN_PINS_SETUP      0x40 /* Set pins as input (1) or output (0),
                                       responds with read
                                       010xxxxx */
#define BP_BIN_PINS_SET        0x80 /* Set pins high (1) or low (0),
                                       responds with read
                                       1xxxxxxx */

#define BP_PIN_CS     0x01
#define BP_PIN_MISO   0x02
#define BP_PIN_CLK    0x04
#define BP_PIN_MOSI   0x08
#define BP_PIN_AUX    0x10
#define BP_PIN_PULLUP 0x20
#define BP_PIN_POWER  0x40

#define BP_STATE_TEXT    1
#define BP_STATE_BIN     10
#define BP_STATE_BIN_I2C 11
#define BP_STATE_BIN_SPI 12
#define BP_STATE_BIN_RAW 13

#define BP_SUCCESS 1
#define BP_FAILURE 0

extern volatile int loop;

#ifdef __cplusplus
extern "C" {
#endif

  BP * bp_open (const char * filename);
  int  bp_close(BP * bp);
  int  bp_reset(BP * bp);
  int  bp_readc(BP * bp, unsigned char * c);
  int  bp_read(BP * bp, unsigned char * buf, size_t nbyte);
  int  bp_write(BP * bp, const void * buf, size_t nbyte);

  int  bp_bin_init(BP * bp, unsigned char * version);
  int  bp_bin_reset(BP * bp, unsigned char * version);
  int  bp_bin_pins_setup(BP * bp, int aux, int mosi, int clk, int miso,
			 int cs, unsigned char * value);
  int  bp_bin_pins_set(BP * bp, int power, int pullup, int aux, int mosi,
		       int clk, int miso, int cs, unsigned char * value);
  int  bp_bin_read_voltage(BP * bp, int * mV);
  int  bp_bin_mode_i2c(BP * bp, unsigned char * version);
  int  bp_bin_mode_spi(BP * bp, unsigned char * version);
  int  bp_bin_mode_raw(BP * bp, unsigned char * version);

  void _bp_check_state(BP * bp, int state);
  int  _bp_bin_write_read(BP * bp, unsigned char cmd,
			  unsigned char * buf, size_t buflen);

  int bp_firmware_version_high(BP * bp);
  int bp_firmware_version_low(BP * bp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
