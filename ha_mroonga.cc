#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

#define MYSQL_SERVER 1

#include <mysql_priv.h>
#include <mysql/plugin.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ha_mroonga.h"

unsigned long mrn_log_level;

const char* mrn_log_level_names_lib[] =
{
  "NONE",
  "EMERG",
  "ALERT",
  "CRIT",
  "ERROR",
  "WARNING",
  "NOTICE",
  "INFO",
  "DEBUG",
  "DUMP",
  NullS
};

TYPELIB mrn_log_level_typelib =
{
  array_elements(mrn_log_level_names_lib)-1, "",
  mrn_log_level_names_lib, NULL
};

static void mrn_log_level_update_func
(MYSQL_THD thd, struct st_mysql_sys_var *var, void *var_ptr, const void *save)
{
  if (save)
  {
    mrn_log_level = *((ulong*) save);
    mrn_logger_info.max_level = (grn_log_level) mrn_log_level;
  }
}

static MYSQL_SYSVAR_ENUM(
                         log_level,
                         mrn_log_level,
                         PLUGIN_VAR_RQCMDARG,
                         "max logging level.",
                         NULL,
                         mrn_log_level_update_func,
                         3,
                         &mrn_log_level_typelib
                         );

static MYSQL_THDVAR_BOOL(
                         debug,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_THDLOCAL,
                         "use debug print.",
                         NULL,
                         NULL,
                         FALSE
                         );

static MYSQL_THDVAR_BOOL(
                         use_column_pruning,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_THDLOCAL,
                         "use column pruning.",
                         NULL,
                         NULL,
                         TRUE
                         );

static MYSQL_THDVAR_BOOL(
                         use_cond_push,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_THDLOCAL,
                         "use cond_push and groonga expression.",
                         NULL,
                         NULL,
                         TRUE
                         );

static MYSQL_THDVAR_BOOL(
                         idx_repos_per_table,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_THDLOCAL,
                         "use index repository per table.",
                         NULL,
                         NULL,
                         FALSE
                         );

MRN_CHARSET_MAP mrn_charset_map[] = {
  {"utf8", GRN_ENC_UTF8},
  {"cp932", GRN_ENC_SJIS},
  {"sjis", GRN_ENC_SJIS},
  {"eucjpms", GRN_ENC_EUC_JP},
  {"ujis", GRN_ENC_EUC_JP},
  {"latin1", GRN_ENC_LATIN1},
  {"koi8r", GRN_ENC_KOI8R},
  {0x0, GRN_ENC_DEFAULT},
  {0x0, GRN_ENC_NONE}
};

const char *mrn_item_type_string[] = {
  "FIELD_ITEM", "FUNC_ITEM", "SUM_FUNC_ITEM", "STRING_ITEM",
  "INT_ITEM", "REAL_ITEM", "NULL_ITEM", "VARBIN_ITEM",
  "COPY_STR_ITEM", "FIELD_AVG_ITEM", "DEFAULT_VALUE_ITEM",
  "PROC_ITEM", "COND_ITEM", "REF_ITEM", "FIELD_STD_ITEM",
  "FIELD_VARIANCE_ITEM", "INSERT_VALUE_ITEM",
  "SUBSELECT_ITEM", "ROW_ITEM", "CACHE_ITEM", "TYPE_HOLDER",
  "PARAM_ITEM", "TRIGGER_FIELD_ITEM", "DECIMAL_ITEM",
  "XPATH_NODESET", "XPATH_NODESET_CMP",
  "VIEW_FIXER_ITEM"};

const char *mrn_functype_string[] = {
  "UNKNOWN_FUNC","EQ_FUNC","EQUAL_FUNC","NE_FUNC","LT_FUNC","LE_FUNC",
  "GE_FUNC","GT_FUNC","FT_FUNC",
  "LIKE_FUNC","ISNULL_FUNC","ISNOTNULL_FUNC",
  "COND_AND_FUNC", "COND_OR_FUNC", "COND_XOR_FUNC",
  "BETWEEN", "IN_FUNC", "MULT_EQUAL_FUNC",
  "INTERVAL_FUNC", "ISNOTNULLTEST_FUNC",
  "SP_EQUALS_FUNC", "SP_DISJOINT_FUNC","SP_INTERSECTS_FUNC",
  "SP_TOUCHES_FUNC","SP_CROSSES_FUNC","SP_WITHIN_FUNC",
  "SP_CONTAINS_FUNC","SP_OVERLAPS_FUNC",
  "SP_STARTPOINT","SP_ENDPOINT","SP_EXTERIORRING",
  "SP_POINTN","SP_GEOMETRYN","SP_INTERIORRINGN",
  "NOT_FUNC", "NOT_ALL_FUNC",
  "NOW_FUNC", "TRIG_COND_FUNC",
  "SUSERVAR_FUNC", "GUSERVAR_FUNC", "COLLATE_FUNC",
  "EXTRACT_FUNC", "CHAR_TYPECAST_FUNC", "FUNC_SP", "UDF_FUNC",
  "NEG_FUNC", "GSYSVAR_FUNC"};

