// ==================================================================
// @(#)i2c.c
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
// Contributions
// - 21/05/2014, Guy Martin
//     fixes in bp_bin_i2c_write() where incorrect return codes where
//     used for bp_write() and bp_read()
// ==================================================================


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <buspirate.h>
//#define DEBUG
#define DEBUG_STREAM stderr
#define DEBUG_ID     "BP:I2C:"
#include <debug.h>
#include <i2c.h>

// ------------------------------------------------------------------
/**
 * I2C: read version.
 */
int bp_bin_i2c_version(BP * bp, unsigned char * version)
{
  __debug__("VERSION\n");
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char rbuf[4];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_VERSION, rbuf, 4) < 0)
    return -1;
  if (memcmp(rbuf, "I2C", 3) != 0)
    return -1;
  if ((rbuf[3] <= '0') || (rbuf[3] >= '9'))
    return -1;
  if (version != NULL)
    *version= rbuf[3]-'0';
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: set communication speed
 */
int bp_bin_i2c_set_speed(BP * bp, unsigned char speed)
{
  __debug__("SET_SPEED %u\n", speed);
  _bp_check_state(bp, BP_STATE_BIN_I2C);
  assert(speed <= 3);

  unsigned char  rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_SET_SPEED | speed, rbuf, 1) < 0)
    return -1;
  __debug__("SET SPEED answer=%u\n", rbuf[0]);
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: write peripheral configuration.
 *
 * config: wxyz
 *   w = power
 *   x = pullups
 *   y = AUX
 *   z = CS
 */
int bp_bin_i2c_set_periph(BP * bp, unsigned char config)
{
  __debug__("SET_PERIPH %u\n", config);
  _bp_check_state(bp, BP_STATE_BIN_I2C);
  assert(config <= 15);

  unsigned char rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_SET_PERIPH | config, rbuf, 1) < 0)
    return -1;
  if (rbuf[0] != 0x01)
    return -1;
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: start condition.
 */
int bp_bin_i2c_start(BP * bp)
{
  __debug__("START\n");
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_START_BIT, rbuf, 1) < 0)
    return -1;
  if (rbuf[0] != 0x01)
    return -1;
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: stop condition.
 */
int bp_bin_i2c_stop(BP * bp)
{
  __debug__("STOP\n");
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_STOP_BIT, rbuf, 1) < 0)
    return -1;
  if (rbuf[0] != 0x01)
    return -1;
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: ack condition.
 */
int bp_bin_i2c_ack(BP * bp)
{
  __debug__("ACK\n");
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_ACK_BIT, rbuf, 1) < 0)
    return -1;
  if (rbuf[0] != 0x01)
    return -1;
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: no-ack condition.
 */
int bp_bin_i2c_nack(BP * bp)
{
  __debug__("NACK\n");
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_NACK_BIT, rbuf, 1) < 0)
    return -1;
  if (rbuf[0] != 0x01)
    return -1;
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: write a single byte. The device ack condition is returned.
 */
int bp_bin_i2c_write(BP * bp, unsigned char value, unsigned char * ack)
{
  __debug__("WRITE 0x%X\n", value);
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char wbuf[2]= { BP_BIN_I2C_BULK_WRITE, value };
  if (bp_write(bp, wbuf, 2) != BP_SUCCESS)
    return -1;
  if (bp_read(bp, wbuf, 2) != BP_SUCCESS)
    return -1;
  if (wbuf[0] != 0x01)
    return -1;
  if ((wbuf[1] != 0x00) && (wbuf[1] != 0x01)) /* ACK = 0x00 ; NACK = 0x01 */   
    return -1;
  __debug__("  result = %s\n", (wbuf[1] == 0x00)?"ACK":"NACK");
  if (ack != NULL)
    *ack= wbuf[1];
  return 0;
}

// ------------------------------------------------------------------
/**
 * I2C: read a single byte.
 */
int bp_bin_i2c_read(BP * bp, unsigned char * value)
{
  __debug__("READ\n");
  _bp_check_state(bp, BP_STATE_BIN_I2C);

  unsigned char rbuf[1];
  if (_bp_bin_write_read(bp, BP_BIN_I2C_READ_BYTE, rbuf, 1) < 0)
    return -1;
  if (value != NULL)
    *value= rbuf[0];
  __debug__("  read 0x%X\n", rbuf[0]);
  return 0;
}
