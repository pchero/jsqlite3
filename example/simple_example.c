
#include "../jsqlite3.h"

int main(int argc, char** argv)
{
  jsql_ctx_t* j_ctx;
  const char* sql;
  char* res;
  int ret;
  json_t* j_tmp;
  json_t* j_res;

  j_ctx = jsql_ctx_init("test.db");
  if(j_ctx == NULL) {
    printf("Could not initiate db.\n");
    return 1;
  }
  printf("Finished db_init.\n");

  // drop table
  sql = "drop table if exists simple_example;";
  jsql_ctx_exec(j_ctx, sql);

  // create table
  sql = "create table simple_example("
      "id varchar(255), "
      "type int, "
      "data text"
      ");";
  ret = jsql_ctx_exec(j_ctx, sql);
  if(ret == false) {
    printf("Could not create table.\n");
    return 1;
  }

  // create insert data
  j_tmp = json_pack("{s:s, s:i, s:{s:s, s:s}}",
      "id",     "3dd36215-2393-4a98-8bd5-aafff809af91",
      "type",   1,
      "data",
        "name",     "pchero",
        "message",  "hello world"
      );
  res = json_dumps(j_tmp, JSON_ENCODE_ANY);
  printf("Insert data. res[%s]\n", res);
  free(res);

  // insert data
  jsql_ctx_insert(j_ctx, "simple_example", j_tmp);
  json_decref(j_tmp);

  sql = "select * from simple_example;";
  jsql_ctx_query(j_ctx, sql);

  // get data
  j_res = jsql_ctx_get_record(j_ctx);
  res = json_dumps(j_res, JSON_ENCODE_ANY);
  json_decref(j_res);
  printf("Result. res[%s]\n", res);
  free(res);

  jsql_ctx_free(j_ctx);
  jsql_ctx_term(j_ctx);

  return 0;
}