struct st_mysql_storage_engine storage_engine_structure =
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

longlong mrn_status_column_target = 0;
longlong mrn_status_column_used = 0;
longlong mrn_status_cond_push_used = 0;

struct st_mysql_show_var mrn_status_variables[] =
{
  {"mroonga_column_target", (char*) &mrn_status_column_target, SHOW_LONGLONG},
  {"mroonga_column_used", (char*) &mrn_status_column_used, SHOW_LONGLONG},
  {"mroonga_cond_push_used", (char*) &mrn_status_cond_push_used, SHOW_LONGLONG},
  NULL
};

struct st_mysql_sys_var  *mrn_system_variables[] =
{
  MYSQL_SYSVAR(log_level),
  MYSQL_SYSVAR(debug),
  MYSQL_SYSVAR(use_column_pruning),
  MYSQL_SYSVAR(use_cond_push),
  MYSQL_SYSVAR(idx_repos_per_table),
  NULL
};

grn_encoding mrn_charset_mysql_groonga(const char *csname)
{
  if (!csname) return GRN_ENC_NONE;
  int i;
  for (i = 0; mrn_charset_map[i].csname_mysql; i++) {
    if (!(strcasecmp(csname, mrn_charset_map[i].csname_mysql)))
      return mrn_charset_map[i].csname_groonga;
  }
  return GRN_ENC_NONE;
}

const char *mrn_charset_groonga_mysql(grn_encoding encoding)
{
  int i;
  for (i = 0; (mrn_charset_map[i].csname_groonga != GRN_ENC_DEFAULT); i++) {
    if (mrn_charset_map[i].csname_groonga == encoding)
      return mrn_charset_map[i].csname_mysql;
  }
  return NULL;
}

grn_builtin_type mrn_get_type(grn_ctx *ctx, int type)
{
  grn_builtin_type gtype;
  switch (type)
  {
  case MYSQL_TYPE_LONG:
    gtype = GRN_DB_INT32;
    break;
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_BLOB:
    gtype = GRN_DB_TEXT;
    break;
  default:
    gtype = GRN_DB_VOID;
  }
  return gtype;
}

handler *mrn_handler_create(handlerton *hton,
			    TABLE_SHARE *share,
			    MEM_ROOT *root)
{
  return (new (root) ha_mroonga(hton, share));
}

void mrn_handler_drop_db(handlerton *hton, char *path)
{
  char db_path[MRN_MAX_PATH_SIZE];
  char db_name[MRN_MAX_PATH_SIZE];
  int i;
  int len = strlen(path);
  struct stat dummy;
  grn_ctx ctx;
  grn_ctx_init(&ctx, 0);
  strncpy(db_path, path+2, len-3);
  db_path[len-3] = '\0';
  strncpy(db_name, db_path, MRN_MAX_PATH_SIZE);
  strncat(db_path, MRN_DB_FILE_SUFFIX, MRN_MAX_PATH_SIZE);
  pthread_mutex_lock(mrn_lock);
  if (stat(db_path, &dummy) == 0)
  {
    mrn_db_drop(&ctx, db_path);
    mrn_hash_remove(&ctx, db_name);
  }
  pthread_mutex_unlock(mrn_lock);
  grn_ctx_fin(&ctx);
}

int mrn_plugin_init(void *p)
{
  handlerton *hton;
  hton = (handlerton *)p;
  hton->state = SHOW_OPTION_YES;
  hton->create = mrn_handler_create;
  hton->flags = 0;
  hton->drop_database = mrn_handler_drop_db;
  return mrn_init(0);
}

int mrn_plugin_deinit(void *p)
{
  return mrn_deinit();
}

mysql_declare_plugin(mroonga)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &storage_engine_structure,
  "mroonga",
  "Tetsuro IKEDA",
  "MySQL binding for Groonga",
  PLUGIN_LICENSE_BSD,
  mrn_plugin_init,
  mrn_plugin_deinit,
  0x0001,
  mrn_status_variables,
  mrn_system_variables,
  NULL
}
mysql_declare_plugin_end;

