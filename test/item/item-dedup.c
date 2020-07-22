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

#include <julea.h>
#include <julea-item.h>

#include "test.h"

static void
test_item_dedup_fixture_setup(JItemDedup** item, gconstpointer data)
{
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JCollection) collection = NULL;

	(void)data;

	batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
	collection = j_collection_create("test-collection", batch);
	*item = j_item_dedup_create(collection, "test-item", NULL, batch);
}

static void
test_item_dedup_fixture_teardown(JItemDedup** item, gconstpointer data)
{
	(void)data;

	j_item_dedup_unref(*item);
}

static void
test_item_dedup_new_free(void)
{
	guint const n = 1;

	for (guint i = 0; i < n; i++)
	{
		g_autoptr(JBatch) batch = NULL;
		g_autoptr(JCollection) collection = NULL;
		g_autoptr(JItemDedup) item = NULL;

		batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
		collection = j_collection_create("test-collection", batch);
		item = j_item_dedup_create(collection, "test-item", NULL, batch);

		g_assert(item != NULL);
	}
}

static void
test_item_dedup_ref_unref(JItemDedup** item, gconstpointer data)
{
	JItemDedup* ref_item;

	(void)data;

	ref_item = j_item_dedup_ref(*item);
	g_assert_true(*item == ref_item);
	j_item_dedup_unref(*item);
}

static void
test_item_dedup_name(JItemDedup** item, gconstpointer data)
{
	(void)data;

	g_assert_cmpstr(j_item_dedup_get_name(*item), ==, "test-item");
}

static void
test_item_dedup_size(JItemDedup** item, gconstpointer data)
{
	(void)data;

	g_assert_cmpuint(j_item_dedup_get_size(*item), ==, 0);
}

static void
test_item_dedup_modification_time(JItemDedup** item, gconstpointer data)
{
	(void)data;

	g_assert_cmpuint(j_item_dedup_get_modification_time(*item), >, 0);
}

static void
test_example(void)
{
	guint const n = 1;

	for (guint i = 0; i < n; i++)
	{
		gboolean ret;
		g_autoptr(JBatch) batch = NULL;
		g_autoptr(JCollection) collection = NULL;
		g_autoptr(JItemDedup) item = NULL;
		const char data[] = "1234567887654321"; //test-data-12345
		char data2[sizeof(data)];
		char data3[3];
		const char fortytwo[] = "42";
		guint64 bytes_written = 0;
		guint64 bytes_read = 0;
		printf("sizeof(data): %lu\n", sizeof(data));

		batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
		collection = j_collection_create("test-collection", batch);
		item = j_item_dedup_create(collection, "test-item", NULL, batch);
		ret = j_batch_execute(batch);

		printf("before write: data: %s\n", data);

		// strings und ihr doofer \0 terminator >.<
		printf("\nTEST: write 2 full chunks\n");
		j_item_dedup_write(item, &data, sizeof(data) - 1, 0, &bytes_written, batch);
		ret = j_batch_execute(batch);
		printf("bytes_written: %lu\n", bytes_written);

		printf("\nTEST: read 2 full chunks\n");
		j_item_dedup_read(item, data2, 16, 0, &bytes_read, batch);
		ret = j_batch_execute(batch);
		data2[sizeof(data) - 1] = '\0';
		printf("bytes_read: %lu\n", bytes_read);
		printf("after read: data: %s\n", data2);

		printf("\nTEST: read 2 chars\n");
		j_item_dedup_read(item, data3, 2, 7, &bytes_read, batch);
		ret = j_batch_execute(batch);
		data3[2] = '\0';
		printf("bytes_read: %lu\n", bytes_read);
		printf("after read: data: %s\n", data3);

		printf("\nTEST: overwrite 2 chars\n");
		printf("before write: data: %s\n", fortytwo);
		bytes_written = 0;
		j_item_dedup_write(item, &fortytwo, 2, 0, &bytes_written, batch);
		ret = j_batch_execute(batch);
		printf("bytes_written: %lu\n", bytes_written);

		memset(data2, '0', 16);
		printf("\nTEST: read 2 full chunks\n");
		j_item_dedup_read(item, data2, 16, 0, &bytes_read, batch);
		ret = j_batch_execute(batch);
		data2[16] = '\0';
		printf("bytes_read: %lu\n", bytes_read);
		printf("after read: data: %s\n", data2);

		memset(data2, '0', 16);
		printf("\nTEST: read 3 bytes of 1 chunks\n");
		j_item_dedup_read(item, data2, 3, 0, &bytes_read, batch);
		ret = j_batch_execute(batch);
		data2[16] = '\0';
		printf("bytes_read: %lu\n", bytes_read);
		printf("after read: data: %s\n", data2);

		j_item_dedup_delete(item, batch);
		ret = j_batch_execute(batch);

		g_assert(item != NULL);

		//TODO: ??
		if (!ret)
		{
		}
	}
}

void
test_item_dedup(void)
{
	g_test_add_func("/item/item_dedup/new_free", test_item_dedup_new_free);
	g_test_add_func("/item/item_dedup/test_example", test_example);
	g_test_add("/item/item_dedup/ref_unref", JItemDedup*, NULL, test_item_dedup_fixture_setup, test_item_dedup_ref_unref, test_item_dedup_fixture_teardown);
	g_test_add("/item/item_dedup/name", JItemDedup*, NULL, test_item_dedup_fixture_setup, test_item_dedup_name, test_item_dedup_fixture_teardown);
	g_test_add("/item/item_dedup/size", JItemDedup*, NULL, test_item_dedup_fixture_setup, test_item_dedup_size, test_item_dedup_fixture_teardown);
	g_test_add("/item/item_dedup/modification_time", JItemDedup*, NULL, test_item_dedup_fixture_setup, test_item_dedup_modification_time, test_item_dedup_fixture_teardown);
}