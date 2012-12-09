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

#ifndef _LIBMSI_QUERY_H
#define _LIBMSI_QUERY_H

#include "libmsi-types.h"

LibmsiResult      libmsi_query_fetch (LibmsiQuery *,LibmsiRecord **);
LibmsiResult      libmsi_query_execute (LibmsiQuery *,LibmsiRecord *);
LibmsiResult      libmsi_query_close (LibmsiQuery *);
LibmsiDBError     libmsi_query_get_error (LibmsiQuery *,char *,unsigned *);
LibmsiResult      libmsi_query_get_column_info (LibmsiQuery *, LibmsiColInfo, LibmsiRecord **);

#endif /* _LIBMSI_QUERY_H */