/* handler implementation */
ha_mroonga::ha_mroonga(handlerton *hton, TABLE_SHARE *share)
  :handler(hton, share)
{
  ctx = (grn_ctx*) malloc(sizeof(grn_ctx));
  grn_ctx_init(ctx,0);
  grn_ctx_use(ctx, mrn_system_db);
  minfo = NULL;
  mcond = NULL;
  cur = NULL;
  obj = (mrn_object *) malloc(sizeof(mrn_object));
}

ha_mroonga::~ha_mroonga()
{
  grn_ctx_fin(ctx);
  free(ctx);
  free(obj);
}

const char *ha_mroonga::table_type() const
{
  return "mroonga";
}

const char *ha_mroonga::index_type(uint inx)
{
  return "NONE";
}

static const char*ha_mroonga_exts[] = {
  NullS
};
const char **ha_mroonga::bas_ext() const
{
  return ha_mroonga_exts;
}

ulonglong ha_mroonga::table_flags() const
{
  return HA_NO_TRANSACTIONS|HA_REC_NOT_IN_SEQ|HA_NULL_IN_KEY|HA_CAN_FULLTEXT;
}

ulong ha_mroonga::index_flags(uint idx, uint part, bool all_parts) const
{
  return 0;
}

int ha_mroonga::create(const char *name, TABLE *form, HA_CREATE_INFO *info)
{
  int res;
  mrn_info *minfo;
  MRN_HTRACE;
  pthread_mutex_lock(mrn_lock);
  convert_info(name, this->table_share, &minfo);
  if (res = mrn_db_open_or_create(ctx, minfo, obj) == 0)
  {
    grn_ctx_use(ctx, obj->db);
    res = mrn_create(ctx, minfo, obj);
    mrn_hash_put(ctx, name, minfo);
  }
  pthread_mutex_unlock(mrn_lock);
  return res;
}

int ha_mroonga::open(const char *name, int mode, uint test_if_locked)
{
  MRN_HTRACE;
  thr_lock_init(&thr_lock);
  thr_lock_data_init(&thr_lock, &thr_lock_data, NULL);

  mrn_info *minfo;
  int res = 0;

  pthread_mutex_lock(mrn_lock);

  if (mrn_hash_get(ctx, name, (void**) &minfo) != 0)
  {
    convert_info(name, this->table_share, &minfo);
    mrn_hash_put(ctx, name, minfo);
  }
  this->minfo = minfo;

  if (res = mrn_db_open_or_create(ctx, minfo, obj) == 0)
  {
    grn_ctx_use(ctx, obj->db);
    res = mrn_open(ctx, minfo, obj);
  }
  pthread_mutex_unlock(mrn_lock);
  return res;
}

int ha_mroonga::close()
{
  MRN_HTRACE;
  thr_lock_delete(&thr_lock);

  mrn_info *minfo = this->minfo;
  pthread_mutex_lock(mrn_lock);
  mrn_close(ctx, minfo, obj);
  mrn_deinit_obj_info(ctx, minfo);
  this->minfo = NULL;
  pthread_mutex_unlock(mrn_lock);
  return 0;
}

int ha_mroonga::delete_table(const char *name)
{
  MRN_HTRACE;
  int res, i, j;
  char db_name[32], table_name[32];
  mrn_info *minfo;
  pthread_mutex_lock(mrn_lock);
  if (mrn_hash_get(ctx, name, (void**) &minfo) == 0)
  {
    mrn_hash_remove(ctx, name);
  }
  int name_len = strlen(name);
  for (i=2; i < name_len; i++)
  {
    if (name[i] != '/')
    {
      db_name[i-2] = name[i];
    }
    else
    {
      db_name[i-2] = '\0';
      break;
    }
  }
  i++;
  for (j=0; i < name_len; j++, i++)
  {
    table_name[j] = name[i];
  }
  table_name[j] = '\0';

  char db_path[MRN_MAX_PATH_SIZE];
  strncpy(db_path, db_name, MRN_MAX_PATH_SIZE);
  strncat(db_path, MRN_DB_FILE_SUFFIX, MRN_MAX_PATH_SIZE);

  res = mrn_drop(ctx, db_path, table_name);
  pthread_mutex_unlock(mrn_lock);
  return res;
}

int ha_mroonga::info(uint flag)
{
  MRN_HTRACE;
  if (this->minfo)
  {
    stats.records = (ha_rows) mrn_table_size(ctx, obj);
  }
  else
  {
    stats.records = 2;
  }
  return 0;
}

