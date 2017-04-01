/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2017 Michael Kuhn
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
#include <julea-object.h>

#include "benchmark.h"

static
void
_benchmark_object_create (BenchmarkResult* result, gboolean use_batch)
{
	guint const n = 100000;

	JBatch* delete_batch;
	JBatch* batch;
	JSemantics* semantics;
	gdouble elapsed;

	semantics = j_benchmark_get_semantics();
	delete_batch = j_batch_new(semantics);
	batch = j_batch_new(semantics);

	j_benchmark_timer_start();

	for (guint i = 0; i < n; i++)
	{
		JObject* object;
		gchar* name;

		name = g_strdup_printf("benchmark-%d", i);
		object = j_object_new(0, "benchmark", name);
		j_object_create(object, batch);
		g_free(name);

		j_object_delete(object, delete_batch);
		j_object_unref(object);

		if (!use_batch)
		{
			j_batch_execute(batch);
		}
	}

	if (use_batch)
	{
		j_batch_execute(batch);
	}

	elapsed = j_benchmark_timer_elapsed();

	j_batch_execute(delete_batch);

	j_batch_unref(delete_batch);
	j_batch_unref(batch);
	j_semantics_unref(semantics);

	result->elapsed_time = elapsed;
	result->operations = n;
}

static
void
benchmark_object_create (BenchmarkResult* result)
{
	_benchmark_object_create(result, FALSE);
}

static
void
benchmark_object_create_batch (BenchmarkResult* result)
{
	_benchmark_object_create(result, TRUE);
}

static
void
_benchmark_object_delete (BenchmarkResult* result, gboolean use_batch)
{
	guint const n = 100000;

	JBatch* batch;
	JSemantics* semantics;
	gdouble elapsed;

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	for (guint i = 0; i < n; i++)
	{
		JObject* object;
		gchar* name;

		name = g_strdup_printf("benchmark-%d", i);
		object = j_object_new(0, "benchmark", name);
		j_object_create(object, batch);
		g_free(name);

		j_object_unref(object);
	}

	j_batch_execute(batch);

	j_benchmark_timer_start();

	for (guint i = 0; i < n; i++)
	{
		JObject* object;
		gchar* name;

		name = g_strdup_printf("benchmark-%d", i);
		object = j_object_new(0, "benchmark", name);
		g_free(name);

		j_object_delete(object, batch);
		j_object_unref(object);

		if (!use_batch)
		{
			j_batch_execute(batch);
		}
	}

	if (use_batch)
	{
		j_batch_execute(batch);
	}

	elapsed = j_benchmark_timer_elapsed();

	j_batch_execute(batch);

	j_batch_unref(batch);
	j_semantics_unref(semantics);

	result->elapsed_time = elapsed;
	result->operations = n;
}

static
void
benchmark_object_delete (BenchmarkResult* result)
{
	_benchmark_object_delete(result, FALSE);
}

static
void
benchmark_object_delete_batch (BenchmarkResult* result)
{
	_benchmark_object_delete(result, TRUE);
}

static
void
_benchmark_object_status (BenchmarkResult* result, gboolean use_batch)
{
	guint const n = 200000;

	JObject* object;
	JBatch* batch;
	JSemantics* semantics;
	gchar dummy[1];
	gdouble elapsed;
	gint64 modification_time;
	guint64 size;

	memset(dummy, 0, 1);

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	object = j_object_new(0, "benchmark", "benchmark");
	j_object_create(object, batch);
	j_object_write(object, dummy, 1, 0, &size, batch);

	j_batch_execute(batch);

	j_benchmark_timer_start();

	for (guint i = 0; i < n; i++)
	{
		j_object_status(object, &modification_time, &size, batch);

		if (!use_batch)
		{
			j_batch_execute(batch);
		}
	}

	if (use_batch)
	{
		j_batch_execute(batch);
	}

	elapsed = j_benchmark_timer_elapsed();

	j_object_delete(object, batch);
	j_object_unref(object);
	j_batch_execute(batch);

	j_batch_unref(batch);
	j_semantics_unref(semantics);

	result->elapsed_time = elapsed;
	result->operations = n;
}

static
void
benchmark_object_status (BenchmarkResult* result)
{
	_benchmark_object_status(result, FALSE);
}

static
void
benchmark_object_status_batch (BenchmarkResult* result)
{
	_benchmark_object_status(result, TRUE);
}

