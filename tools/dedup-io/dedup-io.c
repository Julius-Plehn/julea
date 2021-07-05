/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2019-2021 Michael Kuhn
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

#include <julea.h>
#include <julea-item.h>

#include <locale.h>
#include <stdio.h>

GArray* files;

void
collect_files(gchar* path)
{
	//GError* error;
	GDir* dir = g_dir_open(path, 0, NULL);
	if (dir)
	{
		gchar* elem;
		while ((elem = g_dir_read_name(dir)))
		{
			gchar* absolute_path = g_build_filename(path, elem);
			collect_files(absolute_path);
			//g_free(absolute_path)
		}
		//free(elem);
		g_dir_close(dir);
	}
	else
	{
		g_array_append_val(files, path);
	}
}

int
main(int argc, char** argv)
{
	gint opt_chunk_size = 128000;
	gchar* opt_path = NULL;
	g_autoptr(JBatch) batch = NULL;
	g_autoptr(JCollection) collection = NULL;
	g_autoptr(JItemDedup) item = NULL;
	g_autofree gchar* basename = NULL;
	gsize size_total = 0;
	gsize size_physical = 0;

	GError* error = NULL;
	GOptionContext* context;

	GOptionEntry entries[] = {
		{ "chunk_size", 'd', 0, G_OPTION_ARG_INT, &opt_chunk_size, "Chunk size to use", "128000" },
		{ "path", 'p', 0, G_OPTION_ARG_STRING, &opt_path, "File path to use", NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	// Explicitly enable UTF-8 since functions such as g_format_size might return UTF-8 characters.
	setlocale(LC_ALL, "C.UTF-8");

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, entries, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error))
	{
		g_option_context_free(context);

		if (error != NULL)
		{
			g_printerr("Error: %s\n", error->message);
			g_error_free(error);
		}

		return 1;
	}

	g_option_context_free(context);
	basename = g_path_get_basename(opt_path);

	// Store files in a GArray and work on them later on

	files = g_array_new(FALSE, FALSE, sizeof(gchar*));
	if (g_file_test(opt_path, G_FILE_TEST_IS_REGULAR))
	{
		g_array_append_val(files, opt_path);
	}
	else
	{
		collect_files(opt_path);
	}

	for (int i = 0; i < files->len; i++)
	{
		gchar* contents;
		gsize len;
		guint64 bytes_written = 0;
		gchar* file = g_array_index(files, gchar*, i);
		GString* file_id = g_string_new(file);
		g_string_replace(file_id, "/", "-", 0);

		if (!g_file_get_contents(file, &contents, &len, &error))
		{
			g_printerr("Error: %s\n", error->message);
			g_error_free(error);
			continue;
		}
		printf("Open file: %s | Size: %ld\n", file, len);
		size_total += len;

		batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
		collection = j_collection_create("dedup-io", batch);
		item = j_item_dedup_create(collection, file_id->str, NULL, batch);
		//g_assert_true(j_batch_execute(batch));

		j_item_dedup_set_chunk_size(item, opt_chunk_size);
		j_item_dedup_write(item, contents, len, 0, &bytes_written, batch);

		g_assert_true(j_batch_execute(batch));
		//g_assert_cmpint(bytes_written, ==, len);
		guint64 physical_size = j_item_dedup_get_size_physical(item);
		g_print("Physical Size: %ld\n", physical_size);
		size_physical += physical_size;

		g_free(contents);
		g_string_free(file_id, TRUE);
		//g_free(file);
	}
	g_array_free(files, TRUE);
	g_print("\nTotal Size: %ld | Total Physical Size: %ld\n", size_total, size_physical);
	/*

	if (!g_file_get_contents(opt_path, &contents, &len, &error))
	{
		g_printerr("Error: %s\n", error->message);
		g_error_free(error);
	}
	printf("Read file: %s | Size: %ld\n", opt_path, len);

	batch = j_batch_new_for_template(J_SEMANTICS_TEMPLATE_DEFAULT);
	collection = j_collection_create("dedup-io", batch);
	item = j_item_dedup_create(collection, basename, NULL, batch);
	//g_assert_true(j_batch_execute(batch));

	j_item_dedup_set_chunk_size(item, opt_chunk_size);
	j_item_dedup_write(item, contents, len, 0, &bytes_written, batch);

	g_assert_true(j_batch_execute(batch));
	//g_assert_cmpint(bytes_written, ==, len);
	guint64 physical_size = j_item_dedup_get_size_physical(item);
	g_print("Physical Size: %ld\n", physical_size);

	g_free(contents);
	g_free(opt_path);
	*/
	g_free(opt_path);
	return 0;
}