THR_LOCK_DATA **ha_mroonga::store_lock(THD *thd,
				       THR_LOCK_DATA **to,
				       enum thr_lock_type lock_type)
{
  MRN_HTRACE;
  if (lock_type != TL_IGNORE && thr_lock_data.type == TL_UNLOCK)
    thr_lock_data.type = lock_type;
  *to++ = &thr_lock_data;
  return to;
}

int ha_mroonga::rnd_init(bool scan)
{
  MRN_HTRACE;
  int i, used=0, n_columns, alloc_size;
  char use_column_pruning = THDVAR(table->in_use, use_column_pruning);
  char use_cond_push = THDVAR(table->in_use, use_cond_push);
  n_columns = minfo->n_columns;
  alloc_size = n_columns / 8 + 1;
  uchar* column_map = (uchar*) malloc(alloc_size);
  memset(column_map,0,alloc_size);
  for (i=0; i < n_columns; i++)
  {
    if (use_column_pruning == 0 ||
        bitmap_is_set(table->read_set, i) ||
        bitmap_is_set(table->write_set, i))
    {
      MRN_SET_BIT(column_map, i);
      used++;
    }
  }
  mrn_status_column_target += n_columns;
  mrn_status_column_used += used;
  this->cur = mrn_init_record(ctx, minfo, column_map, used);

  if (use_cond_push && mcond)
  {
    mrn_status_cond_push_used++;
    const COND *cond = mcond->cond;
    mcond->expr = (mrn_expr*) malloc(sizeof(mrn_expr));
    mrn_expr *expr = mcond->expr;
    expr->prev = NULL;
    make_expr((Item*) cond, &expr);
    check_other_conditions(mcond, this->table->in_use);
    if (THDVAR(table->in_use, debug))
    {
      dump_condition(cond);
      dump_tree((Item*)cond,0);
      dump_condition2(mcond);
    }
  }
  return mrn_rnd_init(ctx, minfo,
                      mcond ? mcond->expr : NULL, obj);
}

int ha_mroonga::rnd_next(uchar *buf)
{
  MRN_HTRACE;
  int rc;
  mrn_record *record;
  mrn_info *info = this->minfo;
  mrn_cond *cond = this->mcond;
  record = this->cur;
  rc = mrn_rnd_next(ctx, record, obj);
  if (rc == 0)
  {
    Field **field = table->field;
    mrn_column_info **column = info->columns;
    grn_obj *value;
    int i, j;
    for (i=0, j=0; *field; field++, column++, i++)
    {
      // column pruning
      if (MRN_IS_BIT(record->bitmap, i))
      {
        value = record->value[j];
        switch ((*field)->type())
        {
        case (MYSQL_TYPE_LONG) :
          (*field)->set_notnull();
          (*field)->store(GRN_INT32_VALUE(value));
          break;
        case (MYSQL_TYPE_VARCHAR) :
        case (MYSQL_TYPE_BLOB) :
          (*field)->set_notnull();
          (*field)->store(GRN_TEXT_VALUE(value), GRN_BULK_WSIZE(value), (*field)->charset());
          break;
        }
        j++;
      }
    }
    mrn_rewind_record(ctx, record);
    return 0;
  }

  if (mcond)
  {
    mrn_free_expr(mcond->expr);
    mcond = NULL;
  }
  free(record->bitmap);
  mrn_deinit_record(ctx, record);
  cur = NULL;

  if (rc == 1)
  {
    return HA_ERR_END_OF_FILE;
  }
  else
  {
    return -1;
  }
}

#ifdef PROTOTYPE
int ha_mroonga::rnd_pos(uchar *buf, uchar *pos)
{
  grn_id gid = *((grn_id*) pos);

  grn_obj obj;
  int pkey_val;
  int *val;
  GRN_TEXT_INIT(&obj,0);

  Field **mysql_field;
  mrn_field **grn_field;
  int num;
  for (mysql_field = table->field, grn_field = share->field, num=0;
       *mysql_field;
       mysql_field++, grn_field++, num++) {
    if (num == share->pkey_field) {
      int ret_val = grn_table_get_key(ctx, share->obj, gid, (void*) &pkey_val, sizeof(int));
      (*mysql_field)->set_notnull();
      (*mysql_field)->store(pkey_val);
    } else {
      GRN_BULK_REWIND(&obj);
      grn_obj_get_value(ctx, (*grn_field)->obj, gid, &obj);
      val = (int*) GRN_BULK_HEAD(&obj);
      (*mysql_field)->set_notnull();
      (*mysql_field)->store(*val);
    }
  }
  return 0;
}
#else
int ha_mroonga::rnd_pos(uchar *buf, uchar *pos)
{
  MRN_HTRACE;
  return 0;
}
#endif

