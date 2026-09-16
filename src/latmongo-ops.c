#include "latmongo.h"
#include <stdlib.h>
#include <string.h>

static long reply_contador(const bson_t *reply, const char *key) {
  bson_iter_t it;
  if (!bson_iter_init_find(&it, reply, key)) {
    return 0;
  }
  if (BSON_ITER_HOLDS_INT32(&it)) {
    return (long)bson_iter_int32(&it);
  }
  if (BSON_ITER_HOLDS_INT64(&it)) {
    return (long)bson_iter_int64(&it);
  }
  if (BSON_ITER_HOLDS_DOUBLE(&it)) {
    return (long)bson_iter_double(&it);
  }
  return 0;
}

static void apilar_entero(lat_mv *mv, int v) {
  latC_apilar(mv, latC_crear_numerico(mv, (double)v));
}

void latmongo_op_conectar(lat_mv *mv) {
  lat_objeto *o_db = latC_desapilar(mv);
  lat_objeto *o_uri = latC_desapilar(mv);
  const char *uri = latC_checar_cadena(mv, o_uri);
  const char *db = latC_checar_cadena(mv, o_db);

  if (uri[0] == '\0') {
    latC_error(mv, "mongo: el URI no puede ser vacio, ej: mongodb://localhost:27017");
  }
  if (db[0] == '\0') {
    latC_error(mv, "mongo: el nombre de la base de datos no puede ser vacio");
  }

  if (!latmongo_iniciado) {
    mongoc_init();
    latmongo_iniciado = true;
  }

  if (latmongo_cliente) {
    mongoc_client_destroy(latmongo_cliente);
    latmongo_cliente = NULL;
  }
  if (latmongo_db_nombre) {
    free(latmongo_db_nombre);
    latmongo_db_nombre = NULL;
  }

  bson_error_t uerr;
  mongoc_uri_t *muri = mongoc_uri_new_with_error(uri, &uerr);
  if (!muri) {
    latC_error(mv, "mongo: URI invalido '%s': %s", uri, uerr.message);
  }

  latmongo_cliente = mongoc_client_new_from_uri(muri);
  mongoc_uri_destroy(muri);
  if (!latmongo_cliente) {
    latC_error(mv, "mongo: no se pudo crear el cliente para '%s'", uri);
  }
  mongoc_client_set_error_api(latmongo_cliente, MONGOC_ERROR_API_VERSION_2);

  latmongo_db_nombre = strdup(db);
  if (!latmongo_db_nombre) {
    mongoc_client_destroy(latmongo_cliente);
    latmongo_cliente = NULL;
    latC_error(mv, "mongo: sin memoria para el nombre de la base de datos");
  }

  bson_error_t error;
  bson_t *ping = BCON_NEW("ping", BCON_INT32(1));
  bool ok = mongoc_client_command_simple(latmongo_cliente, db, ping, NULL,
                                         NULL, &error);
  bson_destroy(ping);

  if (!ok) {
    mongoc_client_destroy(latmongo_cliente);
    latmongo_cliente = NULL;
    free(latmongo_db_nombre);
    latmongo_db_nombre = NULL;
    latC_error(mv, "mongo: fallo la conexion a '%s': %s", uri, error.message);
  }

  apilar_entero(mv, 1);
}

void latmongo_op_desconectar(lat_mv *mv) {
  (void)mv;
  if (latmongo_cliente) {
    mongoc_client_destroy(latmongo_cliente);
    latmongo_cliente = NULL;
  }
  if (latmongo_db_nombre) {
    free(latmongo_db_nombre);
    latmongo_db_nombre = NULL;
  }
}

void latmongo_op_ping(lat_mv *mv) {
  latmongo_requerir_conexion(mv);
  bson_error_t error;
  bson_t *ping = BCON_NEW("ping", BCON_INT32(1));
  bool ok = mongoc_client_command_simple(latmongo_cliente, latmongo_db_nombre,
                                         ping, NULL, NULL, &error);
  bson_destroy(ping);
  if (!ok) {
    latC_error(mv, "mongo: ping fallo: %s", error.message);
  }
  apilar_entero(mv, 1);
}

