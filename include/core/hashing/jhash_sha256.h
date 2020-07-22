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

#ifndef JULEA_HASH_SHA256_H
#define JULEA_HASH_SHA256_H

#if !defined(JULEA_H) && !defined(JULEA_COMPILATION)
#error "Only <julea.h> can be included directly."
#endif

#include <glib.h>
#include <hashing/jhash_impl.h>

G_BEGIN_DECLS

void* sha256_context(void);

int sha256_init(void* ctx);

int sha256_update(void* ctx, const void* data, size_t length);

int sha256_finalize(void* ctx, gchar** hash);

int sha256_destroy(void* ctx);

extern jhash_algorithm hash_sha256;

G_END_DECLS

#endif