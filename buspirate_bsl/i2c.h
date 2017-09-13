// ==================================================================
// @(#)i2c.h
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

#ifndef __BUSPIRATE_I2C_H__
#define __BUSPIRATE_I2C_H__

#define BP_BIN_I2C_VERSION    0x01
#define BP_BIN_I2C_START_BIT  0x02
#define BP_BIN_I2C_STOP_BIT   0x03
#define BP_BIN_I2C_READ_BYTE  0x04
#define BP_BIN_I2C_ACK_BIT    0x06
#define BP_BIN_I2C_NACK_BIT   0x07
#define BP_BIN_I2C_BULK_WRITE 0x10
#define BP_BIN_I2C_SET_PERIPH 0x40
#define BP_BIN_I2C_SET_SPEED  0x60

#define BP_BIN_I2C_SPEED_5K   0x00
#define BP_BIN_I2C_SPEED_50K  0x01
#define BP_BIN_I2C_SPEED_100K 0x02
#define BP_BIN_I2C_SPEED_400K 0x03

#define BP_BIN_I2C_PERIPH_POWER   0x08
#define BP_BIN_I2C_PERIPH_PULLUPS 0x04
#define BP_BIN_I2C_PERIPH_AUX     0x02
#define BP_BIN_I2C_PERIPH_CS      0x01

#define BP_BIN_I2C_ACK  0x00
#define BP_BIN_I2C_NACK 0x01

#ifdef __cplusplus
extern "C" {
#endif

  int bp_bin_i2c_version(BP * bp, unsigned char * version);
  int bp_bin_i2c_set_speed(BP * bp, unsigned char speed);
  int bp_bin_i2c_set_periph(BP * bp, unsigned char config);
  int bp_bin_i2c_start(BP * bp);
  int bp_bin_i2c_stop(BP * bp);
  int bp_bin_i2c_ack(BP * bp);
  int bp_bin_i2c_nack(BP * bp);
  int bp_bin_i2c_write(BP * bp, unsigned char value, unsigned char * ack);
  int bp_bin_i2c_read(BP * bp, unsigned char * value);

#ifdef __cplusplus
}
#endif

#endif /* __BUSPIRATE_I2C_H__ */
