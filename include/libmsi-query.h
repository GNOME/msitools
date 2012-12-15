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

#include <glib-object.h>

#include "libmsi-types.h"

G_BEGIN_DECLS

#define LIBMSI_TYPE_QUERY             (libmsi_query_get_type ())
#define LIBMSI_QUERY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIBMSI_TYPE_QUERY, LibmsiQuery))
#define LIBMSI_QUERY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LIBMSI_TYPE_QUERY, LibmsiQueryClass))
#define LIBMSI_IS_QUERY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIBMSI_TYPE_QUERY))
#define LIBMSI_IS_QUERY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LIBMSI_TYPE_QUERY))
#define LIBMSI_QUERY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LIBMSI_TYPE_QUERY, LibmsiQueryClass))

typedef struct _LibmsiQueryClass LibmsiQueryClass;

struct _LibmsiQueryClass
{
    GObjectClass parent_class;
};

GType libmsi_query_get_type (void) G_GNUC_CONST;


LibmsiQuery *     libmsi_query_new               (LibmsiDatabase *database,
                                                  const gchar *query,
                                                  GError **error);
LibmsiRecord *    libmsi_query_fetch             (LibmsiQuery *query,
                                                  GError **error);
gboolean          libmsi_query_execute           (LibmsiQuery *query,
                                                  LibmsiRecord *rec,
                                                  GError **error);
gboolean          libmsi_query_close             (LibmsiQuery *query,
                                                  GError **error);
void              libmsi_query_get_error         (LibmsiQuery *query,
                                                  gchar **column,
                                                  GError **error);
LibmsiRecord *    libmsi_query_get_column_info   (LibmsiQuery *query,
                                                  LibmsiColInfo info,
                                                  GError **error);

G_END_DECLS

#endif /* _LIBMSI_QUERY_H */
