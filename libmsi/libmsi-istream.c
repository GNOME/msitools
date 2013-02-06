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

#include "msipriv.h"
#include "libmsi-istream.h"

struct _LibmsiIStream
{
    GInputStream parent;

    GsfInput *input;
};

static void libmsi_seekable_iface_init (GSeekableIface  *iface);

G_DEFINE_TYPE_WITH_CODE (LibmsiIStream, libmsi_istream, G_TYPE_INPUT_STREAM,
                         G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
                                                libmsi_seekable_iface_init);
                         )

static goffset
libmsi_tell (GSeekable *seekable)
{
    g_return_val_if_fail (LIBMSI_IS_ISTREAM(seekable), FALSE);

    return gsf_input_tell (LIBMSI_ISTREAM(seekable)->input);
}

static gboolean
libmsi_can_seek (GSeekable *seekable)
{
    return TRUE;
}

static gboolean
libmsi_seek (GSeekable *seekable, goffset offset,
             GSeekType type, GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail (LIBMSI_IS_ISTREAM(seekable), FALSE);

    return !gsf_input_seek (LIBMSI_ISTREAM(seekable)->input, offset, type);
}

static gboolean
libmsi_can_truncate (GSeekable *seekable)
{
    return FALSE;
}

static void
libmsi_seekable_iface_init (GSeekableIface *iface)
{
    iface->tell = libmsi_tell;
    iface->can_seek = libmsi_can_seek;
    iface->seek = libmsi_seek;
    iface->can_truncate = libmsi_can_truncate;
}

static void
libmsi_istream_init (LibmsiIStream *self)
{
}

static void
libmsi_istream_finalize (GObject *object)
{
    LibmsiIStream *self = LIBMSI_ISTREAM (object);

    if (self->input)
        g_object_unref (self->input);

    G_OBJECT_CLASS (libmsi_istream_parent_class)->finalize (object);
}

static gssize
input_stream_read (GInputStream  *stream,
                   void *buffer,
                   gsize count,
                   GCancellable *cancellable,
                   GError **error)
{
    LibmsiIStream *self = LIBMSI_ISTREAM (stream);
    gssize remaining = gsf_input_remaining (self->input);

    if (remaining == 0)
        return 0;

    count = MIN(count, remaining);
    if (!gsf_input_read (self->input, count, buffer))
        return -1;

    return count;
}

static gssize
input_stream_skip (GInputStream *stream,
                   gsize count,
                   GCancellable  *cancellable,
                   GError **error)
{
    LibmsiIStream *self = LIBMSI_ISTREAM (stream);

    count = MIN (count, gsf_input_remaining (self->input));
    if (!gsf_input_seek (self->input, count, G_SEEK_CUR))
        return -1;

    return count;
}

static gboolean
input_stream_close (GInputStream *stream,
                    GCancellable *cancellable,
                    GError **error)
{
    return TRUE;
}

static void
libmsi_istream_class_init (LibmsiIStreamClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GInputStreamClass *istream_class;

    object_class->finalize = libmsi_istream_finalize;

    istream_class = G_INPUT_STREAM_CLASS (klass);
    istream_class->read_fn = input_stream_read;
    istream_class->skip  = input_stream_skip;
    istream_class->close_fn = input_stream_close;
}

G_GNUC_INTERNAL LibmsiIStream *
libmsi_istream_new (GsfInput *input)
{
    GsfInput *dup = gsf_input_dup (input, NULL);
    g_return_val_if_fail (dup, NULL);

    LibmsiIStream *self = g_object_new (LIBMSI_TYPE_ISTREAM, NULL);
    self->input = dup;

    return self;
}