void latmongo_op_insertar(lat_mv *mv) {
  lat_objeto *o_doc = latC_desapilar(mv);
  lat_objeto *o_col = latC_desapilar(mv);
  const char *col = latC_checar_cadena(mv, o_col);

  if (!latmongo_es_dict(o_doc)) {
    latC_error(mv, "mongo: insertar espera un diccionario, ej: {\"nombre\": \"Cafe\"}");
  }

  bson_t doc = BSON_INITIALIZER;
  latmongo_dict_a_bson(mv, o_doc, &doc);

  mongoc_collection_t *collection = latmongo_obtener_coleccion(mv, col);
  bson_error_t error;
  bool ok = mongoc_collection_insert_one(collection, &doc, NULL, NULL, &error);

  bson_destroy(&doc);
  mongoc_collection_destroy(collection);

  if (!ok) {
    latC_error(mv, "mongo: error al insertar en '%s': %s", col, error.message);
  }
  apilar_entero(mv, 1);
}

void latmongo_op_insertar_varios(lat_mv *mv) {
  lat_objeto *o_docs = latC_desapilar(mv);
  lat_objeto *o_col = latC_desapilar(mv);
  const char *col = latC_checar_cadena(mv, o_col);

  if (!isLista(o_docs)) {
    latC_error(mv, "mongo: insertar_varios espera una lista de diccionarios");
  }

  bson_t **docs = (bson_t **)calloc(LATMONGO_MAX_DOCS, sizeof(bson_t *));
  if (!docs) {
    latC_error(mv, "mongo: sin memoria para el lote de insercion");
  }

  size_t count = 0;
  lista *l = getLista(o_docs);
  for (nodo_lista *n = l->primero; n != NULL; n = n->siguiente) {
    if (count >= LATMONGO_MAX_DOCS) {
      latC_error(mv, "mongo: insertar_varios admite max %d documentos (recibidos mas)",
                 LATMONGO_MAX_DOCS);
    }
    lat_objeto *d = (lat_objeto *)n->valor;
    if (!latmongo_es_dict(d)) continue;
    bson_t *b = bson_new();
    if (!b) {
      latC_error(mv, "mongo: sin memoria para documento del lote");
    }
    latmongo_dict_a_bson(mv, d, b);
    docs[count++] = b;
  }

  if (count == 0) {
    latC_error(mv, "mongo: la lista no contiene diccionarios validos");
  }

  mongoc_collection_t *collection = latmongo_obtener_coleccion(mv, col);
  bson_error_t error;
  bool ok = mongoc_collection_insert_many(collection, (const bson_t **)docs,
                                          count, NULL, NULL, &error);

  for (size_t i = 0; i < count; i++) {
    bson_destroy(docs[i]);
  }
  free(docs);
  mongoc_collection_destroy(collection);

  if (!ok) {
    latC_error(mv, "mongo: error al insertar en '%s': %s", col, error.message);
  }
  apilar_entero(mv, (int)count);
}

void latmongo_op_buscar_uno(lat_mv *mv) {
  lat_objeto *o_filtro = latC_desapilar(mv);
  lat_objeto *o_col = latC_desapilar(mv);
  const char *col = latC_checar_cadena(mv, o_col);

  if (!latmongo_es_dict(o_filtro)) {
    latC_error(mv, "mongo: buscar_uno espera un diccionario como filtro");
  }

  bson_t query = BSON_INITIALIZER;
  latmongo_dict_a_bson(mv, o_filtro, &query);

  mongoc_collection_t *collection = latmongo_obtener_coleccion(mv, col);
  mongoc_cursor_t *cursor =
      mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

  const bson_t *doc;
  if (mongoc_cursor_next(cursor, &doc)) {
    latC_apilar(mv, latmongo_bson_a_dict(mv, doc));
  } else {
    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
      latC_error(mv, "mongo: error en la consulta a '%s': %s", col,
                 error.message);
    } else {
      latC_apilar(mv, &latO_nulo_);
    }
  }

  mongoc_cursor_destroy(cursor);
  bson_destroy(&query);
  mongoc_collection_destroy(collection);
}

