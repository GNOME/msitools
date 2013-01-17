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

#include <glib-object.h>

#include "libmsi-types.h"

G_BEGIN_DECLS

#define LIBMSI_TYPE_DATABASE             (libmsi_database_get_type ())
#define LIBMSI_DATABASE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIBMSI_TYPE_DATABASE, LibmsiDatabase))
#define LIBMSI_DATABASE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LIBMSI_TYPE_DATABASE, LibmsiDatabaseClass))
#define LIBMSI_IS_DATABASE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIBMSI_TYPE_DATABASE))
#define LIBMSI_IS_DATABASE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LIBMSI_TYPE_DATABASE))
#define LIBMSI_DATABASE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LIBMSI_TYPE_DATABASE, LibmsiDatabaseClass))

typedef struct _LibmsiDatabaseClass LibmsiDatabaseClass;

struct _LibmsiDatabaseClass
{
    GObjectClass parent_class;
};

GType libmsi_database_get_type (void) G_GNUC_CONST;


LibmsiDatabase *    libmsi_database_new                 (const gchar *path,
                                                         guint flags,
                                                         const char *persist,
                                                         GError **error);

gboolean            libmsi_database_is_readonly         (LibmsiDatabase *db);
LibmsiRecord *      libmsi_database_get_primary_keys    (LibmsiDatabase *db,
                                                         const char *table,
                                                         GError **error);
gboolean            libmsi_database_apply_transform     (LibmsiDatabase *db,
                                                         const char *file,
                                                         GError **error);
gboolean            libmsi_database_export              (LibmsiDatabase *db,
                                                         const char *table,
                                                         int fd,
                                                         GError **error);
gboolean            libmsi_database_import              (LibmsiDatabase *db,
                                                         const char *path,
                                                         GError **error);
gboolean            libmsi_database_is_table_persistent (LibmsiDatabase *db,
                                                         const char *table,
                                                         GError **error);
gboolean            libmsi_database_merge               (LibmsiDatabase *db,
                                                         LibmsiDatabase *merge,
                                                         const char *table,
                                                         GError **error);
gboolean            libmsi_database_commit              (LibmsiDatabase *db,
                                                         GError **error);

G_END_DECLS

#endif /* _LIBMSI_DATABASE_H */
