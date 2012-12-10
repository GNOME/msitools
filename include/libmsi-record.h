/*
 * Copyright (C) 2002,2003 Mike McCormack
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef _LIBMSI_RECORD_H
#define _LIBMSI_RECORD_H

#include "libmsi-types.h"

LibmsiRecord *    libmsi_record_create (guint count);
LibmsiResult      libmsi_record_clear_data (LibmsiRecord *);
LibmsiResult      libmsi_record_set_int (LibmsiRecord *,unsigned,int);
LibmsiResult      libmsi_record_set_string (LibmsiRecord *,unsigned,const char *);
LibmsiResult      libmsi_record_get_string (const LibmsiRecord *,unsigned,char *,unsigned *);
unsigned          libmsi_record_get_field_count (const LibmsiRecord *);
int               libmsi_record_get_integer (const LibmsiRecord *,unsigned);
unsigned          libmsi_record_get_field_size (const LibmsiRecord *,unsigned);
gboolean          libmsi_record_is_null (const LibmsiRecord *,unsigned);

LibmsiResult      libmsi_record_load_stream (LibmsiRecord *,unsigned,const char *);
LibmsiResult      libmsi_record_save_stream (LibmsiRecord *,unsigned,char*,unsigned *);

#endif /* _LIBMSI_RECORD_H */
