// ==================================================================
// @(#)serial.h
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

#ifndef __SERIAL_H__
#define __SERIAL_H__

struct serial_driver_t;

struct serial_driver_t * serial_open(const char * port, long timeout);
int serial_readc(struct serial_driver_t * d, unsigned char * c);
int serial_writec(struct serial_driver_t * d, unsigned char c);
int serial_write(struct serial_driver_t * d, unsigned char * buf, int nbytes);
void serial_close(struct serial_driver_t * d);
//const char * get_last_error();

#endif /* __SERIAL_H__ */
