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

#include <glib-object.h>

#include "libmsi-types.h"

G_BEGIN_DECLS

#define LIBMSI_TYPE_SUMMARY_INFO             (libmsi_summary_info_get_type ())
#define LIBMSI_SUMMARY_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIBMSI_TYPE_SUMMARY_INFO, LibmsiSummaryInfo))
#define LIBMSI_SUMMARY_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LIBMSI_TYPE_SUMMARY_INFO, LibmsiSummaryInfoClass))
#define LIBMSI_IS_SUMMARY_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIBMSI_TYPE_SUMMARY_INFO))
#define LIBMSI_IS_SUMMARY_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LIBMSI_TYPE_SUMMARY_INFO))
#define LIBMSI_SUMMARY_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LIBMSI_TYPE_SUMMARY_INFO, LibmsiSummaryInfoClass))

typedef struct _LibmsiSummaryInfoClass LibmsiSummaryInfoClass;

struct _LibmsiSummaryInfoClass
{
    GObjectClass parent_class;
};

GType libmsi_summary_info_get_type (void) G_GNUC_CONST;

LibmsiSummaryInfo *   libmsi_summary_info_new          (LibmsiDatabase *database,
                                                        unsigned update_count,
                                                        GError **error);
LibmsiPropertyType    libmsi_summary_info_get_property_type (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        GError **error);
const gchar *         libmsi_summary_info_get_string   (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        GError **error);
gint                  libmsi_summary_info_get_int      (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        GError **error);
guint64               libmsi_summary_info_get_filetime (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        GError **error);
gboolean              libmsi_summary_info_set_string   (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        const gchar *value,
                                                        GError **error);
gboolean              libmsi_summary_info_set_int      (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        gint value,
                                                        GError **error);
gboolean              libmsi_summary_info_set_filetime (LibmsiSummaryInfo *si,
                                                        LibmsiProperty prop,
                                                        guint64 value,
                                                        GError **error);
gboolean              libmsi_summary_info_persist      (LibmsiSummaryInfo *si,
                                                        GError **error);
gboolean              libmsi_summary_info_save         (LibmsiSummaryInfo *si,
                                                        LibmsiDatabase *db,
                                                        GError **error);
GArray *              libmsi_summary_info_get_properties (LibmsiSummaryInfo *si);

G_END_DECLS

#endif /* _LIBMSI_SUMMARY_INFO_H */
