/*
 * jsqlite3.h
 *
 *  Created on: Dec 6, 2017
 *      Author: pchero
 */

#ifndef _JSQLITE3_H_
#define _JSQLITE3_H_

#include <sqlite3.h>
#include <stdbool.h>
#include <jansson.h>

typedef struct _jsql_ctx_t
{
  struct sqlite3* db;

  struct sqlite3_stmt* stmt;
} jsql_ctx_t;

jsql_ctx_t* jsql_ctx_init(const char* name);
void jsql_ctx_term(jsql_ctx_t* ctx);

bool jsql_ctx_exec(jsql_ctx_t* ctx, const char* query);
bool jsql_ctx_query(jsql_ctx_t* ctx, const char* query);
json_t* jsql_ctx_get_record(jsql_ctx_t* ctx);

bool jsql_ctx_insert(jsql_ctx_t* ctx, const char* table, const json_t* j_data);
bool jsql_ctx_insert_or_replace(jsql_ctx_t* ctx, const char* table, const json_t* j_data);

char* jsql_ctx_get_update_str(const json_t* j_data);

bool jsql_ctx_free(jsql_ctx_t* ctx);


#endif /* _JSQLITE3_H_ */
