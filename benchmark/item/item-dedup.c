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
_benchmark_item_dedup_create(BenchmarkRun* run, gboolean use_batch)
{
	guint const n = 1000;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JBatch) delete_batch = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	gboolean ret;

	semantics = j_benchmark_get_semantics();
	delete_batch = j_batch_new(semantics);
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	while (j_benchmark_iterate(run))
	{
		j_benchmark_timer_start(run);

		for (guint i = 0; i < n; i++)
		{
			g_autoptr(JItemDedup) item = NULL;
			g_autofree gchar* name = NULL;

			name = g_strdup_printf("benchmark-%d", i);
			item = j_item_dedup_create(collection, name, NULL, batch);

			j_item_dedup_delete(item, delete_batch);

			if (!use_batch)
			{
				ret = j_batch_execute(batch);
				g_assert_true(ret);
			}
		}

		if (use_batch)
		{
			ret = j_batch_execute(batch);
			g_assert_true(ret);
		}

		j_benchmark_timer_stop(run);

		ret = j_batch_execute(delete_batch);
		g_assert_true(ret);
	}

	j_collection_delete(collection, delete_batch);
	ret = j_batch_execute(delete_batch);
	g_assert_true(ret);

	run->operations = n;
}

static void
benchmark_item_dedup_create(BenchmarkRun* run)
{
	_benchmark_item_dedup_create(run, FALSE);
}

static void
benchmark_item_dedup_create_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_create(run, TRUE);
}

static void
_benchmark_item_dedup_delete(BenchmarkRun* run, gboolean use_batch)
{
	guint const n = 1000;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JBatch) get_batch = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	gboolean ret;

	semantics = j_benchmark_get_semantics();
	get_batch = j_batch_new(semantics);
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			g_autoptr(JItemDedup) item = NULL;
			g_autofree gchar* name = NULL;

			name = g_strdup_printf("benchmark-%d", i);
			item = j_item_dedup_create(collection, name, NULL, batch);
		}

		ret = j_batch_execute(batch);
		g_assert_true(ret);

		j_benchmark_timer_start(run);

		for (guint i = 0; i < n; i++)
		{
			g_autoptr(JItemDedup) item = NULL;
			g_autofree gchar* name = NULL;

			name = g_strdup_printf("benchmark-%d", i);
			j_item_dedup_get(collection, &item, name, get_batch);
			ret = j_batch_execute(get_batch);
			g_assert_true(ret);

			j_item_dedup_delete(item, batch);

			if (!use_batch)
			{
				ret = j_batch_execute(batch);
				g_assert_true(ret);
			}
		}

		if (use_batch)
		{
			ret = j_batch_execute(batch);
			g_assert_true(ret);
		}

		j_benchmark_timer_stop(run);
	}

	j_collection_delete(collection, batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	run->operations = n;
}

static void
benchmark_item_dedup_delete(BenchmarkRun* run)
{
	_benchmark_item_dedup_delete(run, FALSE);
}

static void
benchmark_item_dedup_delete_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_delete(run, TRUE);
}

static void
benchmark_item_dedup_delete_batch_without_get(BenchmarkRun* run)
{
	guint const n = 1000;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JBatch) delete_batch = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	gboolean ret;

	semantics = j_benchmark_get_semantics();
	delete_batch = j_batch_new(semantics);
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			g_autoptr(JItemDedup) item = NULL;
			g_autofree gchar* name = NULL;

			name = g_strdup_printf("benchmark-%d", i);
			item = j_item_dedup_create(collection, name, NULL, batch);

			j_item_dedup_delete(item, delete_batch);
		}

		ret = j_batch_execute(batch);
		g_assert_true(ret);

		j_benchmark_timer_start(run);

		ret = j_batch_execute(delete_batch);
		g_assert_true(ret);

		j_benchmark_timer_stop(run);
	}

	j_collection_delete(collection, batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	run->operations = n;
}

static void
_benchmark_item_dedup_get_status(BenchmarkRun* run, gboolean use_batch)
{
	guint const n = (use_batch) ? 10000 : 1000;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JItemDedup) item = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	gchar dummy[1];
	guint64 nb;
	gboolean ret;

	memset(dummy, 0, 1);

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	item = j_item_dedup_create(collection, "benchmark", NULL, batch);
	j_item_dedup_write(item, dummy, 1, 0, &nb, batch);

	ret = j_batch_execute(batch);
	g_assert_true(ret);

	j_benchmark_timer_start(run);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			j_item_dedup_get_status(item, batch);

			if (!use_batch)
			{
				ret = j_batch_execute(batch);
				g_assert_true(ret);
			}
		}

		if (use_batch)
		{
			ret = j_batch_execute(batch);
			g_assert_true(ret);
		}
	}

	j_benchmark_timer_stop(run);

	j_item_dedup_delete(item, batch);
	j_collection_delete(collection, batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	run->operations = n;
}

