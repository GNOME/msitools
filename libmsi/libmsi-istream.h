/*
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

#ifndef _LIBMSI_ISTREAM_H
#define _LIBMSI_ISTREAM_H

#include <glib-object.h>
#include <gio/gio.h>

#include "libmsi-types.h"

G_BEGIN_DECLS

#define LIBMSI_TYPE_ISTREAM             (libmsi_istream_get_type ())
#define LIBMSI_ISTREAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIBMSI_TYPE_ISTREAM, LibmsiIStream))
#define LIBMSI_ISTREAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LIBMSI_TYPE_ISTREAM, LibmsiIStreamClass))
#define LIBMSI_IS_ISTREAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIBMSI_TYPE_ISTREAM))
#define LIBMSI_IS_ISTREAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LIBMSI_TYPE_ISTREAM))
#define LIBMSI_ISTREAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LIBMSI_TYPE_ISTREAM, LibmsiIStreamClass))

typedef struct _LibmsiIStreamClass LibmsiIStreamClass;

struct _LibmsiIStreamClass
{
    GOutputStreamClass parent_class;
};

GType libmsi_istream_get_type (void) G_GNUC_CONST;

gssize            libmsi_istream_get_size           (LibmsiIStream *ostream);

G_END_DECLS

#endif /* _LIBMSI_ISTREAM_H */
