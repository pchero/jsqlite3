
/*
 * jsqlite3.c
 *
 *  Created on: Dec 6, 2017
 *      Author: pchero
 */

#define _GNU_SOURCE

#include "jsqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define jfree(p) { if(p != NULL) free(p); p=NULL; }

typedef struct {
  int max_retry;  /* Max retry times. */
  int sleep_ms;   /* Time to sleep before retry again. */
} busy_handler_attr;


static bool jsql_ctx_connect(jsql_ctx_t* ctx, const char* filename);
static jsql_ctx_t* jsql_ctx_create(void);
static int jsql_ctx_busy_handler(void *data, int retry);
static bool jsql_ctx_insert_basic(jsql_ctx_t* ctx, const char* table, const json_t* j_data, int replace);


static jsql_ctx_t* jsql_ctx_create(void)
{
  jsql_ctx_t* ctx;

  ctx = calloc(1, sizeof(jsql_ctx_t));
  if(ctx == NULL) {
    return NULL;
  }
  ctx->db = NULL;
  ctx->stmt = NULL;

  return ctx;
}

/**
 Connect to db.

 @return Success:TRUE, Fail:FALSE
 */
static bool jsql_ctx_connect(jsql_ctx_t* ctx, const char* filename)
{
  int ret;

  if((ctx == NULL) || (filename == NULL)) {
    // wrong paramenter
    return false;
  }

  if(ctx->db != NULL) {
    // database is already connected.
    return true;
  }

  ret = sqlite3_open(filename, &ctx->db);
  if(ret != SQLITE_OK) {
    //printf("Could not initiate database. err[%s]\n", sqlite3_errmsg(ctx->db));
    return false;
  }
  //printf("Connected to database ctx. filename[%s]\n", filename);

  return true;
}

/**
 * free ctx stmt
 * @param ctx
 */
bool jsql_ctx_free(jsql_ctx_t* ctx)
{
  int ret;

  if(ctx == NULL) {
    //printf("Wrong input parameter.\n");
    return false;
  }

  // check already freed
  if(ctx->stmt == NULL) {
    return true;
  }

  ret = sqlite3_finalize(ctx->stmt);
  if(ret != SQLITE_OK) {
    //printf("Could not finalize stme. ret[%d]\n", ret);
    return false;
  }
  ctx->stmt = NULL;

  return true;
}

/*
 * Busy callback handler for all operations. If the busy callback handler is
 * NULL, then SQLITE_BUSY or SQLITE_IOERR_BLOCKED is returned immediately upon
 * encountering the lock. If the busy callback is not NULL, then the callback
 * might be invoked.
 *
 * This callback will be registered by SQLite's API:
 *    int sqlite3_busy_handler(sqlite3*, int(*)(void*,int), void*);
 * That's very useful to deal with SQLITE_BUSY event automatically. Otherwise,
 * you have to check the return code, reset statement and do retry manually.
 *
 * We've to use ANSI C declaration here to eliminate warnings in Visual Studio.
 */
static int jsql_ctx_busy_handler(void *data, int retry)
{
  busy_handler_attr* attr;

  attr = (busy_handler_attr*)data;

  if (retry < attr->max_retry) {
    /* Sleep a while and retry again. */
    printf("Hits SQLITE_BUSY %d times, retry again.\n", retry);
    sqlite3_sleep(attr->sleep_ms);

    /* Return non-zero to let caller retry again. */
    return 1;
  }

  /* Return zero to let caller return SQLITE_BUSY immediately. */
  printf("Retried too many, exit. retry[%d]\n", retry);
  return 0;
}


/**
 * Init database
 * @return
 */
jsql_ctx_t* jsql_ctx_init(const char* name)
{
  int ret;
  jsql_ctx_t* jsql_ctx;

  if(name == NULL) {
//    printf("Wrong input parameter.\n");
    return NULL;
  }
//  printf("Initiate db context. name[%s]\n", name);

  // create new ctx
  jsql_ctx = jsql_ctx_create();
  if(jsql_ctx == NULL) {
//    printf("Could not create db context.\n");
    return NULL;
  }

  // connect db
  ret = jsql_ctx_connect(jsql_ctx, name);
  if(ret == false) {
    jfree(jsql_ctx);
    return NULL;
  }

  return jsql_ctx;
}