#ifdef PROTOTYPE
void ha_mroonga::position(const uchar *record)
{
  memcpy(this->ref, &this->record_id, sizeof(grn_id));
}
#else
void ha_mroonga::position(const uchar *record)
{
  MRN_HTRACE;
}
#endif

int ha_mroonga::write_row(uchar *buf)
{
  MRN_HTRACE;
  mrn_info *minfo = this->minfo;
  uchar *bitmap;
  int size = set_bitmap(&bitmap);
  mrn_record *record = mrn_init_record(ctx, minfo, bitmap, size);
  Field **field;
  int i, j;
  for (i=0, j=0, field = table->field; *field; i++, field++)
  {
    if (MRN_IS_BIT(record->bitmap, i))
    {
      switch ((*field)->type())
      {
      case MYSQL_TYPE_LONG:
      {
        GRN_INT32_SET(ctx, record->value[j], (*field)->val_int());
        break;
      }
      case MYSQL_TYPE_VARCHAR:
      {
        String tmp;
        const char *val = (*field)->val_str(&tmp)->ptr();
        GRN_TEXT_SET(ctx, record->value[j], val, (*field)->data_length());
        break;
      }
      case MYSQL_TYPE_BLOB:
      {
        String tmp;
        Field_blob *blob = (Field_blob*) *field;
        const char *val = blob->val_str(0,&tmp)->ptr();
        GRN_TEXT_SET(ctx, record->value[j], val, blob->get_length());
        break;
      }
      default:
        return HA_ERR_UNSUPPORTED;
      }
      j++;
    }
  }
  if (mrn_write_row(ctx, record, obj) != 0)
  {
    return -1;
  }
  free(record->bitmap);
  mrn_deinit_record(ctx, record);
  return 0;
}

#ifdef PROTOTYPE
int ha_mroonga::index_read(uchar *buf, const uchar *key,
			   uint key_len, enum ha_rkey_function find_flag)
{
  Field *key_field= table->key_info[active_index].key_part->field;
  uint rc= 0;
  grn_id gid;
  grn_obj wrapper;
  Field **mysql_field;
  mrn_field **grn_field;
  int num;
  grn_search_flags flags = 0;

  int k;
  memcpy(&k,key,sizeof(int));
  gid = grn_table_lookup(ctx, share->obj,
			 (const void*) key, sizeof(key), &flags);

  GRN_TEXT_INIT(&wrapper,0);
  for (mysql_field = table->field, grn_field = share->field, num=0;
       *mysql_field;
       mysql_field++, grn_field++, num++) {
    if (num == share->pkey_field) {
      continue;
    }
    GRN_BULK_REWIND(&wrapper);
    grn_obj_get_value(ctx, (*grn_field)->obj, gid, &wrapper);
    int *tmp_int;
    char *tmp_char;
    switch((*mysql_field)->type()) {
    case (MYSQL_TYPE_LONG) :
      tmp_int = (int*) GRN_BULK_HEAD(&wrapper);
      (*mysql_field)->set_notnull();
      (*mysql_field)->store(*tmp_int);
      break;
    case (MYSQL_TYPE_VARCHAR) :
      tmp_char = (char*) GRN_BULK_HEAD(&wrapper);
      (*mysql_field)->set_notnull();
      (*mysql_field)->store(tmp_char,strlen(tmp_char), system_charset_info);
      break;
    }
  }
  grn_obj_unlink(ctx, &wrapper);

  if (key_field->field_index == table->s->primary_key)
  {
    key_field->set_key_image(key, key_len);
    key_field->set_notnull();
  }
  return rc;
}
#else
int ha_mroonga::index_read(uchar *buf, const uchar *key,
			   uint key_len, enum ha_rkey_function find_flag)
{
  MRN_HTRACE;
  return 0;
}
#endif

int ha_mroonga::index_next(uchar *buf)
{
  MRN_HTRACE;
  return HA_ERR_END_OF_FILE;
}

int ha_mroonga::ft_init() {
  MRN_HTRACE;
  return 0;
}

