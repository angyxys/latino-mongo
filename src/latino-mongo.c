
#ifdef _WIN32
#define LATINO_BUILD_AS_DLL
#endif

#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif

#define LATINO_LIB

#include <mongoc/mongoc.h>
#include <latino.h>
#include <stdlib.h>
#include <string.h>

#define NOMBRE_LIB "mongo"

static mongoc_client_t *cliente = NULL;
static char *db_nombre = NULL;

static void requerir_conexion(lat_mv *mv) {
  if (!cliente) {
    latC_error(mv, "mongo: no hay conexión, llama a mongo.conectar(uri, db)");
  }
}

static void buf_append(char **buf, size_t *cap, size_t *len, const char *s) {
  size_t s1 = strnlen(s, 16 * 1024 * 1024);
  if (*len + s1 + 1 > *cap) {
    *cap = (*len + s1 + 1) * 2;
    *buf = (char *)realloc(*buf, *cap);
  } 
  memcpy(*buf + *len, s, s1);
  *len += s1;
  (*buf)[*len] = '\0';
}

static mongoc_collection_t *obtener_collecion(lat_mv *mv, const char *col) {
  requerir_conexion(mv);
  return mongoc_client_get_collection(cliente, db_nombre, col);
}

static bson_t *json_a_bson(lat_mv *mv, const char *json) {
  bson_error_t error;
  bson_t *doc = bson_new_from_json((const uint8_t *)json, strlen(json), &error);
  if (!doc) {
    latC_error(mv, "mongo: JSON invalido: %s", error.message);
  }
  return doc;
}

static void lat_mongo_conectar(lat_mv *mv) {
  lat_objeto *o_db = latC_desapilar(mv);
  lat_objeto *o_uri = latC_desapilar(mv);
  const char *uri = latC_checar_cadena(mv, o_uri);
  const char *db = latC_checar_cadena(mv, o_db);

  mongoc_init();

  if (cliente) {
    mongoc_client_destroy(cliente);
    cliente = NULL;
  }

  if (db_nombre) {
    free(db_nombre);
    db_nombre = NULL;
  }

  cliente = mongoc_client_new(uri);
  if (!cliente) {
    latC_apilar_int(mv, 0);
    return;
  }
  db_nombre = strdup(db);
  bson_error_t error;
  bson_t *ping = BCON_NEW("ping", BCON_INT32(1));
  bool ok = mongoc_client_command_simple(cliente, db, ping, NULL, NULL, &error);
  bson_destroy(ping);
  if (!ok) {
    mongoc_client_destroy(cliente);
    cliente = NULL;
    latC_error(mv, "mongo: fallo la conexión: %s", error.message);
    return;
  }
  latC_apilar_int(mv, 1);
}

static void lat_mongo_desconectar(lat_mv *mv) {
  if (cliente) {
    mongoc_client_destroy(cliente);
    cliente = NULL;
  }
  if (db_nombre) {
    free(db_nombre);
    db_nombre = NULL;
  }

  mongoc_cleanup();
}

static void lat_mongo_insertar(lat_mv *mv) {
  lat_objeto *o_doc = latC_desapilar(mv);
  lat_objeto *o_col = latC_desapilar(mv);
  const char *col = latC_checar_cadena(mv, o_col);
  const char *json = latC_checar_cadena(mv, o_doc);

  bson_t *doc = json_a_bson(mv, json);
  mongoc_collection_t *collection = obtener_collecion(mv, col);

  bson_error_t error;
  bool ok = mongoc_collection_insert_one(collection, doc, NULL, NULL, &error);
  if (!ok) {
    latC_error(mv, "mongo: error al insertar: %s", error.message);
  }

  bson_destroy(doc);
  mongoc_collection_destroy(collection);
  latC_apilar_int(mv, 1);
}

static void lat_mongo_insertar_varios(lat_mv *mv) {
  lat_objeto *o_docs = latC_desapilar(mv);
  lat_objeto *o_col = latC_desapilar(mv);
  const char *col = latC_checar_cadena(mv, o_col);
  const char *json = latC_checar_cadena(mv, o_docs);

  size_t n = strlen(json) + strlen("{\"_docs\":}") + 1;
  char *wrap = (char *)malloc(n);
  snprintf(wrap, n, "{\"_docs\":%s}", json);

  bson_t *wrapdoc = json_a_bson(mv, wrap);
  free(wrap);

  bson_iter_t outer, inner;
  if (!bson_iter_init_find(&outer, wrapdoc, "_docs") || !BSON_ITER_HOLDS_ARRAY(&outer)) {
    bson_destroy(wrapdoc);
    latC_error(mv, "mongo: insertar_varios espera un array JSON de documentos");
  }

  bson_t *docs[256]; // Limite practico
  size_t count = 0;
  bson_iter_recurse(&outer, &inner);
  while (bson_iter_next(&inner) && count < 256) {
    if (!BSON_ITER_HOLDS_DOCUMENT(&inner)) {
      continue;
    }
    uint32_t len = 0;
    const uint8_t *data = NULL;
    bson_iter_document(&inner, &len, &data);
    bson_t *doc = bson_new_from_data(data, len);
    if (doc) {
      docs[count++] = doc;
    }
  }

  mongoc_collection_t *collection = obtener_collecion(mv, col);
  bson_error_t error;
  bool ok = mongoc_collection_insert_many(collection, (const bson_t **)docs, count, NULL, NULL, &error);

  for (size_t i = 0; i < count; i++) {
    bson_destroy(docs[i]);
  }
  mongoc_collection_destroy(collection);
  bson_destroy(wrapdoc);
  if (!ok) {
    latC_error(mv, "mongo: error al insertar varios: %s", error.message);
  }
  latC_apilar_int(mv, (int)count);
}

static const lat_CReg lib_mongo[] = {
  {"conectar", lat_mongo_conectar, 2},
  {"insertar", lat_mongo_insertar, 2},
  {"insertar_varios", lat_mongo_insertar_varios, 2},
  {"desconectar", lat_mongo_desconectar, 0},
  {NULL, NULL, 0}
};

LATINO_API void latC_abrir_liblatino_mongo(lat_mv* mv) {
  latC_abrir_liblatino(mv, NOMBRE_LIB, lib_mongo);
}