/**
 Disconnect to db.
 */
void jsql_ctx_term(jsql_ctx_t* ctx)
{
  int ret;

  if(ctx == NULL) {
//    printf("Wrong input parameter.\n");
    return;
  }
  jsql_ctx_free(ctx);

  ret = sqlite3_close(ctx->db);
  if(ret != SQLITE_OK) {
    printf("Could not close the database correctly. err[%s]\n", sqlite3_errmsg(ctx->db));
    return;
  }

//  printf("Released database context.\n");
  ctx->db = NULL;

  jfree(ctx);
}

/**
 database query function. (select)
 @param query
 @return Success:, Fail:NULL
 */
bool jsql_ctx_query(jsql_ctx_t* ctx, const char* query)
{
  int ret;
  sqlite3_stmt* result;

  if((ctx == NULL) || (query == NULL) || (ctx->db == NULL)) {
    //printf("Wrong input parameter.");
    return false;
  }

  // free ctx stmt if exists
  jsql_ctx_free(ctx);

  ret = sqlite3_prepare_v2(ctx->db, query, -1, &result, NULL);
  if(ret != SQLITE_OK) {
    //printf("Could not prepare query. query[%s], err[%s]", query, sqlite3_errmsg(ctx->db));
    return false;
  }

  ctx->stmt = result;
  return true;
}

/**
 * database query execute function. (update, delete, insert)
 * @param query
 * @return  success:true, fail:false
 */
bool jsql_ctx_exec(jsql_ctx_t* ctx, const char* query)
{
  int ret;
  char* err;
  busy_handler_attr bh_attr;

  if((ctx == NULL) || (query == NULL) || (ctx->db == NULL)) {
    //printf("Wrong input parameter.");
    return false;
  }

  /* Setup busy handler for all following operations. */
  bh_attr.max_retry = 100;  /* Max retry times */
  bh_attr.sleep_ms = 100;   /* Sleep 100ms before each retry */
  sqlite3_busy_handler(ctx->db, jsql_ctx_busy_handler, &bh_attr);

  // execute
  ret = sqlite3_exec(ctx->db, query, NULL, 0, &err);
  if(ret != SQLITE_OK) {
    //printf("Could not execute query. query[%s], err[%s]", query, err);
    sqlite3_free(err);
    return false;
  }
  sqlite3_free(err);

  return true;
}

/**
 * Return 1 record info by json.
 * If there's no more record or error happened, it will return NULL.
 * @param res
 * @return  success:json_t*, fail:NULL
 */
json_t* jsql_ctx_get_record(jsql_ctx_t* ctx)
{
	int ret;
	int cols;
	int i;
	json_t* j_res;
	json_t* j_tmp;
	int type;
	const char* tmp_const;

  if(ctx == NULL) {
    //printf("Wrong input parameter.");
    return NULL;
  }

	ret = sqlite3_step(ctx->stmt);
	if(ret != SQLITE_ROW) {
		if(ret != SQLITE_DONE) {
		  //printf("Could not patch the result. ret[%d], err[%s]", ret, sqlite3_errmsg(ctx->db));
		}
		return NULL;
	}

	cols = sqlite3_column_count(ctx->stmt);
	j_res = json_object();
	for(i = 0; i < cols; i++) {
		j_tmp = NULL;
		type = sqlite3_column_type(ctx->stmt, i);
		switch(type) {
			case SQLITE_INTEGER: {
				j_tmp = json_integer(sqlite3_column_int(ctx->stmt, i));
			}
			break;

			case SQLITE_FLOAT: {
				j_tmp = json_real(sqlite3_column_double(ctx->stmt, i));
			}
			break;

			case SQLITE_NULL: {
				j_tmp = json_null();
			}
			break;

			case SQLITE3_TEXT:
			{
			  // if the text is loadable, create json object.
			  tmp_const = (const char*)sqlite3_column_text(ctx->stmt, i);
			  if(tmp_const == NULL) {
			    j_tmp = json_null();
			  }
			  else {
	        j_tmp = json_loads(tmp_const, JSON_DECODE_ANY, NULL);
	        if(j_tmp == NULL) {
	          j_tmp = json_string((const char*)sqlite3_column_text(ctx->stmt, i));
	        }
	        else {
	          // check type
            // the only array/object/string types are allowed
	          // especially, we don't allow the JSON_NULL type at this point.
	          // Cause the json_loads() consider the "null" string to JSON_NULL.
	          // It's should be done at the above.
            ret = json_typeof(j_tmp);
            if((ret != JSON_ARRAY) && (ret != JSON_OBJECT) && (ret != JSON_STRING)) {
              json_decref(j_tmp);
              j_tmp = json_string((const char*)sqlite3_column_text(ctx->stmt, i));
            }
	        }
			  }
			}
			break;

			case SQLITE_BLOB:
			default:
			{
				// not done yet.
			  //printf("Not supported type. type[%d]", type);
				j_tmp = json_null();
			}
			break;
		}

		if(j_tmp == NULL) {
		  //printf("Could not parse result column. name[%s], type[%d]", sqlite3_column_name(ctx->stmt, i), type);
			j_tmp = json_null();
		}
		json_object_set_new(j_res, sqlite3_column_name(ctx->stmt, i), j_tmp);
	}

	return j_res;
}