#ifdef PROTOTYPE
FT_INFO *ha_mroonga::ft_init_ext(uint flags, uint inx,String *key)
{
  const char *match_param;
  match_param = key->ptr();
  if (flags & FT_BOOL) {
    /* boolean search */
    grn_query *query;
    this->res = grn_table_create(ctx, NULL, 0, NULL, GRN_TABLE_HASH_KEY, share->obj, 0);
    query = grn_query_open(ctx, match_param, strlen(match_param), GRN_OP_OR, 32);
    grn_obj_search(ctx, share->field[1]->index, (grn_obj*) query, this->res, GRN_OP_OR, NULL);
    this->cursor = grn_table_cursor_open(ctx, res, NULL, 0, NULL, 0, 0);
    //grn_query_close(ctx, query);
  } else {
    /* nlq search */
    grn_obj buff;
    this->res = grn_table_create(ctx, NULL, 0, NULL, GRN_TABLE_HASH_KEY, share->obj, 0);
    GRN_TEXT_INIT(&buff, 0);
    GRN_TEXT_SET(ctx, &buff, match_param, strlen(match_param));
    grn_obj_search(ctx, share->field[1]->index, &buff, this->res, GRN_OP_OR, NULL);
    this->cursor = grn_table_cursor_open(ctx, res, NULL, 0, NULL, 0, 0);
    //grn_obj_close(ctx, &buff);
  }
  int nrec = grn_table_size(ctx, res);
  return NULL;
}
#else
FT_INFO *ha_mroonga::ft_init_ext(uint flags, uint inx,String *key)
{
  MRN_HTRACE;
  return NULL;
}
#endif

#ifdef PROTOTYPE
int ha_mroonga::ft_read(uchar *buf)
{
  /*
  if (mrn_counter == 0) {
    table->field[0]->set_notnull();
    table->field[0]->store(10);
    table->field[1]->set_notnull();
    table->field[1]->store("test", 4, system_charset_info);
    mrn_counter++;
    return 0;
  } else {
    mrn_counter=0;
    return HA_ERR_END_OF_FILE;
    }*/
  grn_id id, docid;
  grn_obj buff;
  GRN_TEXT_INIT(&buff,0);
  if  ((id = grn_table_cursor_next(ctx, this->cursor))) {
    GRN_BULK_REWIND(&buff);
    grn_table_get_key(ctx, this->res, id, &docid, sizeof(grn_id));
    grn_obj_get_value(ctx, share->field[1]->obj, docid, &buff);
    table->field[0]->set_notnull();
    table->field[0]->store(docid);
    table->field[1]->set_notnull();
    table->field[1]->store((char*) GRN_BULK_HEAD(&buff), GRN_TEXT_LEN(&buff), system_charset_info);
    return 0;
  }
  table->status = HA_ERR_END_OF_FILE;
  return HA_ERR_END_OF_FILE;
}
#else
int ha_mroonga::ft_read(uchar *buf)
{
  MRN_HTRACE;
  return HA_ERR_END_OF_FILE;
}
#endif

const COND *ha_mroonga::cond_push(const COND *cond)
{
  MRN_HTRACE;
  if (cond)
  {
    mrn_cond *tmp = (mrn_cond*) malloc(sizeof(mrn_cond));
    if (tmp == NULL)
    {
      goto err_oom;
    }
    tmp->cond = (COND *) cond;
    tmp->next = this->mcond;
    tmp->expr = NULL;
    tmp->limit = 0;
    tmp->offset = 0;
    mcond = tmp;
  }
  DBUG_RETURN(NULL);

err_oom:
  my_errno = HA_ERR_OUT_OF_MEM;
  GRN_LOG(ctx, GRN_LOG_ERROR, "malloc error in cond_push");
  return NULL;
}

void ha_mroonga::cond_pop()
{
  MRN_HTRACE;
  if (mcond)
  {
    mrn_cond *tmp = mcond->next;
    free(mcond);
    mcond = tmp;
  }
}

int ha_mroonga::convert_info(const char *name, TABLE_SHARE *share, mrn_info **_minfo)
{
  uint n_columns = share->fields, i;
  mrn_db_info *db;
  mrn_table_info *table;
  mrn_info *minfo = mrn_init_obj_info(ctx, n_columns);
  minfo->name = name;

  db = minfo->db;
  db->name = share->db.str;
  db->name_size = share->db.length;
  strncpy(db->path, db->name, MRN_MAX_PATH_SIZE);
  strncat(db->path, MRN_DB_FILE_SUFFIX, MRN_MAX_PATH_SIZE);

  table = minfo->table;
  table->name = share->table_name.str;
  table->name_size = share->table_name.length;
  table->flags |= GRN_OBJ_TABLE_NO_KEY;

  for (i=0; i < n_columns; i++)
  {
    Field *field = share->field[i];
    minfo->columns[i]->name = field->field_name;
    minfo->columns[i]->name_size = strlen(minfo->columns[i]->name);
    minfo->columns[i]->flags |= GRN_OBJ_COLUMN_SCALAR;
    grn_builtin_type gtype = mrn_get_type(ctx, field->type());
    minfo->columns[i]->gtype = gtype;
    if (gtype != GRN_DB_VOID)
    {
      minfo->columns[i]->type = grn_ctx_at(ctx, gtype);
    }
    else
    {
      GRN_LOG(ctx, GRN_LOG_ERROR, "cannot use column type=%d for %s.%s",
              field->type(), minfo->table->name, field->field_name);
      return -1;
    }
  }

  minfo->n_columns = n_columns;
  *_minfo = minfo;
  return 0;
}

