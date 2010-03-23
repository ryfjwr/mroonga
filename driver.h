#ifndef _driver_h
#define _driver_h

#include <stdlib.h>
#include <stdio.h>
#include <groonga.h>

#define MRN_MAX_KEY_LEN 1024
#define MRN_MAX_PATH_SIZE 64
#define MRN_DB_FILE_SUFFIX ".mrn"
#define MRN_LOG_FILE_NAME "mroonga.log"
#define MRN_INDEX_LEXICON_NAME "index_lexicon"
#define MRN_INDEX_HASH_NAME "index_hash"
#define MRN_INDEX_PAT_NAME "index_pat"

typedef unsigned char uchar;

typedef struct _mrn_charset_map
{
  const char *csname_mysql;
  grn_encoding csname_groonga;
} MRN_CHARSET_MAP;

#ifdef PROTOTYPE
typedef struct _mrn_field
{
  const char *name;
  uint name_len;
  grn_obj *obj;
  grn_obj *index;
  uint field_no;
} mrn_field;

typedef struct _mrn_table
{
  const char *name;
  uint name_len;
  uint use_count;
  grn_obj *obj;
  mrn_field **field;
  uint fields;
  uint pkey_field;
} mrn_table;
#endif

typedef struct _mrn_index_info
{
  const char *name;
  uint name_size;
  grn_obj_flags flags;
  grn_obj *type;
  grn_builtin_type gtype;
  grn_obj *obj;
  uint columnno;
} mrn_index_info;

typedef struct _mrn_key_info
{
  uint per_table;
  const char *name;
  uint name_size;
  grn_obj_flags flags;
  grn_obj *type;
  grn_builtin_type gtype;
  grn_obj *obj;
} mrn_key_info;

typedef struct _mrn_column_info
{
  const char *name;
  uint name_size;
  grn_obj_flags flags;
  grn_obj *type;
  grn_builtin_type gtype;
  uint indexno;
} mrn_column_info;

typedef struct _mrn_table_info
{
  const char *name;
  uint name_size;
  grn_obj_flags flags;
  grn_obj *key_type;
} mrn_table_info;

typedef struct _mrn_db_info
{
  const char *name;
  uint name_size;
  char *path;
} mrn_db_info;

typedef struct _mrn_object
{
  grn_obj *db;
  grn_obj *table;
  grn_obj **columns;
} mrn_object;

typedef struct _mrn_info
{
  mrn_db_info *db;
  mrn_table_info *table;
  mrn_key_info *key;
  mrn_column_info **columns;
  mrn_index_info **indexes;
  uint n_columns;
  uint n_indexes;
  grn_table_cursor *cursor;
  grn_obj *res;
  const char *name;
} mrn_info;

typedef struct _mrn_record
{
  mrn_info *info;
  const void *key;
  uint key_size;
  grn_obj **value;
  uint n_columns;
  uint actual_size;
  uchar *bitmap;
  grn_id id;
} mrn_record;

typedef enum {
  MRN_EXPR_UNKNOWN=0,
  MRN_EXPR_COLUMN,
  MRN_EXPR_AND,
  MRN_EXPR_OR,
  MRN_EXPR_EQ,
  MRN_EXPR_NOT_EQ,
  MRN_EXPR_GT,
  MRN_EXPR_GT_EQ,
  MRN_EXPR_LESS,
  MRN_EXPR_LESS_EQ,
  MRN_EXPR_INT,
  MRN_EXPR_TEXT,
  MRN_EXPR_NEGATIVE
} mrn_expr_type;

typedef struct _mrn_expr
{
  mrn_expr_type type;
  const char *val_string;
  int val_int;
  int n_args;
  struct _mrn_expr *prev;
  struct _mrn_expr *next;
} mrn_expr;

/* macro */
#define MRN_MALLOC(size) malloc(size)
#define MRN_FREE(ptr) free(ptr)

#define MRN_HANDLER_NAME(obj_name) (obj_name - 2)
#define MRN_TABLE_NAME(name) (name + 2)

#define MRN_IS_BIT(map,idx) \
  (uint) (((uchar*)map)[(idx)/8] & (1 << ((idx)&7)))
#define MRN_SET_BIT(map,idx) \
  (((uchar*)map)[(idx)/8] |= (1 << ((idx)&7)))
#define MRN_CLEAR_BIT(map,idx) \
  (((uchar*)map)[(idx)/8] &= ~(1 << ((idx)&7)))

/* functions */
int mrn_init(int in_mysql);
int mrn_deinit();
void mrn_logger_func(int level, const char *time, const char *title,
                     const char *msg, const char *location, void *func_arg);
void mrn_logger_mysql(int level, const char *time, const char *title,
                      const char *msg, const char *location, void *func_arg);

int mrn_hash_put(grn_ctx *ctx, const char *key, void *value);
int mrn_hash_get(grn_ctx *ctx, const char *key, void **value);
int mrn_hash_remove(grn_ctx *ctx, const char *key);

mrn_info* mrn_init_obj_info(grn_ctx *ctx, uint n_columns);
int mrn_deinit_obj_info(grn_ctx *ctx, mrn_info *info);
int mrn_create(grn_ctx *ctx, mrn_info *info, mrn_object *obj);
int mrn_open(grn_ctx *ctx, mrn_info *info, mrn_object *obj);
int mrn_close(grn_ctx *ctx, mrn_info *info, mrn_object *obj);
int mrn_drop(grn_ctx *ctx, char *db_path, char *table_name);
int mrn_write_row(grn_ctx *ctx, mrn_record *record, mrn_object *obj);
mrn_record* mrn_init_record(grn_ctx *ctx, mrn_info *info, uchar *bitmap, int size);
int mrn_deinit_record(grn_ctx *ctx, mrn_record *record);
int mrn_rewind_record(grn_ctx *ctx, mrn_record *record);
int mrn_rnd_init(grn_ctx *ctx, mrn_info *info, mrn_expr *expr, mrn_object *obj);
int mrn_rnd_next(grn_ctx *ctx, mrn_record *record, mrn_object *obj);
uint mrn_table_size(grn_ctx *ctx, mrn_object *obj);
void mrn_free_expr(mrn_expr *expr);
void mrn_dump_expr(mrn_expr *expr);
void mrn_dump_buffer(uchar *buf, int size);
int mrn_db_open_or_create(grn_ctx *ctx, mrn_info *info, mrn_object *obj);
int mrn_db_drop(grn_ctx *ctx, char *path);

/* static variables */
extern grn_hash *mrn_system_hash;
extern grn_obj *mrn_system_db;
extern pthread_mutex_t *mrn_lock, *mrn_lock_hash;
extern const char *mrn_logfile_name;
extern FILE *mrn_logfile;

extern grn_logger_info mrn_logger_info;
extern const char *mrn_log_level_str[];

#endif /* _driver_h */