/**
 * Insert j_data into table.
 * @param table
 * @param j_data
 * @return
 */
bool jsql_ctx_insert(jsql_ctx_t* ctx, const char* table, const json_t* j_data)
{
  int ret;

  if((ctx == NULL) || (table == NULL) || (j_data == NULL)) {
    //printf("Wrong input parameter.");
    return false;
  }
  //printf("Fired db_ctx_insert");

  ret = jsql_ctx_insert_basic(ctx, table, j_data, false);
  if(ret == false) {
    //printf("Could not insert data.");
    return false;
  }

  return true;
}

/**
 * Insert or replace j_data into table.
 * @param table
 * @param j_data
 * @return
 */
bool db_ctx_insert_or_replace(jsql_ctx_t* ctx, const char* table, const json_t* j_data)
{
  int ret;

  if((ctx == NULL) || (ctx->db == NULL) || (table == NULL) || (j_data == NULL)) {
    //printf("Wrong input parameter.");
    return false;
  }
  //printf("Fired db_ctx_insert_or_replace");

  ret = jsql_ctx_insert_basic(ctx, table, j_data, true);
  if(ret == false) {
    //printf("Could not insert data.");
    return false;
  }

  return true;
}

/**
 * Insert j_data into table.
 * @param table
 * @param j_data
 * @return
 */
static bool jsql_ctx_insert_basic(jsql_ctx_t* ctx, const char* table, const json_t* j_data, int replace)
{
  char* sql;
  json_t*     j_data_cp;
  char*       tmp;
  const char* key;
  json_t*     j_val;
  int         ret;
  json_type   type;
  char*       sql_keys;
  char*       sql_values;
  char*       tmp_sub;
  char*       tmp_sqlite_buf; // sqlite3_mprintf

  if((ctx == NULL) || (ctx->db == NULL) || (table == NULL) || (j_data == NULL)) {
    //printf("Wrong input parameter.");
    return false;
  }
  //printf("jsql_ctx_insert_basic.");

  // copy original.
  j_data_cp = json_deep_copy(j_data);

  tmp = NULL;
  sql_keys  = NULL;
  sql_values = NULL;
  tmp_sub = NULL;
  json_object_foreach(j_data_cp, key, j_val) {

    // key
    if(sql_keys == NULL) {
      asprintf(&tmp, "%s", key);
    }
    else {
      asprintf(&tmp, "%s, %s", sql_keys, key);
    }

    jfree(sql_keys);
    asprintf(&sql_keys, "%s", tmp);
    jfree(tmp);

    // value
    jfree(tmp_sub);

    // get type.
    type = json_typeof(j_val);
    switch(type) {
      // string
      case JSON_STRING: {
        asprintf(&tmp_sub, "\'%s\'", json_string_value(j_val));
      }
      break;

      // numbers
      case JSON_INTEGER: {
        asprintf(&tmp_sub, "%lld", json_integer_value(j_val));
      }
      break;

      case JSON_REAL: {
        asprintf(&tmp_sub, "%f", json_real_value(j_val));
      }
      break;

      // true
      case JSON_TRUE: {
        asprintf(&tmp_sub, "\"%s\"", "true");
      }
      break;

      // false
      case JSON_FALSE: {
        asprintf(&tmp_sub, "\"%s\"", "false");
      }
      break;

      case JSON_NULL: {
        asprintf(&tmp_sub, "%s", "null");
      }
      break;

      case JSON_ARRAY:
      case JSON_OBJECT: {
        tmp = json_dumps(j_val, JSON_ENCODE_ANY);
        tmp_sqlite_buf = sqlite3_mprintf("%q", tmp);
        jfree(tmp);

        asprintf(&tmp_sub, "'%s'", tmp_sqlite_buf);
        sqlite3_free(tmp_sqlite_buf);
      }
      break;

      default: {
        // we don't support another types.
        //printf("Wrong type input. We don't handle this.");
        asprintf(&tmp_sub, "\"%s\"", "null");
      }
      break;
    }

    if(sql_values == NULL) {
      asprintf(&tmp, "%s", tmp_sub);
    }
    else {
      asprintf(&tmp, "%s, %s", sql_values, tmp_sub);
    }

    jfree(tmp_sub);
    jfree(sql_values);

    sql_values = strdup(tmp);
    jfree(tmp);
  }
  json_decref(j_data_cp);

  if(replace == true) {
    asprintf(&sql, "insert or replace into %s(%s) values (%s);", table, sql_keys, sql_values);
  }
  else {
    asprintf(&sql, "insert into %s(%s) values (%s);", table, sql_keys, sql_values);
  }

  jfree(sql_keys);
  jfree(sql_values);

  ret = jsql_ctx_exec(ctx, sql);
  jfree(sql);
  if(ret == false) {
    //printf("Could not insert data.");
    return false;
  }

  return true;
}

