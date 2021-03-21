/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2010-2021 Michael Kuhn
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
test_item_fixture_setup(JItem** item, gconstpointer data)
{
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JCollection) collection = NULL;

	(void)data;

	batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
	collection = j_collection_create("test-collection", batch);
	*item = j_item_create(collection, "test-item", NULL, batch);
}

static void
test_item_fixture_teardown(JItem** item, gconstpointer data)
{
	(void)data;

	j_item_unref(*item);
}

static void
test_item_new_free(void)
{
	guint const n = 100000;

	for (guint i = 0; i < n; i++)
	{
		g_autoptr(JBatch) batch = NULL;
		g_autoptr(JCollection) collection = NULL;
		g_autoptr(JItem) item = NULL;

		batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
		collection = j_collection_create("test-collection", batch);
		item = j_item_create(collection, "test-item", NULL, batch);

		g_assert_true(item != NULL);
	}
}

static void
test_item_ref_unref(JItem** item, gconstpointer data)
{
	JItem* ref_item;

	(void)data;

	ref_item = j_item_ref(*item);
	g_assert_true(*item == ref_item);
	j_item_unref(*item);
}

static void
test_item_name(JItem** item, gconstpointer data)
{
	(void)data;

	g_assert_cmpstr(j_item_get_name(*item), ==, "test-item");
}

static void
test_item_size(JItem** item, gconstpointer data)
{
	(void)data;

	g_assert_cmpuint(j_item_get_size(*item), ==, 0);
}

static void
test_item_modification_time(JItem** item, gconstpointer data)
{
	(void)data;

	g_assert_cmpuint(j_item_get_modification_time(*item), >, 0);
}

static void
test_io(void)
{
	gboolean ret;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JItem) item = NULL;
	const char data[] = "1234567887654321";
	char data2[sizeof(data)];
	char data3[3];
	const char fortytwo[] = "42";
	guint64 bytes_written = 0;
	guint64 bytes_read = 0;

	batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
	collection = j_collection_create("test-collection", batch);
	item = j_item_create(collection, "test-item", NULL, batch);
	ret = j_batch_execute(batch);

	//printf("\nTEST: write 2 full chunks\n");
	j_item_write(item, &data, sizeof(data) - 1, 0, &bytes_written, batch);
	ret = j_batch_execute(batch);
	g_assert_cmpint(bytes_written, ==, 16);

	//printf("\nTEST: read 2 full chunks\n");
	j_item_read(item, data2, 16, 150, &bytes_read, batch);
	ret = j_batch_execute(batch);
	data2[sizeof(data) - 1] = '\0';
	printf("test %s\n", data2);
	g_assert_cmpint(bytes_read, ==, 16);
	g_assert_cmpstr(data2, ==, "1234567887654321");

	//printf("\nTEST: read 2 chars\n");
	j_item_read(item, data3, 2, 7, &bytes_read, batch);
	ret = j_batch_execute(batch);
	data3[2] = '\0';
	g_assert_cmpint(bytes_read, ==, 2);
	g_assert_cmpstr(data3, ==, "88");

	//printf("\nTEST: overwrite 2 chars\n");
	j_item_write(item, &fortytwo, 2, 0, &bytes_written, batch);
	ret = j_batch_execute(batch);
	g_assert_cmpint(bytes_written, ==, 2);

	memset(data2, '0', 16);
	//printf("\nTEST: read 2 full chunks #2\n");
	j_item_read(item, data2, 16, 0, &bytes_read, batch);
	ret = j_batch_execute(batch);
	g_assert_cmpint(bytes_read, ==, 16);
	g_assert_cmpstr(data2, ==, "4234567887654321");

	memset(data2, '0', 16);
	//printf("\nTEST: read 3 bytes of 1 chunks\n");
	j_item_read(item, data2, 3, 0, &bytes_read, batch);
	ret = j_batch_execute(batch);
	data2[3] = '\0';
	g_assert_cmpint(bytes_read, ==, 3);
	g_assert_cmpstr(data2, ==, "423");

	j_item_delete(item, batch);
	ret = j_batch_execute(batch);

	g_assert(item != NULL);

	//TODO: ??
	if (!ret)
	{
	}
}

void
test_item_item(void)
{
	g_test_add_func("/item/item/new_free", test_item_new_free);
	//g_test_add_func("/item/item/io", test_io);
	g_test_add("/item/item/ref_unref", JItem*, NULL, test_item_fixture_setup, test_item_ref_unref, test_item_fixture_teardown);
	g_test_add("/item/item/name", JItem*, NULL, test_item_fixture_setup, test_item_name, test_item_fixture_teardown);
	g_test_add("/item/item/size", JItem*, NULL, test_item_fixture_setup, test_item_size, test_item_fixture_teardown);
	g_test_add("/item/item/modification_time", JItem*, NULL, test_item_fixture_setup, test_item_modification_time, test_item_fixture_teardown);
}
