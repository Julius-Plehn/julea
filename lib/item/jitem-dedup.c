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

#include <string.h>

#include <bson.h>

#include <item/jitem-dedup.h>
#include <item/jitem-dedup-internal.h>

#include <item/jcollection.h>
#include <item/jcollection-internal.h>

#include <julea.h>
#include <julea-kv.h>
#include <julea-object.h>

#include <openssl/evp.h>
#include <math.h>

//#include <hashing/jhash_sha256.h>
//#include <hashing/jhash_xxhash.h>

static jhash_algorithm* algo_array[] = {
	&hash_sha256,
	//& hash_xxhash
};
/**
 * \defgroup JItem Item
 *
 * Data structures and functions for managing items.
 *
 * @{
 **/

struct JItemGetData
{
	JCollection* collection;
	JItemDedup** item;
};

typedef struct JItemGetData JItemGetData;

/**
 * A JItem.
 **/
struct JItemDedup
{
	/**
	 * The ID.
	 **/
	bson_oid_t id;

	/**
	 * The name.
	 **/
	gchar* name;

	JCredentials* credentials;

	JDistribution* distribution;

	JKV* kv;
	JKV* kv_h;
	JObject* object;

	/**
	 * The status.
	 **/
	struct
	{
		guint64 age;

		/**
		 * The size.
		 * Stored in bytes.
		 */
		guint64 size;

		/**
		 * The time of the last modification.
		 * Stored in microseconds since the Epoch.
		 */
		gint64 modification_time;
	} status;

	/**
	 * The parent collection.
	 **/
	JCollection* collection;

	/**
	 * The reference count.
	 **/
	gint ref_count;

	/**
	 * The hashes the item consists of
	 */
	GArray* hashes;

	/**
	 * Chunk size for static hashing
	 */
	guint64 chunk_size;
};

/**
 * Increases an item's reference count.
 *
 * \code
 * JItemDedup* i;
 *
 * j_item_ref(i);
 * \endcode
 *
 * \param item An item.
 *
 * \return #item.
 **/
JItemDedup*
j_item_dedup_ref(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_atomic_int_inc(&(item->ref_count));

	return item;
}

/**
 * Decreases an item's reference count.
 * When the reference count reaches zero, frees the memory allocated for the item.
 *
 * \code
 * \endcode
 *
 * \param item An item.
 **/
void
j_item_dedup_unref(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	g_return_if_fail(item != NULL);

	if (g_atomic_int_dec_and_test(&(item->ref_count)))
	{
		if (item->kv != NULL)
		{
			j_kv_unref(item->kv);
		}

		if (item->kv_h != NULL)
		{
			j_kv_unref(item->kv_h);
		}

		/*
		if (item->object != NULL)
		{
			j_distributed_object_unref(item->object);
		}
        */

		if (item->collection != NULL)
		{
			j_collection_unref(item->collection);
		}

		j_credentials_unref(item->credentials);
		//j_distribution_unref(item->distribution);

		g_free(item->name);

		g_slice_free(JItemDedup, item);
	}
}

/**
 * Returns an item's name.
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return The name.
 **/
gchar const*
j_item_dedup_get_name(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	g_return_val_if_fail(item != NULL, NULL);

	return item->name;
}

/**
 * Creates an item in a collection.
 *
 * \code
 * \endcode
 *
 * \param collection   A collection.
 * \param name         A name.
 * \param batch        A batch.
 *
 * \return A new item. Should be freed with j_item_dedup_unref().
 **/
