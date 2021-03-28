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

#include <julea-config.h>

#include <glib.h>

#include <string.h>

#include <julea.h>
#include <julea-item.h>

#include "benchmark.h"

static void
_benchmark_item_dedup_read(BenchmarkRun* run, gboolean use_batch, guint block_size)
{
	guint const n = (use_batch) ? 10 : 10;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JItemDedup) item = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	gchar dummy[block_size];
	guint64 nb = 0;
	gboolean ret;

	memset(dummy, 0, block_size);

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	item = j_item_dedup_create(collection, "benchmark", NULL, batch);
	j_item_dedup_set_chunk_size(item, block_size);
	for (guint i = 0; i < n; i++)
	{
		j_item_dedup_write(item, dummy, block_size, i * block_size, &nb, batch);
	}
	ret = j_batch_execute(batch);
	g_assert_true(ret);
	//g_assert_cmpuint(nb, ==, n * block_size);

	//g_printerr("physical_size: %ld\n", j_item_dedup_get_size_physical(item));

	j_benchmark_timer_start(run);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			j_item_dedup_read(item, dummy, block_size, i * block_size, &nb, batch);

			if (!use_batch)
			{
				ret = j_batch_execute(batch);
				g_assert_true(ret);
				//g_assert_cmpuint(nb, ==, block_size); //FIXME
			}
		}

		if (use_batch)
		{
			ret = j_batch_execute(batch);
			g_assert_true(ret);
			//g_assert_cmpuint(nb, ==, n * block_size);
		}
	}

	j_benchmark_timer_stop(run);

	j_item_dedup_delete(item, batch);
	j_collection_delete(collection, batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	run->operations = n;
	run->bytes = n * block_size;
}

static void
benchmark_item_dedup_read(BenchmarkRun* run)
{
	_benchmark_item_dedup_read(run, FALSE, 4 * 1024);
}

static void
benchmark_item_dedup_read_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_read(run, TRUE, 4 * 1024);
}

static void
_benchmark_item_dedup_write(BenchmarkRun* run, gboolean use_batch, guint64 block_size)
{
	guint const n = (use_batch) ? 250 : 10;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JItemDedup) item = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	//gchar dummy[block_size];
	gchar* dummy = calloc(1, block_size);

	guint64 nb = 0;
	gboolean ret;

	//memset(dummy, 0, block_size);

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	item = j_item_dedup_create(collection, "benchmark", NULL, batch);
	j_item_dedup_set_chunk_size(item, block_size);

	ret = j_batch_execute(batch);
	g_assert_true(ret);

	j_benchmark_timer_start(run);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			j_item_dedup_write(item, dummy, block_size, i * block_size, &nb, batch);

			if (!use_batch)
			{
				ret = j_batch_execute(batch);
				//g_assert_true(ret);
				//g_assert_cmpuint(nb, ==, block_size);
			}
		}

		if (use_batch)
		{
			ret = j_batch_execute(batch);
			//g_assert_true(ret);
			//g_assert_cmpuint(nb, ==, n * block_size);
		}
	}

	j_benchmark_timer_stop(run);

	j_item_dedup_delete(item, batch);
	j_collection_delete(collection, batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);
	free(dummy);
	run->operations = n;
	run->bytes = n * block_size;
}

static void
benchmark_item_dedup_write(BenchmarkRun* run)
{
	_benchmark_item_dedup_write(run, FALSE, 4 * 1024 * 100000);
}

static void
benchmark_item_dedup_write_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_write(run, TRUE, 4 * 1024 * 10000);
}

void
benchmark_item_dedup(void)
{
	j_benchmark_add("/item/item-dedup/read", benchmark_item_dedup_read);
	j_benchmark_add("/item/item-dedup/read-batch", benchmark_item_dedup_read_batch);
	j_benchmark_add("/item/item-dedup/write", benchmark_item_dedup_write);
	j_benchmark_add("/item/item-dedup/write-batch", benchmark_item_dedup_write_batch);
}