void latmongo_op_buscar(lat_mv *mv) {
  lat_objeto *o_filtro = latC_desapilar(mv);
  lat_objeto *o_col = latC_desapilar(mv);
  const char *col = latC_checar_cadena(mv, o_col);

  if (!latmongo_es_dict(o_filtro)) {
    latC_error(mv, "mongo: buscar espera un diccionario como filtro");
  }

  bson_t query = BSON_INITIALIZER;
  latmongo_dict_a_bson(mv, o_filtro, &query);

  mongoc_collection_t *collection = latmongo_obtener_coleccion(mv, col);
  mongoc_cursor_t *cursor =
      mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

  lista *resultados = latL_crear(mv);
  const bson_t *doc;
  while (mongoc_cursor_next(cursor, &doc)) {
    if (latL_longitud(resultados) >= LATMONGO_MAX_RESULTS) {
      latC_error(mv,
                 "mongo: buscar supero el tope de %d resultados en '%s'; "
                 "usa un filtro mas especifico",
                 LATMONGO_MAX_RESULTS, col);
    }
    latL_agregar(mv, resultados, latmongo_bson_a_dict(mv, doc));
  }
  bson_error_t error;
  if (mongoc_cursor_error(cursor, &error)) {
    latC_error(mv, "mongo: error en la consulta a '%s': %s", col, error.message);
  }

  latC_apilar(mv, latC_crear_lista(mv, resultados));

  mongoc_cursor_destroy(cursor);
  bson_destroy(&query);
  mongoc_collection_destroy(collection);
}

void latmongo_op_actualizar(lat_mv* mv) {
  lat_objeto* o_update = latC_desapilar(mv);
  lat_objeto* o_filtro = latC_desapilar(mv);
  lat_objeto* o_col = latC_desapilar(mv);
  const char* col = latC_checar_cadena(mv, o_col);

  if (!latmongo_es_dict(o_filtro)) {
    latC_error(mv, "mongo: actualizar espera un diccionario como filtro");
  }
  if (!latmongo_es_dict(o_update)) {
    latC_error(mv,
               "mongo: actualizar espera un diccionario de cambios con "
               "operadores, ej: {\"$set\": {\"edad\": 31}}");
  }

  bson_t query = BSON_INITIALIZER;
  bson_t update = BSON_INITIALIZER;
  latmongo_dict_a_bson(mv, o_filtro, &query);
  latmongo_dict_a_bson(mv, o_update, &update);

  mongoc_collection_t* collection = latmongo_obtener_coleccion(mv, col);
  bson_t reply;
  bson_error_t error;
  bool ok = mongoc_collection_update_one(collection, &query, &update, NULL,
                                         &reply, &error);

  long modificados = ok ? reply_contador(&reply, "modifiedCount") : 0;

  bson_destroy(&reply);
  bson_destroy(&update);
  bson_destroy(&query);
  mongoc_collection_destroy(collection);

  if (!ok) {
    latC_error(mv, "mongo: error al actualizar en '%s': %s", col,
               error.message);
  }
  apilar_entero(mv, (int)modificados);
}

void latmongo_op_eliminar(lat_mv* mv) {
  lat_objeto* o_filtro = latC_desapilar(mv);
  lat_objeto* o_col = latC_desapilar(mv);
  const char* col = latC_checar_cadena(mv, o_col);

  if (!latmongo_es_dict(o_filtro)) {
    latC_error(mv, "mongo: eliminar espera un diccionario como filtro");
  }

  bson_t query = BSON_INITIALIZER;
  latmongo_dict_a_bson(mv, o_filtro, &query);

  mongoc_collection_t* collection = latmongo_obtener_coleccion(mv, col);
  bson_t reply;
  bson_error_t error;
  bool ok =
      mongoc_collection_delete_one(collection, &query, NULL, &reply, &error);

  long eliminados = ok ? (long)reply_contador(&reply, "deletedCount") : 0;

  bson_destroy(&reply);
  bson_destroy(&query);
  mongoc_collection_destroy(collection);

  if (!ok) {
    latC_error(mv, "mongo: error al eliminar en '%s': %s", col, error.message);
  }
  apilar_entero(mv, (int)eliminados);
}

void latmongo_op_contar(lat_mv* mv) {
  lat_objeto* o_filtro = latC_desapilar(mv);
  lat_objeto* o_col = latC_desapilar(mv);
  const char* col = latC_checar_cadena(mv, o_col);

  if (!latmongo_es_dict(o_filtro)) {
    latC_error(mv, "mongo: contar espera un diccionario como filtro");
  }

  bson_t query = BSON_INITIALIZER;
  latmongo_dict_a_bson(mv, o_filtro, &query);

  mongoc_collection_t* collection = latmongo_obtener_coleccion(mv, col);
  bson_error_t error;
  int64_t n = mongoc_collection_count_documents(collection, &query, NULL, NULL,
                                                NULL, &error);

  bson_destroy(&query);
  mongoc_collection_destroy(collection);

  if (n < 0) {
    latC_error(mv, "mongo: error al contar en '%s': %s", col, error.message);
  }
  apilar_entero(mv, (int)n);
}