/**
 * Return part of update sql.
 * @param j_data
 * @return
 */
char* jsql_ctx_get_update_str(const json_t* j_data)
{
  char*   res;
  char*   tmp;
  char*   tmp_sub;
  char*   tmp_sqlite_buf;
  json_t*  j_val;
  json_t*  j_data_cp;
  const char* key;
  bool        is_first;
  json_type   type;

  // copy original data.
  j_data_cp = json_deep_copy(j_data);

  is_first = true;
  res = NULL;
  tmp = NULL;
  tmp_sub = NULL;

  json_object_foreach(j_data_cp, key, j_val) {

    // create update string
    type = json_typeof(j_val);
    switch(type) {
      // string
      case JSON_STRING: {
        asprintf(&tmp_sub, "%s = \'%s\'", key, json_string_value(j_val));
      }
      break;

      // numbers
      case JSON_INTEGER: {
        asprintf(&tmp_sub, "%s = %lld", key, json_integer_value(j_val));
      }
      break;

      case JSON_REAL: {
        asprintf(&tmp_sub, "%s = %lf", key, json_real_value(j_val));
      }
      break;

      // true
      case JSON_TRUE: {
        asprintf(&tmp_sub, "%s = \"%s\"", key, "true");
      }
      break;

      // false
      case JSON_FALSE: {
        asprintf(&tmp_sub, "%s = \"%s\"", key, "false");
      }
      break;

      case JSON_NULL: {
        asprintf(&tmp_sub, "%s = %s", key, "null");
      }
      break;

      case JSON_ARRAY:
      case JSON_OBJECT: {
        tmp = json_dumps(j_val, JSON_ENCODE_ANY);
        tmp_sqlite_buf = sqlite3_mprintf("%q", tmp);
        jfree(tmp);

        asprintf(&tmp_sub, "%s = '%s'", key, tmp_sqlite_buf);
        sqlite3_free(tmp_sqlite_buf);
      }
      break;

      default: {
        // Not done yet.
        // we don't support another types.
        //printf("Wrong type input. We don't handle this.");
        asprintf(&tmp_sub, "%s = %s", key, "null");
      }
      break;
    }

    // copy/set previous sql.
    jfree(tmp);
    if(is_first == true) {
      asprintf(&tmp, "%s", tmp_sub);
      is_first = false;
    }
    else {
      asprintf(&tmp, "%s, %s", res, tmp_sub);
    }
    jfree(res);
    jfree(tmp_sub);

    res = strdup(tmp);
    jfree(tmp);
  }
  json_decref(j_data_cp);

  return res;
}

