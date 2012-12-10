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

#ifndef _LIBMSI_SUMMARY_INFO_H
#define _LIBMSI_SUMMARY_INFO_H

#include "libmsi-types.h"

LibmsiSummaryInfo *   libmsi_summary_info_new (LibmsiDatabase *database, unsigned update_count, GError **error);
LibmsiResult          libmsi_summary_info_get_property (LibmsiSummaryInfo *, LibmsiPropertyType,unsigned *,int *,guint64*,char *,unsigned *);
LibmsiResult          libmsi_summary_info_set_property (LibmsiSummaryInfo *, LibmsiPropertyType, unsigned, int, guint64*, const char *);
LibmsiResult          libmsi_summary_info_persist (LibmsiSummaryInfo *);
LibmsiResult          libmsi_summary_info_get_property_count (LibmsiSummaryInfo *,unsigned *);

#endif /* _LIBMSI_SUMMARY_INFO_H */
