#include "latmongo.h"
#include <stdlib.h>
#include <string.h>

mongoc_client_t* latmongo_cliente = NULL;
char* latmongo_db_nombre = NULL;
bool latmongo_iniciado = false;

/* ============================================================
 * latH_hash — FNV-1a, copiado de src/latdic.c del proyecto
 * Latino (MIT). El latino.exe instalado NO exporta este simbolo,
 * asi que proveemos una copia local que DEBE coincidir bit a bit
 * con la del interprete (verificado con doc.nombre en runtime).
 * ============================================================ */
int latH_hash(const char *key) {
  unsigned int h = 5381u;
  while (*key) {
    h = h * 33u + (unsigned char)*key++;
  }
  return (int)(h % 256);
}

void latmongo_requerir_conexion(lat_mv* mv) {
  if (!latmongo_cliente) {
    latC_error(mv, "mongo: no hay conexion, llama a mongo.conectar(uri, db)");
  }
}

mongoc_collection_t* latmongo_obtener_coleccion(lat_mv* mv, const char* col) {
  latmongo_requerir_conexion(mv);
  if (col[0] == '\0') {
    latC_error(mv, "mongo: el nombre de la coleccion no puede ser vacio");
  }
  return mongoc_client_get_collection(latmongo_cliente, latmongo_db_nombre,
                                      col);
}

bool latmongo_es_dict(lat_objeto* o) {
  return getTipo(o) == T_DIC || getTipo(o) == T_CONTEXT;
}

lat_objeto* latmongo_crear_dic(lat_mv* mv) {
  hash_map* hm = (hash_map*)calloc(1, sizeof(hash_map));
  if (!hm) {
    latC_error(mv, "mongo: sin memoria para diccionario de resultados");
  }
  return latC_crear_dic(mv, hm);
}

void latmongo_dic_insertar(lat_mv* mv, lat_objeto* dic, const char* key,
                           lat_objeto* val) {
  hash_map* hm = getDic(dic);
  size_t klen = strnlen(key, LATMONGO_KEY_SIZE);
  if (klen >= LATMONGO_KEY_SIZE) {
    latC_error(mv, "mongo: clave demasiado larga (max %d caracteres): '%s'",
               LATMONGO_KEY_SIZE - 1, key);
  }
  int b = latH_hash(key);
  if (hm->buckets[b] == NULL) {
    hm->buckets[b] = latL_crear(mv);
  }
  hash_val* hv = (hash_val*)malloc(sizeof(hash_val));
  if (!hv) {
    latC_error(mv, "mongo: sin memoria para entrada del diccionario");
  }
  memcpy(hv->llave, key, klen + 1);
  hv->valor = val;
  latL_agregar(mv, hm->buckets[b], hv);
  hm->longitud++;
}
