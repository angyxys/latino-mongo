#ifndef LATMONGO_H
#define LATMONGO_H

/* ============================================================
 * latmongo.h — cabecera interna de latino-mongo
 *
 * ORDEN OBLIGATORIO: mongoc ANTES que latino.h.
 * latcompat.h (via latino.h) define getcwd como macro y rompe
 * las declaraciones de los headers de MinGW si llega primero.
 * ============================================================ */

#ifdef _WIN32
#define LATINO_BUILD_AS_DLL
#endif

#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif

#define LATINO_LIB

#include <mongoc/mongoc.h>

#undef WIN32

#include <latino.h>
#include <stdbool.h>

#define LATMONGO_MAX_DOCS 256
#define LATMONGO_MAX_DEPTH 64
#define LATMONGO_MAX_RESULTS 10000
#define LATMONGO_KEY_SIZE 64

extern mongoc_client_t* latmongo_cliente;
extern char* latmongo_db_nombre;
extern bool latmongo_iniciado;

void latmongo_requerir_conexion(lat_mv* mv);
mongoc_collection_t* latmongo_obtener_coleccion(lat_mv* mv, const char* col);
bool latmongo_es_dict(lat_objeto* o);
lat_objeto* latmongo_crear_dic(lat_mv* mv);
void latmongo_dic_insertar(lat_mv* mv, lat_objeto* dic, const char* key,
                           lat_objeto* val);

void latmongo_dict_a_bson(lat_mv* mv, lat_objeto* o, bson_t* doc);
lat_objeto* latmongo_bson_a_dict(lat_mv* mv, const bson_t* doc);

void latmongo_op_conectar(lat_mv* mv);
void latmongo_op_desconectar(lat_mv* mv);
void latmongo_op_ping(lat_mv* mv);
void latmongo_op_insertar(lat_mv* mv);
void latmongo_op_insertar_varios(lat_mv* mv);
void latmongo_op_buscar_uno(lat_mv* mv);
void latmongo_op_buscar(lat_mv* mv);
void latmongo_op_actualizar(lat_mv *mv);
void latmongo_op_eliminar(lat_mv *mv);
void latmongo_op_contar(lat_mv *mv);

#endif