JItemDedup*
j_item_dedup_create(JCollection* collection, gchar const* name, JDistribution* distribution, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	JItemDedup* item;
	bson_t* tmp;
	gpointer value;
	guint32 len;

	g_return_val_if_fail(collection != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	if ((item = j_item_dedup_new(collection, name, distribution)) == NULL)
	{
		return NULL;
	}

	tmp = j_item_dedup_serialize(item, j_batch_get_semantics(batch));

	/*
    gchar* json = bson_as_canonical_extended_json(tmp, NULL);
	g_print("JSON dings: %s\n", json);
	bson_free(json);
    */

	value = bson_destroy_with_steal(tmp, TRUE, &len);

	//j_distributed_object_create(item->object, batch);
	j_kv_put(item->kv, value, len, bson_free, batch);

	return item;
}

static void
j_item_dedup_get_callback(gpointer value, guint32 len, gpointer data_)
{
	JItemGetData* data = data_;
	bson_t tmp[1];

	bson_init_static(tmp, value, len);
	*(data->item) = j_item_dedup_new_from_bson(data->collection, tmp);

	j_collection_unref(data->collection);
	g_slice_free(JItemGetData, data);

	g_free(value);
}

/**
 * Gets an item from a collection.
 *
 * \code
 * \endcode
 *
 * \param collection A collection.
 * \param item       A pointer to an item.
 * \param name       A name.
 * \param batch      A batch.
 **/
void
j_item_dedup_get(JCollection* collection, JItemDedup** item, gchar const* name, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	JItemGetData* data;
	g_autoptr(JKV) kv = NULL;
	g_autofree gchar* path = NULL;

	g_return_if_fail(collection != NULL);
	g_return_if_fail(item != NULL);
	g_return_if_fail(name != NULL);

	data = g_slice_new(JItemGetData);
	data->collection = j_collection_ref(collection);
	data->item = item;

	path = g_build_path("/", j_collection_get_name(collection), name, NULL);
	kv = j_kv_new("items", path);
	j_kv_get_callback(kv, j_item_dedup_get_callback, data, batch);
}

/*
	JItemGetData* data = data_;
	bson_t tmp[1];

	bson_init_static(tmp, value, len);
	*(data->item) = j_item_new_from_bson(data->collection, tmp);

	j_collection_unref(data->collection);
	g_slice_free(JItemGetData, data);

	g_free(value);
*/

static void
j_item_hash_ref_callback(gpointer value, guint32 len, gpointer data_)
{
	bson_iter_t iter;
	bson_t tmp[1];
	guint32* refcount = data_;

	bson_init_static(tmp, value, len);

	if (bson_iter_init_find(&iter, tmp, "ref"))
		*refcount = (guint32)bson_iter_int32(&iter);
	g_free(value);
}

//TODO should only be called with chunk_lock held but chunk_lock not
//implemented yet
static void
j_item_unref_chunk(gchar* hash, JBatch* batch)
{
	JKV* chunk_kv;
	JObject* chunk_obj;
	guint64 refcount = 0;
	JBatch* sub_batch = j_batch_new(j_batch_get_semantics(batch));
	bson_t* new_ref_bson;
	gboolean ret;

	chunk_kv = j_kv_new("chunk_refs", (const gchar*)hash);
	j_kv_get_callback(chunk_kv, j_item_hash_ref_callback, &refcount, sub_batch);
	ret = j_batch_execute(sub_batch);

	refcount -= 1;

	if (refcount > 0)
	{
		new_ref_bson = bson_new();
		bson_append_int32(new_ref_bson, "ref", -1, refcount);

		{
			gpointer value;
			guint32 value_len;
			value = bson_destroy_with_steal(new_ref_bson, TRUE, &value_len);

			j_kv_put(chunk_kv, value, value_len, bson_free, sub_batch);
		}
		ret = j_batch_execute(sub_batch);
	}
	else
	{
		j_kv_delete(chunk_kv, batch);
		chunk_obj = j_object_new("chunks", (const gchar*)hash);
		j_object_delete(chunk_obj, batch);
		j_object_unref(chunk_obj);
	}

	//TODO: ??
	if (!ret)
	{
	}
}

/**
 * Deletes an item from a collection.
 *
 * \code
 * \endcode
 *
 * \param collection A collection.
 * \param item       An item.
 * \param batch      A batch.
 **/
void
j_item_dedup_delete(JItemDedup* item, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	g_return_if_fail(item != NULL);
	g_return_if_fail(batch != NULL);

	j_item_refresh_hashes(item, j_batch_get_semantics(batch));

	j_kv_delete(item->kv, batch);
	j_kv_delete(item->kv_h, batch);

	for (guint64 i = 0; i < item->hashes->len; i++)
	{
		gchar* hash = g_array_index(item->hashes, gchar*, i);
		printf("%s\n", hash);
		j_item_unref_chunk(hash, batch);
		g_array_remove_index(item->hashes, i);
		g_free(hash);
	}
}

bson_t*
j_item_serialize_hashes(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	bson_t* b;

	g_return_val_if_fail(item != NULL, NULL);

	b = bson_new();

	bson_append_int64(b, "len", -1, item->hashes->len);

	for (guint i = 0; i < item->hashes->len; i++)
	{
		gchar* key = g_strdup_printf("%d", i);
		bson_append_utf8(b, key, -1, g_array_index(item->hashes, gchar*, i), 64);
		g_free(key);
	}
	/*
	gchar* json = bson_as_canonical_extended_json(b, NULL);
	g_print("JSON read %s\n", json);
	bson_free(json);
    */
	return b;
}

void
j_item_deserialize_hashes(JItemDedup* item, bson_t const* b)
{
	J_TRACE_FUNCTION(NULL);
	bson_iter_t iter;

	guint len = 0;
	g_return_if_fail(item != NULL);
	g_return_if_fail(b != NULL);

	bson_iter_init_find(&iter, b, "len");
	if (BSON_ITER_HOLDS_INT64(&iter))
		len = bson_iter_int64(&iter);
	else
		g_print("len was not an int64\n");

	for (guint i = 0; i < len; i++)
	{
		gchar* key = g_strdup_printf("%d", i);
		bson_iter_find(&iter, key);
		g_free(key);
		if (BSON_ITER_HOLDS_UTF8(&iter))
		{
			guint slen = 0;
			const gchar* ohash = bson_iter_utf8(&iter, &slen);
			gchar* hash = g_strndup(ohash, slen);
			g_array_insert_val(item->hashes, i, hash);
		}
		else
		{
			g_print("hash #%d was not a string\n", i);
		}
	}
}

void
j_item_refresh_hashes(JItemDedup* item, JSemantics* semantics)
{
	//gchar* json;
	JBatch* sub_batch = j_batch_new(semantics);
	gpointer b;
	guint32 len;
	bson_t data[1];
	gboolean ret;
	j_kv_get(item->kv_h, &b, &len, sub_batch);
	ret = j_batch_execute(sub_batch);

	if (!ret)
	{
		return;
	}
	g_assert_nonnull(b);
	bson_init_static(data, b, len);
	/*
    json = bson_as_canonical_extended_json(data, NULL);
    g_print("JSON refreshed %s\n", json);
    bson_free(json);
    */
	//g_printerr("data->len: %d\n", data->len);
	if (bson_empty(data))
	{
		j_item_deserialize_hashes(item, data);
	}
	bson_destroy(data);
	g_free(b);

	if (!ret)
	{
		//FIXME
	}
}
/**
 * Reads an item.
 *
 * \code
 * \endcode
 *
 * \param item       An item.
 * \param data       A buffer to hold the read data.
 * \param length     Number of bytes to read.
 * \param offset     An offset within #item.
 * \param bytes_read Number of bytes read.
 * \param batch      A batch.
 **/
void
j_item_dedup_read(JItemDedup* item, gpointer data, guint64 length, guint64 offset, guint64* bytes_read, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	guint64 chunks, first_chunk, destination_relative;
	JObject* chunk_obj;

	g_return_if_fail(item != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(bytes_read != NULL);

	first_chunk = offset / item->chunk_size;
	//FIXME test all cases
	chunks = (length + offset) / item->chunk_size;
	if ((length + offset) % item->chunk_size > 0)
		chunks++;
	destination_relative = 0;

	j_item_refresh_hashes(item, j_batch_get_semantics(batch));

	for (guint64 chunk = first_chunk; chunk < chunks; chunk++)
	{
		guint64 from, to, part;
		const gchar* hash = g_array_index(item->hashes, gchar*, chunk);
		//printf("Read Hash: %s\n", hash);
		chunk_obj = j_object_new("chunks", hash);
		j_object_create(chunk_obj, batch);
		from = 0;
		to = item->chunk_size;

		if (chunk == first_chunk)
		{
			from = offset % item->chunk_size;
		}

		if (chunk == chunks - 1)
		{
			to = item->chunk_size - (chunks * item->chunk_size - offset - length);
			if (to <= 0)
			{
				to = item->chunk_size;
			}
		}
		part = to - from;
		//printf("From: %ld | To: %ld | Length: %ld\n", from, to, part);
		j_object_read(chunk_obj, (gchar*)data + destination_relative, part, from, bytes_read, batch);
		destination_relative += part;
	}
}

/**
 * Writes an item.
 *
 * \note
 * j_item_dedup_write() modifies bytes_written even if j_batch_execute() is not called.
 *
 * \code
 * \endcode
 *
 * \param item          An item.
 * \param data          A buffer holding the data to write.
 * \param length        Number of bytes to write.
 * \param offset        An offset within #item.
 * \param bytes_written Number of bytes written.
 * \param batch         A batch.
 **/
void
j_item_dedup_write(JItemDedup* item, gconstpointer data, guint64 length, guint64 offset, guint64* bytes_written, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	guint64 first_chunk, chunk_offset, chunks, old_chunks, hash_len, remaining, bytes_read;
	GArray* hashes;
	gpointer first_buf = NULL, last_buf = NULL;
	JObject *first_obj, *last_obj, *chunk_obj;
	JKV* chunk_kv;
	bson_t* new_ref_bson;
	bson_t* serializes_bson;
	JBatch* sub_batch;
	void* hash_context;
	int hash_choice;
	gboolean ret;
	//guint64 len;
	guint64 last_chunk;

	g_return_if_fail(item != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(bytes_written != NULL);

	hash_choice = J_HASH_SHA256;
	// refresh chunks before write
	j_item_refresh_hashes(item, j_batch_get_semantics(batch));

	// needs to be modified for non static hashing
	first_chunk = offset / item->chunk_size;
	// Chunk offset of the very first chunk
	chunk_offset = offset % item->chunk_size;
	chunks = length / item->chunk_size;
	if ((length % item->chunk_size) > 0)
		chunks++;
	last_chunk = first_chunk + chunks - 1; // might be unecesarry
	remaining = chunks * item->chunk_size - chunk_offset - length;
	//len = item->chunk_size - chunk_offset - (chunks * item->chunk_size - chunk_offset - length);
	//len = item->chunk_size - chunk_offset - remaining; // INFO: +1?

	// TODO: ???????
	//if(item->chunk_size % len != 0){
		//++len;
	//}

	printf("\nContent: %s", data);
	printf("\nlast_chunk: %ld\n", last_chunk);
	printf("Chunk Size: %ld\n", item->chunk_size);
	printf("First_chunk: %ld\n", first_chunk);
	printf("Offset: %ld\n", offset);
	printf("Chunk Offset: %ld\n", chunk_offset);
	printf("Chunks: %ld\n", chunks);
	printf("remaining: %ld\n", remaining);
	printf("Length: %ld\n", length);
	//printf("Len: %ld\n", len);
	printf("Hash Len: %d\n", item->hashes->len);

	printf("chunks:%ld\n", chunks);
	hash_len = g_array_get_element_size(item->hashes);

	old_chunks = MIN(chunks, item->hashes->len - first_chunk);
	printf("old_chunks: %d\n", old_chunks);
	/*
	hashes = g_array_sized_new(FALSE, TRUE, hash_len, old_chunks);
	g_array_insert_vals(hashes, 0, item->hashes->data, old_chunks * hash_len);
	printf("g_array len: %d\n", hashes->len);
	*/
	sub_batch = j_batch_new(j_batch_get_semantics(batch));

	// get old_chunks:
	// first_chunk if chunk_offset nonzero
	// last_chunk if remaining is nonzero
	if (chunk_offset > 0 && old_chunks > 0)
	{
		// get offset part of first_chunk
		first_buf = g_slice_alloc(chunk_offset);
		first_obj = j_object_new("chunks", g_array_index(item->hashes, gchar*, first_chunk));
		j_object_create(first_obj, sub_batch);
		j_object_read(first_obj, first_buf, chunk_offset, 0, &bytes_read, sub_batch);
		
	}
	else if (chunk_offset > 0)
	{
		first_buf = g_slice_alloc0(chunk_offset);
	}

	if (remaining > 0 && old_chunks == chunks)
	{
		// get remaining part of last_chunk
		last_buf = g_slice_alloc(remaining);
		last_obj = j_object_new("chunks", g_array_index(item->hashes, gchar*, last_chunk));
		j_object_create(last_obj, sub_batch);
		j_object_read(last_obj, last_buf, remaining, item->chunk_size - remaining, &bytes_read, sub_batch);
		
	}
	else if (remaining > 0)
	{
		last_buf = g_slice_alloc0(remaining);
	}

	ret = j_batch_execute(sub_batch);
	printf("first_buf: %s\n", first_buf);
	printf("last_buf: %s\n", last_buf);

	hash_context = algo_array[hash_choice]->create_context();

	for (guint64 chunk = 0; chunk < chunks; chunk++)
	{
		gchar* hash;
		guint32 refcount = 0;
		guint64 data_offset = chunk * item->chunk_size;
		guint64 len = item->chunk_size - chunk_offset - remaining;

		//EVP_DigestInit_ex(hash_context, EVP_sha256(), NULL);
		algo_array[hash_choice]->init(hash_context);

		if (chunk == 0)
		{
			//EVP_DigestUpdate(hash_context, first_buf, chunk_offset);
			algo_array[hash_choice]->update(hash_context, first_buf, chunk_offset);
			//EVP_DigestUpdate(hash_context, data, item->chunk_size - chunk_offset);
	
		}

		//EVP_DigestUpdate(hash_context, (const gchar*)data + data_offset, len);
		algo_array[hash_choice]->update(hash_context, (const gchar*)data + data_offset, len);

		if (chunk == chunks - 1)
		{
			//EVP_DigestUpdate(hash_context, (const gchar*)data + chunk * item->chunk_size, item->chunk_size - remaining);
			//EVP_DigestUpdate(hash_context, last_buf, remaining);
			algo_array[hash_choice]->update(hash_context, last_buf, remaining);
		}
		//EVP_DigestFinal_ex(hash_context, hash_gen, &md_len);
		algo_array[hash_choice]->finalize(hash_context, &hash);

		chunk_kv = j_kv_new("chunk_refs", (const gchar*)hash);
		j_kv_get_callback(chunk_kv, j_item_hash_ref_callback, &refcount, sub_batch);
		ret = j_batch_execute(sub_batch);

		if (refcount == 0)
		{
			printf("Write Hash: %s\n", hash);
			chunk_obj = j_object_new("chunks", (const gchar*)hash);
			j_object_create(chunk_obj, batch);

			if (chunk == 0 && chunk_offset > 0)
			{
				printf("stuff 1: %s\n", first_buf);
				j_object_write(chunk_obj, first_buf, chunk_offset, 0, bytes_written, batch);
				//j_object_write(chunk_obj, data, item->chunk_size - chunk_offset, chunk_offset, bytes_written, batch);
			}

			j_object_write(chunk_obj, (const gchar*)data + data_offset, len, chunk_offset, bytes_written, batch);

			if (chunk == chunks - 1 && remaining > 0) // && remaining>len
			{
				//j_object_write(chunk_obj, (const gchar*)data + chunk * item->chunk_size, item->chunk_size - remaining, 0, bytes_written, batch);
				j_object_write(chunk_obj, last_buf, remaining, item->chunk_size - remaining, bytes_written, batch);
			}
		}
		/*
		//FIXME
		else
		{
			if (chunk == 0 && chunk_offset > 0)
			{
				j_helper_atomic_add(bytes_written, chunk_offset);
			}

			j_helper_atomic_add(bytes_written, len);

			if (chunk == chunks - 1 && remaining > 0)
			{
				j_helper_atomic_add(bytes_written, remaining);
			}
		}
		*/
		new_ref_bson = bson_new();
		bson_append_int32(new_ref_bson, "ref", -1, refcount + 1);

		{
			gpointer value;
			guint32 value_len;
			value = bson_destroy_with_steal(new_ref_bson, TRUE, &value_len);

			j_kv_put(chunk_kv, value, value_len, bson_free, sub_batch);
		}
		ret = j_batch_execute(sub_batch);

		if (chunk < old_chunks)
		{
			gchar* old_hash = g_array_index(item->hashes, gchar*, first_chunk + chunk);
			if (g_strcmp0(old_hash, (gchar*)hash) != 0)
			{
				j_item_unref_chunk(old_hash, batch);
				g_array_remove_index(item->hashes, first_chunk + chunk);
				g_free(old_hash);
			}
		}

		g_array_insert_val(item->hashes, first_chunk + chunk, hash);
	}
	//EVP_MD_CTX_destroy(hash_context);
	algo_array[hash_choice]->destroy(hash_context);

	j_kv_delete(item->kv_h, sub_batch);
	serializes_bson = j_item_serialize_hashes(item);

	{
		gpointer value;
		guint32 value_len;
		value = bson_destroy_with_steal(serializes_bson, TRUE, &value_len);
		j_kv_put(item->kv_h, value, value_len, bson_free, sub_batch);
		ret = j_batch_execute(sub_batch);
	}

	//TODO: ??
	if (!ret)
	{
	}
}

/**
 * Get the status of an item.
 *
 * \code
 * \endcode
 *
 * \param item      An item.
 * \param batch     A batch.
 **/
void
j_item_dedup_get_status(JItemDedup* item, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	g_return_if_fail(item != NULL);

	// TODO: find a meaningful way to do this for chunks
	//j_object_status(item->object, &(item->status.modification_time), &(item->status.size), batch);
	(void)batch;
}

/**
 * Returns an item's size.
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return A size.
 **/
guint64
j_item_dedup_get_size(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);
	g_return_val_if_fail(item != NULL, 0);

	return item->status.size;
}

/**
 * Returns an item's physical size.
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return A size.
 **/
guint64
j_item_dedup_get_size_physical(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);
	GHashTable* table;
	guint size;
	g_return_val_if_fail(item != NULL, 0);

	table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (guint i = 0; i < item->hashes->len; ++i)
	{
		gchar* hash = g_array_index(item->hashes, gchar*, i);
		g_hash_table_add(table, g_strdup(hash));
	}

	size = g_hash_table_size(table) * item->chunk_size;
	g_hash_table_destroy(table);
	return size;
}

/**
 * Returns an item's modification time.
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return A modification time.
 **/
gint64
j_item_dedup_get_modification_time(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	g_return_val_if_fail(item != NULL, 0);

	return item->status.modification_time;
}

/**
 * Returns the item's optimal access size.
 *
 * \code
 * JItemDedup* item;
 * guint64 optimal_size;
 *
 * ...
 * optimal_size = j_item_dedup_get_optimal_access_size(item);
 * j_item_write(item, buffer, optimal_size, 0, &dummy, batch);
 * ...
 * \endcode
 *
 * \param item An item.
 *
 * \return An access size.
 */
guint64
j_item_dedup_get_optimal_access_size(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	g_return_val_if_fail(item != NULL, 0);

	return 512 * 1024;
}

/* Internal */

/**
 * Creates a new item.
 *
 * \code
 * JItemDedup* i;
 *
 * i = j_item_dedup_new("JULEA");
 * \endcode
 *
 * \param collection   A collection.
 * \param name         An item name.
 *
 * \return A new item. Should be freed with j_item_unref().
 **/
JItemDedup*
j_item_dedup_new(JCollection* collection, gchar const* name, JDistribution* distribution)
{
	J_TRACE_FUNCTION(NULL);

	JItemDedup* item = NULL;
	g_autofree gchar* path = NULL;

	g_return_val_if_fail(collection != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	if (strpbrk(name, "/") != NULL)
	{
		return NULL;
	}

	if (distribution == NULL)
	{
		distribution = j_distribution_new(J_DISTRIBUTION_ROUND_ROBIN);
	}

	item = g_slice_new(JItemDedup);
	bson_oid_init(&(item->id), bson_context_get_default());
	item->name = g_strdup(name);
	item->credentials = j_credentials_new();
	item->distribution = distribution;
	item->status.age = g_get_real_time();
	item->status.size = 0;
	item->status.modification_time = g_get_real_time();
	item->collection = j_collection_ref(collection);
	item->ref_count = 1;

	item->hashes = g_array_new(FALSE, FALSE, sizeof(guchar*));
	item->chunk_size = 1024; // small size for testing only

	path = g_build_path("/", j_collection_get_name(item->collection), item->name, NULL);
	item->kv = j_kv_new("items", path);
	item->kv_h = j_kv_new("item_hashes", path);
	return item;
}

void
j_item_dedup_set_chunk_size(JItemDedup* item, guint64 chunk_size)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(chunk_size > 0);
	// TODO: Only allowed to change if item is new
	item->chunk_size = chunk_size;
}

/**
 * Creates a new item from a BSON object.
 *
 * \private
 *
 * \code
 * \endcode
 *
 * \param collection A collection.
 * \param b          A BSON object.
 *
 * \return A new item. Should be freed with j_item_unref().
 **/
JItemDedup*
j_item_dedup_new_from_bson(JCollection* collection, bson_t const* b)
{
	J_TRACE_FUNCTION(NULL);

	JItemDedup* item;
	g_autofree gchar* path = NULL;

	g_return_val_if_fail(collection != NULL, NULL);
	g_return_val_if_fail(b != NULL, NULL);

	item = g_slice_new(JItemDedup);
	item->name = NULL;
	item->credentials = j_credentials_new();
	item->status.age = 0;
	item->status.size = 0;
	item->status.modification_time = 0;
	item->collection = j_collection_ref(collection);
	item->ref_count = 1;

	j_item_dedup_deserialize(item, b);

	path = g_build_path("/", j_collection_get_name(item->collection), item->name, NULL);
	item->kv = j_kv_new("items", path);
	item->kv_h = j_kv_new("item_hashes", path);

	return item;
}

/**
 * Returns an item's collection.
 *
 * \private
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return A collection.
 **/
JCollection*
j_item_dedup_get_collection(JItemDedup* item)
{
	g_return_val_if_fail(item != NULL, NULL);

	return item->collection;
}

/**
 * Returns an item's credentials.
 *
 * \private
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return A collection.
 **/
JCredentials*
j_item_dedup_get_credentials(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	g_return_val_if_fail(item != NULL, NULL);

	return item->credentials;
}

/**
 * Serializes an item.
 *
 * \private
 *
 * \code
 * \endcode
 *
 * \param item      An item.
 * \param semantics A semantics object.
 *
 * \return A new BSON object. Should be freed with g_slice_free().
 **/
bson_t*
j_item_dedup_serialize(JItemDedup* item, JSemantics* semantics)
{
	J_TRACE_FUNCTION(NULL);

	bson_t* b;
	bson_t* b_cred;
	bson_t* b_distribution;

	g_return_val_if_fail(item != NULL, NULL);

	b = bson_new();
	b_cred = j_credentials_serialize(item->credentials);
	b_distribution = j_distribution_serialize(item->distribution);

	bson_append_oid(b, "_id", -1, &(item->id));
	bson_append_oid(b, "collection", -1, j_collection_get_id(item->collection));
	bson_append_utf8(b, "name", -1, item->name, -1);

	if (j_semantics_get(semantics, J_SEMANTICS_CONCURRENCY) == J_SEMANTICS_CONCURRENCY_NONE)
	{
		bson_t b_document[1];

		bson_append_document_begin(b, "status", -1, b_document);

		bson_append_int64(b_document, "size", -1, item->status.size);
		bson_append_int64(b_document, "modification_time", -1, item->status.modification_time);

		bson_append_document_end(b, b_document);

		bson_destroy(b_document);
	}

	bson_append_document(b, "credentials", -1, b_cred);
	bson_append_document(b, "distribution", -1, b_distribution);

	//bson_finish(b);

	bson_destroy(b_cred);
	bson_destroy(b_distribution);

	return b;
}

static void
j_item_deserialize_status(JItemDedup* item, bson_t const* b)
{
	J_TRACE_FUNCTION(NULL);

	bson_iter_t iterator;

	g_return_if_fail(item != NULL);
	g_return_if_fail(b != NULL);

	bson_iter_init(&iterator, b);

	while (bson_iter_next(&iterator))
	{
		gchar const* key;

		key = bson_iter_key(&iterator);

		if (g_strcmp0(key, "size") == 0)
		{
			item->status.size = bson_iter_int64(&iterator);
			item->status.age = g_get_real_time();
		}
		else if (g_strcmp0(key, "modification_time") == 0)
		{
			item->status.modification_time = bson_iter_int64(&iterator);
			item->status.age = g_get_real_time();
		}
	}
}

/**
 * Deserializes an item.
 *
 * \param item An item.
 * \param b    A BSON object.
 **/
void
j_item_dedup_deserialize(JItemDedup* item, bson_t const* b)
{
	J_TRACE_FUNCTION(NULL);

	bson_iter_t iterator;

	g_return_if_fail(item != NULL);
	g_return_if_fail(b != NULL);

	bson_iter_init(&iterator, b);

	while (bson_iter_next(&iterator))
	{
		gchar const* key;

		key = bson_iter_key(&iterator);

		if (g_strcmp0(key, "_id") == 0)
		{
			item->id = *bson_iter_oid(&iterator);
		}
		else if (g_strcmp0(key, "name") == 0)
		{
			g_free(item->name);
			item->name = g_strdup(bson_iter_utf8(&iterator, NULL /*FIXME*/));
		}
		else if (g_strcmp0(key, "status") == 0)
		{
			guint8 const* data;
			guint32 len;
			bson_t b_status[1];

			bson_iter_document(&iterator, &len, &data);
			bson_init_static(b_status, data, len);
			j_item_deserialize_status(item, b_status);
			bson_destroy(b_status);
		}
		else if (g_strcmp0(key, "credentials") == 0)
		{
			guint8 const* data;
			guint32 len;
			bson_t b_cred[1];

			bson_iter_document(&iterator, &len, &data);
			bson_init_static(b_cred, data, len);
			j_credentials_deserialize(item->credentials, b_cred);
			bson_destroy(b_cred);
		}
		else if (g_strcmp0(key, "distribution") == 0)
		{
			guint8 const* data;
			guint32 len;
			bson_t b_distribution[1];

			if (item->distribution != NULL)
			{
				j_distribution_unref(item->distribution);
			}

			bson_iter_document(&iterator, &len, &data);
			bson_init_static(b_distribution, data, len);
			item->distribution = j_distribution_new_from_bson(b_distribution);
			bson_destroy(b_distribution);
		}
	}
}

/**
 * Returns an item's ID.
 *
 * \private
 *
 * \code
 * \endcode
 *
 * \param item An item.
 *
 * \return An ID.
 **/
bson_oid_t const*
j_item_dedup_get_id(JItemDedup* item)
{
	J_TRACE_FUNCTION(NULL);

	g_return_val_if_fail(item != NULL, NULL);

	return &(item->id);
}

/**
 * Sets an item's modification time.
 *
 * \code
 * \endcode
 *
 * \param item              An item.
 * \param modification_time A modification time.
 **/
void
j_item_dedup_set_modification_time(JItemDedup* item, gint64 modification_time)
{
	J_TRACE_FUNCTION(NULL);

	g_return_if_fail(item != NULL);

	item->status.age = g_get_real_time();
	item->status.modification_time = MAX(item->status.modification_time, modification_time);
}

/**
 * Sets an item's size.
 *
 * \code
 * \endcode
 *
 * \param item An item.
 * \param size A size.
 **/
void
j_item_dedup_set_size(JItemDedup* item, guint64 size)
{
	J_TRACE_FUNCTION(NULL);

	g_return_if_fail(item != NULL);

	item->status.age = g_get_real_time();
	item->status.size = size;
}