int ha_mroonga::set_bitmap(uchar **bitmap)
{
  int i, used=0, n_columns, alloc_size;
  n_columns = minfo->n_columns;
  alloc_size = n_columns / 8 + 1;
  uchar* column_map = (uchar*) malloc(alloc_size);
  if (!column_map)
  {
    goto err_oom;
  }
  memset(column_map,0,alloc_size);
  for (i=0; i < n_columns; i++)
  {
    if (bitmap_is_set(table->read_set, i) ||
        bitmap_is_set(table->write_set, i))
    {
      MRN_SET_BIT(column_map, i);
      used++;
    }
  }
  *bitmap = column_map;
  return used;

err_oom:
  my_errno = HA_ERR_OUT_OF_MEM;
  GRN_LOG(ctx, GRN_LOG_ERROR, "malloc error in set_bitmap (%d bytes)", alloc_size);
  return -1;
}

const char *indent = "....................";

void ha_mroonga::dump_tree(Item *item, int offset)
{
  char *str;
  if (item->type() == Item::FUNC_ITEM)
  {
    Item_func *func = (Item_func*) item;
    switch (func->functype())
    {
    case Item_func::EQ_FUNC:
    case Item_func::EQUAL_FUNC:
      str = (char*) "=";
      break;
    case Item_func::LE_FUNC:
      str = (char*) "<=";
      break;
    case Item_func::LT_FUNC:
      str = (char*) "<";
        break;
    case Item_func::GE_FUNC:
      str = (char*) ">=";
      break;
    case Item_func::GT_FUNC:
      str = (char*) ">";
        break;
    case Item_func::NEG_FUNC:
      str = (char*) "-";
      break;
    default:
      str = (char*) mrn_functype_string[func->functype()];
    }
    printf("%s%s\n", (indent+(20-offset)), str);
    int i;
    for (i=0; i < func->arg_count; i++)
    {
      dump_tree((func->arguments())[i],(offset+1));
    }
  }
  else if (item->type() == Item::FIELD_ITEM)
  {
    str =  ((Item_field*) item)->name;
    printf("%s%s\n", (indent+(20-offset)), str);
  }
  else if (item->type() == Item::COND_ITEM)
  {
    Item_cond *cond = (Item_cond*) item;
    switch(cond->functype())
    {
    case Item_func::COND_AND_FUNC:
      str = (char*) "AND";
      break;
    case Item_func::COND_OR_FUNC:
      str = (char*) "OR";
      break;
    default:
      str = (char*) "(COND_ITEM)";
    }
    printf("%s%s\n", (indent+(20-offset)), str);
    List_iterator_fast<Item> lif(*(cond->argument_list()));
    Item *child;
    while ((child = lif++))
    {
      dump_tree(child, (offset+1));
    }
  }
  else if (item->type() == Item::INT_ITEM)
  {
    printf("%s%lld\n", (indent+(20-offset)),((Item_int*) item)->val_int());
  }
  else
  {
    printf("%s%s\n", (indent+(20-offset)), mrn_item_type_string[item->type()]);
  }
}

void ha_mroonga::dump_condition(const COND *cond)
{
  Item *item = (Item*) cond;
  while (item)
  {
    if (item->type() == Item::FUNC_ITEM)
    {
      Item_func *func = (Item_func*) item;
      switch (func->functype())
      {
      case Item_func::EQ_FUNC:
      case Item_func::EQUAL_FUNC:
        printf("(=)");
        break;
      case Item_func::LE_FUNC:
        printf("(<=)");
        break;
      case Item_func::LT_FUNC:
        printf("(<)");
        break;
      case Item_func::GE_FUNC:
        printf("(<=)");
        break;
      case Item_func::GT_FUNC:
        printf("(<)");
        break;
      default:
        printf("(F)");
      }
    }
    else if (item->type() == Item::FIELD_ITEM)
    {
      printf("<%s>", ((Item_field*) item)->name);
    }
    else if (item->type() == Item::COND_ITEM)
    {
      Item_cond *cond = (Item_cond*) item;
      switch(cond->functype())
      {
      case Item_func::COND_AND_FUNC:
        printf("(AND)");
        break;
      case Item_func::COND_OR_FUNC:
        printf("(OR)");
        break;
      default:
        printf("(C)");
      }
    }
    else if (item->type() == Item::INT_ITEM)
    {
      printf("(%lld)", ((Item_int*) item)->val_int());
    }
    else
    {
      printf("(%s)", mrn_item_type_string[item->type()]);
    }
    item = item->next;
  }
  printf("\n\n");
}

