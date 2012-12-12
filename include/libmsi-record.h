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

#include <glib-object.h>

#include "libmsi-types.h"

G_BEGIN_DECLS

#define LIBMSI_TYPE_RECORD             (libmsi_record_get_type ())
#define LIBMSI_RECORD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIBMSI_TYPE_RECORD, LibmsiRecord))
#define LIBMSI_RECORD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LIBMSI_TYPE_RECORD, LibmsiRecordClass))
#define LIBMSI_IS_RECORD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIBMSI_TYPE_RECORD))
#define LIBMSI_IS_RECORD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LIBMSI_TYPE_RECORD))
#define LIBMSI_RECORD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LIBMSI_TYPE_RECORD, LibmsiRecordClass))

typedef struct _LibmsiRecordClass LibmsiRecordClass;

struct _LibmsiRecordClass
{
    GObjectClass parent_class;
};

GType libmsi_record_get_type (void) G_GNUC_CONST;

LibmsiRecord *    libmsi_record_new (guint count);
LibmsiResult      libmsi_record_clear (LibmsiRecord *);
LibmsiResult      libmsi_record_set_int (LibmsiRecord *,unsigned,int);
LibmsiResult      libmsi_record_set_string (LibmsiRecord *,unsigned,const char *);
gchar *           libmsi_record_get_string (const LibmsiRecord *,unsigned);
unsigned          libmsi_record_get_field_count (const LibmsiRecord *);
int               libmsi_record_get_int (const LibmsiRecord *,unsigned);
gboolean          libmsi_record_is_null (const LibmsiRecord *,unsigned);

LibmsiResult      libmsi_record_load_stream (LibmsiRecord *,unsigned,const char *);
LibmsiResult      libmsi_record_save_stream (LibmsiRecord *,unsigned,char*,unsigned *);

G_END_DECLS

#endif /* _LIBMSI_RECORD_H */
