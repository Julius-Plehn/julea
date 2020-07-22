/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2010-2020 Michael Kuhn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 **/

#include <julea-config.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <hashing/jhash_sha256.h>

void*
sha256_context(void)
{
	GChecksum* mdctx;
	mdctx = g_checksum_new(G_CHECKSUM_SHA256);
	return mdctx;
}

int
sha256_init(void* ctx)
{
	g_checksum_reset(ctx);
	return 0;
}

int
sha256_update(void* ctx, const void* data, size_t length)
{
	g_checksum_update(ctx, data, length);
	return 0;
}

int
sha256_finalize(void* ctx, gchar** hash)
{
	const gchar* checksum_string = g_checksum_get_string(ctx);
	*hash = g_strdup(checksum_string);
	return strlen(*hash);
}

int
sha256_destroy(void* ctx)
{
	g_checksum_free(ctx);
	return 0;
}

jhash_algorithm hash_sha256 = {
	sha256_context,
	sha256_init,
	sha256_update,
	sha256_finalize,
	sha256_destroy,
	"SHA256",
	J_HASH_SHA256
};

/**
 * @}
 **/