int ha_mroonga::make_expr(Item *item, mrn_expr **expr)
{
  mrn_expr *cur = *expr;
  switch(item->type())
  {
  case Item::FUNC_ITEM:
  {
    int i;
    Item_func *func = (Item_func*) item;
    for (i=0; i < func->arg_count; i++)
    {
      make_expr((func->arguments())[i], &cur);
      cur->next = (mrn_expr*) malloc(sizeof(mrn_expr));
      cur->next->prev = cur;
      cur = cur->next;
    }
    cur->n_args = func->arg_count;
    cur->next = NULL;
    switch (func->functype())
    {
    case Item_func::EQ_FUNC:
    case Item_func::EQUAL_FUNC:
      cur->type = MRN_EXPR_EQ;
      break;
    case Item_func::LE_FUNC:
      cur->type = MRN_EXPR_LESS_EQ;
      break;
    case Item_func::LT_FUNC:
      cur->type = MRN_EXPR_LESS;
      break;
    case Item_func::GE_FUNC:
      cur->type = MRN_EXPR_GT_EQ;
      break;
    case Item_func::GT_FUNC:
      cur->type = MRN_EXPR_GT;
      break;
    case Item_func::NEG_FUNC:
      cur->type = MRN_EXPR_NEGATIVE;
      cur->prev->val_int = cur->prev->val_int * -1;
      break;
    default:
      cur->type = MRN_EXPR_UNKNOWN;
    }
    break;
  }
  case Item::FIELD_ITEM:
  {
    cur->type = MRN_EXPR_COLUMN;
    cur->val_string =  ((Item_field*) item)->name;
    cur->next = NULL;
    break;
  }
  case Item::COND_ITEM:
  {
    Item_cond *cond = (Item_cond*) item;
    List_iterator_fast<Item> lif(*(cond->argument_list()));
    Item *child;
    int i=0;
    while ((child = lif++))
    {
      make_expr(child, &cur);
      cur->next = (mrn_expr*) malloc(sizeof(mrn_expr));
      cur = cur->next;
      i++;
    }
    cur->n_args = i;
    cur->next = NULL;
    switch(cond->functype())
    {
    case Item_func::COND_AND_FUNC:
      cur->type = MRN_EXPR_AND;
      break;
    case Item_func::COND_OR_FUNC:
      cur->type = MRN_EXPR_OR;
      break;
    default:
      cur->type = MRN_EXPR_UNKNOWN;
    }
    break;
  }
  case Item::INT_ITEM:
  {
    cur->type = MRN_EXPR_INT;
    cur->val_int = ((Item_int*) item)->val_int();
    break;
  }
  case Item::STRING_ITEM:
  {
    cur->type = MRN_EXPR_TEXT;
    cur->val_string = ((Item_string*) item)->val_str(NULL)->ptr();
    break;
  }
  default:
  {
    cur->type = MRN_EXPR_UNKNOWN;
  }
  }
  *expr = cur;
}

int ha_mroonga::check_other_conditions(mrn_cond *cond, THD *thd)
{
  SELECT_LEX lex = thd->lex->select_lex;
  if (lex.explicit_limit == true)
  {
    cond->limit = lex.select_limit ? lex.select_limit->val_int() : 0;
    cond->offset = lex.offset_limit ? lex.offset_limit->val_int() : 0;
  }
  else
  {
    cond->limit = 0;
    cond->offset = 0;
  }
  cond->table_list_size = thd->lex->select_lex.table_list.elements;
  cond->order_list_size = thd->lex->select_lex.order_list.elements;
  return 0;
}

void ha_mroonga::dump_condition2(mrn_cond *cond)
{
  mrn_dump_expr(cond->expr);
  printf(" join=%d order=%d limit=%lld offset=%lld\n",
         cond->table_list_size, cond->order_list_size,
         cond->limit, cond->offset);
}

#ifdef __cplusplus
}
#endif
