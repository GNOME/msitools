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

#ifndef _LIBMSI_DATABASE_H
#define _LIBMSI_DATABASE_H

#include "libmsi-types.h"

LibmsiResult        libmsi_database_open (const char *, const char *, LibmsiDatabase **);
LibmsiResult        libmsi_database_open_query (LibmsiDatabase *,const char *,LibmsiQuery **);
LibmsiDBState       libmsi_database_get_state (LibmsiDatabase *);
LibmsiResult        libmsi_database_get_primary_keys (LibmsiDatabase *,const char *,LibmsiRecord **);
LibmsiResult        libmsi_database_apply_transform (LibmsiDatabase *,const char *,int);
LibmsiResult        libmsi_database_export (LibmsiDatabase *, const char *, int fd);
LibmsiResult        libmsi_database_import (LibmsiDatabase *, const char *, const char *);
LibmsiCondition     libmsi_database_is_table_persistent (LibmsiDatabase *, const char *);
LibmsiResult        libmsi_database_merge (LibmsiDatabase *, LibmsiDatabase *, const char *);
LibmsiResult        libmsi_database_get_summary_info (LibmsiDatabase *, unsigned, LibmsiSummaryInfo **);
LibmsiResult        libmsi_database_commit (LibmsiDatabase *);

#endif /* _LIBMSI_DATABASE_H */