static
void
_benchmark_object_read (BenchmarkResult* result, gboolean use_batch, guint block_size)
{
	guint const n = 200000;

	JObject* object;
	JBatch* batch;
	JSemantics* semantics;
	gchar dummy[block_size];
	gdouble elapsed;
	guint64 nb = 0;

	memset(dummy, 0, block_size);

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	object = j_object_new(0, "benchmark", "benchmark");
	j_object_create(object, batch);

	for (guint i = 0; i < n; i++)
	{
		j_object_write(object, dummy, block_size, i * block_size, &nb, batch);
	}

	j_batch_execute(batch);
	g_assert_cmpuint(nb, ==, n * block_size);

	j_benchmark_timer_start();

	for (guint i = 0; i < n; i++)
	{
		j_object_read(object, dummy, block_size, i * block_size, &nb, batch);

		if (!use_batch)
		{
			j_batch_execute(batch);

			g_assert_cmpuint(nb, ==, block_size);
		}
	}

	if (use_batch)
	{
		j_batch_execute(batch);

		g_assert_cmpuint(nb, ==, n * block_size);
	}

	elapsed = j_benchmark_timer_elapsed();

	j_object_delete(object, batch);
	j_object_unref(object);
	j_batch_execute(batch);

	j_batch_unref(batch);
	j_semantics_unref(semantics);

	result->elapsed_time = elapsed;
	result->operations = n;
	result->bytes = n * block_size;
}

static
void
benchmark_object_read (BenchmarkResult* result)
{
	_benchmark_object_read(result, FALSE, 4 * 1024);
}

static
void
benchmark_object_read_batch (BenchmarkResult* result)
{
	_benchmark_object_read(result, TRUE, 4 * 1024);
}

static
void
_benchmark_object_write (BenchmarkResult* result, gboolean use_batch, guint block_size)
{
	guint const n = 200000;

	JObject* object;
	JBatch* batch;
	JSemantics* semantics;
	gchar dummy[block_size];
	gdouble elapsed;
	guint64 nb = 0;

	memset(dummy, 0, block_size);

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	object = j_object_new(0, "benchmark", "benchmark");
	j_object_create(object, batch);
	j_batch_execute(batch);

	j_benchmark_timer_start();

	for (guint i = 0; i < n; i++)
	{
		j_object_write(object, &dummy, block_size, i * block_size, &nb, batch);

		if (!use_batch)
		{
			j_batch_execute(batch);

			g_assert_cmpuint(nb, ==, block_size);
		}
	}

	if (use_batch)
	{
		j_batch_execute(batch);

		g_assert_cmpuint(nb, ==, n * block_size);
	}

	elapsed = j_benchmark_timer_elapsed();

	j_object_delete(object, batch);
	j_object_unref(object);
	j_batch_execute(batch);

	j_batch_unref(batch);
	j_semantics_unref(semantics);

	result->elapsed_time = elapsed;
	result->operations = n;
	result->bytes = n * block_size;
}

static
void
benchmark_object_write (BenchmarkResult* result)
{
	_benchmark_object_write(result, FALSE, 4 * 1024);
}

static
void
benchmark_object_write_batch (BenchmarkResult* result)
{
	_benchmark_object_write(result, TRUE, 4 * 1024);
}

static
void
_benchmark_object_unordered_create_delete (BenchmarkResult* result, gboolean use_batch)
{
	guint const n = 100000;

	JBatch* batch;
	JSemantics* semantics;
	gdouble elapsed;

	semantics = j_benchmark_get_semantics();
	batch = j_batch_new(semantics);

	j_benchmark_timer_start();

	for (guint i = 0; i < n; i++)
	{
		JObject* object;
		gchar* name;

		name = g_strdup_printf("benchmark-%d", i);
		object = j_object_new(0, "benchmark", name);
		j_object_create(object, batch);
		g_free(name);

		j_object_delete(object, batch);
		j_object_unref(object);

		if (!use_batch)
		{
			j_batch_execute(batch);
		}
	}

	if (use_batch)
	{
		j_batch_execute(batch);
	}

	elapsed = j_benchmark_timer_elapsed();

	j_batch_execute(batch);

	j_batch_unref(batch);
	j_semantics_unref(semantics);

	result->elapsed_time = elapsed;
	result->operations = n;
}

static
void
benchmark_object_unordered_create_delete (BenchmarkResult* result)
{
	_benchmark_object_unordered_create_delete(result, FALSE);
}

static
void
benchmark_object_unordered_create_delete_batch (BenchmarkResult* result)
{
	_benchmark_object_unordered_create_delete(result, TRUE);
}

void
benchmark_object (void)
{
	j_benchmark_run("/object/object/create", benchmark_object_create);
	j_benchmark_run("/object/object/create-batch", benchmark_object_create_batch);
	j_benchmark_run("/object/object/delete", benchmark_object_delete);
	j_benchmark_run("/object/object/delete-batch", benchmark_object_delete_batch);
	j_benchmark_run("/object/object/status", benchmark_object_status);
	j_benchmark_run("/object/object/status-batch", benchmark_object_status_batch);
	/* FIXME get */
	j_benchmark_run("/object/object/read", benchmark_object_read);
	j_benchmark_run("/object/object/read-batch", benchmark_object_read_batch);
	j_benchmark_run("/object/object/write", benchmark_object_write);
	j_benchmark_run("/object/object/write-batch", benchmark_object_write_batch);

	j_benchmark_run("/object/object/unordered-create-delete", benchmark_object_unordered_create_delete);
	j_benchmark_run("/object/object/unordered-create-delete-batch", benchmark_object_unordered_create_delete_batch);
}