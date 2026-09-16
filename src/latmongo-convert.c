#include "latmongo.h"
#include <stdio.h>
#include <string.h>

static void valor_a_bson(lat_mv* mv, lat_objeto* val, bson_t* doc,
                         const char* k, int klen, int depth) {
  if (depth > LATMONGO_MAX_DEPTH) {
    latC_error(mv,
               "mongo: documento anidado demasiado profundo (max %d niveles)",
               LATMONGO_MAX_DEPTH);
  }

  if (isCadena(val)) {
    const char* s = latC_checar_cadena(mv, val);
    bson_append_utf8(doc, k, klen, s, -1);
  } else if (isEntero(val)) {
    bson_append_int32(doc, k, klen, (int32_t)getEntero(val));
  } else if (isNumerico(val)) {
    bson_append_double(doc, k, klen, getNumerico(val));
  } else if (isLogico(val)) {
    bson_append_bool(doc, k, klen, getLogico(val));
  } else if (isNulo(val)) {
    bson_append_null(doc, k, klen);
  } else if (latmongo_es_dict(val)) {
    bson_t sub = BSON_INITIALIZER;

    hash_map* hm = getDic(val);
    for (int b = 0; b < 256; b++) {
      lista* l = hm->buckets[b];
      if (!l) continue;
      for (nodo_lista* n = l->primero; n != NULL; n = n->siguiente) {
        hash_val* hv = (hash_val*)n->valor;
        char kbuf[LATMONGO_KEY_SIZE + 1];
        memcpy(kbuf, hv->llave, LATMONGO_KEY_SIZE);
        kbuf[LATMONGO_KEY_SIZE] = '\0';
        valor_a_bson(mv, hv->valor, &sub, kbuf,
                     (int)strnlen(kbuf, LATMONGO_KEY_SIZE), depth + 1);
      }
    }
    bson_append_document(doc, k, klen, &sub);
    bson_destroy(&sub);
  } else if (isLista(val)) {
    bson_t arr = BSON_INITIALIZER;
    char idx[16];
    uint32_t i = 0;
    lista* l = getLista(val);
    for (nodo_lista* n = l->primero; n != NULL; n = n->siguiente) {
      snprintf(idx, sizeof(idx), "%u", i++);
      valor_a_bson(mv, (lat_objeto*)n->valor, &arr, idx, -1, depth + 1);
    }
    bson_append_array(doc, k, klen, &arr);
    bson_destroy(&arr);
  } else {
    latC_error(mv, "mongo: tipo no soportado en documento (clave: %s)", k);
  }
}

void latmongo_dict_a_bson(lat_mv* mv, lat_objeto* o, bson_t* doc) {
  hash_map* hm = getDic(o);
  for (int b = 0; b < 256; b++) {
    lista* l = hm->buckets[b];
    if (!l) continue;
    for (nodo_lista* n = l->primero; n != NULL; n = n->siguiente) {
      hash_val* hv = (hash_val*)n->valor;

      char kbuf[LATMONGO_KEY_SIZE + 1];
      memcpy(kbuf, hv->llave, LATMONGO_KEY_SIZE);
      kbuf[LATMONGO_KEY_SIZE] = '\0';
      valor_a_bson(mv, hv->valor, doc, kbuf,
                   (int)strnlen(kbuf, LATMONGO_KEY_SIZE), 0);
    }
  }
}

static lat_objeto* valor_a_latino(lat_mv* mv, bson_iter_t* it, int depth) {
  if (depth > LATMONGO_MAX_DEPTH) {
    latC_error(mv, "mongo: documento BSON demasiado profundo (max %d niveles)",
               LATMONGO_MAX_DEPTH);
  }

  switch (bson_iter_type(it)) {
    case BSON_TYPE_UTF8:
      return latC_crear_cadena(mv, bson_iter_utf8(it, NULL));
    case BSON_TYPE_DOUBLE:
      return latC_crear_numerico(mv, bson_iter_double(it));
    case BSON_TYPE_INT32:
      return latC_crear_entero(mv, bson_iter_int32(it));
    case BSON_TYPE_INT64:
      return latC_crear_numerico(mv, (double)bson_iter_int64(it));
    case BSON_TYPE_BOOL:
      return latC_crear_logico(mv, bson_iter_bool(it));
    case BSON_TYPE_NULL:
      return &latO_nulo_;
    case BSON_TYPE_OID: {
      char oid[25];
      bson_oid_to_string(bson_iter_oid(it), oid);
      return latC_crear_cadena(mv, oid);
    }
    case BSON_TYPE_DOCUMENT: {
      uint32_t len;
      const uint8_t* data;
      bson_iter_document(it, &len, &data);
      bson_t* sub = bson_new_from_data(data, len);
      if (!sub) {
        latC_error(mv, "mongo: subdocumento BSON corrupto");
      }
      lat_objeto* dic = latmongo_crear_dic(mv);
      bson_iter_t cit;
      if (bson_iter_init(&cit, sub)) {
        while (bson_iter_next(&cit)) {
          latmongo_dic_insertar(mv, dic, bson_iter_key(&cit),
                                valor_a_latino(mv, &cit, depth + 1));
        }
      }
      bson_destroy(sub);
      return dic;
    }
    case BSON_TYPE_ARRAY: {
      uint32_t len;
      const uint8_t* data;
      bson_iter_array(it, &len, &data);
      bson_t* sub = bson_new_from_data(data, len);
      if (!sub) {
        latC_error(mv, "mongo: array BSON corrupto");
      }
      lista* l = latL_crear(mv);
      bson_iter_t cit;
      if (bson_iter_init(&cit, sub)) {
        while (bson_iter_next(&cit)) {
          latL_agregar(mv, l, valor_a_latino(mv, &cit, depth + 1));
        }
      }
      bson_destroy(sub);
      return latC_crear_lista(mv, l);
    }
    default:
      return &latO_nulo_;
  }
}

lat_objeto* latmongo_bson_a_dict(lat_mv* mv, const bson_t* doc) {
  lat_objeto* dic = latmongo_crear_dic(mv);
  bson_iter_t it;
  if (bson_iter_init(&it, doc)) {
    while (bson_iter_next(&it)) {
      latmongo_dic_insertar(mv, dic, bson_iter_key(&it),
                            valor_a_latino(mv, &it, 0));
    }
  }
  return dic;
}