static void
benchmark_item_dedup_get_status(BenchmarkRun* run)
{
	_benchmark_item_dedup_get_status(run, FALSE);
}

static void
benchmark_item_dedup_get_status_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_get_status(run, TRUE);
}

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

	for (guint i = 0; i < n; i++)
	{
		j_item_dedup_write(item, dummy, block_size, i * block_size, &nb, batch);
	}
	ret = j_batch_execute(batch);
	g_assert_true(ret);
	//g_assert_cmpuint(nb, ==, n * block_size);

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
_benchmark_item_dedup_write(BenchmarkRun* run, gboolean use_batch, guint block_size)
{
	guint const n = (use_batch) ? 250 : 250;

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
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	j_benchmark_timer_start(run);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			j_item_dedup_write(item, &dummy, block_size, i * block_size, &nb, batch);

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

	run->operations = n;
	run->bytes = n * block_size;
}

static void
benchmark_item_dedup_write(BenchmarkRun* run)
{
	_benchmark_item_dedup_write(run, FALSE, 4 * 1024);
}

static void
benchmark_item_dedup_write_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_write(run, TRUE, 4 * 1024);
}

static void
_benchmark_item_dedup_unordered_create_delete(BenchmarkRun* run, gboolean use_batch)
{
	guint const n = 50;

	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JSemantics) semantics = NULL;
	gboolean ret;

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	collection = j_collection_create("benchmark", batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	j_benchmark_timer_start(run);

	while (j_benchmark_iterate(run))
	{
		for (guint i = 0; i < n; i++)
		{
			g_autoptr(JItemDedup) item = NULL;
			g_autofree gchar* name = NULL;

			name = g_strdup_printf("benchmark-%d", i);
			item = j_item_dedup_create(collection, name, NULL, batch);
			j_item_dedup_delete(item, batch);

			if (!use_batch)
			{
				ret = j_batch_execute(batch);
				g_assert_true(ret);
			}
		}

		if (use_batch)
		{
			ret = j_batch_execute(batch);
			g_assert_true(ret);
		}
	}

	j_benchmark_timer_stop(run);

	j_collection_delete(collection, batch);
	ret = j_batch_execute(batch);
	g_assert_true(ret);

	run->operations = n * 2;
}

static void
benchmark_item_dedup_unordered_create_delete(BenchmarkRun* run)
{
	_benchmark_item_dedup_unordered_create_delete(run, FALSE);
}

static void
benchmark_item_dedup_unordered_create_delete_batch(BenchmarkRun* run)
{
	_benchmark_item_dedup_unordered_create_delete(run, TRUE);
}

void
benchmark_item_dedup(void)
{
	j_benchmark_add("/item/item-dedup/create", benchmark_item_dedup_create);
	j_benchmark_add("/item/item-dedup/create-batch", benchmark_item_dedup_create_batch);
	j_benchmark_add("/item/item-dedup/delete", benchmark_item_dedup_delete);
	j_benchmark_add("/item/item-dedup/delete-batch", benchmark_item_dedup_delete_batch);
	j_benchmark_add("/item/item-dedup/delete-batch-without-get", benchmark_item_dedup_delete_batch_without_get);
	//	j_benchmark_add("/item/item-dedup/get-status", benchmark_item_dedup_get_status);
	//	j_benchmark_add("/item/item-dedup/get-status-batch", benchmark_item_dedup_get_status_batch);
	/* FIXME get */
	j_benchmark_add("/item/item-dedup/read", benchmark_item_dedup_read);
	j_benchmark_add("/item/item-dedup/read-batch", benchmark_item_dedup_read_batch);
	j_benchmark_add("/item/item-dedup/write", benchmark_item_dedup_write);
	j_benchmark_add("/item/item-dedup/write-batch", benchmark_item_dedup_write_batch);

	j_benchmark_add("/item/item-dedup/unordered-create-delete", benchmark_item_dedup_unordered_create_delete);
	j_benchmark_add("/item/item-dedup/unordered-create-delete-batch", benchmark_item_dedup_unordered_create_delete_batch);
}
