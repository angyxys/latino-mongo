#include "latmongo.h"

static const lat_CReg lib_mongo[] = {
  {"conectar", latmongo_op_conectar, 2},
  {"desconectar", latmongo_op_desconectar, 0},
  {"ping", latmongo_op_ping, 0},
  {"insertar", latmongo_op_insertar, 2},
  {"insertar_varios", latmongo_op_insertar_varios, 2},
  {"buscar_uno", latmongo_op_buscar_uno, 2},
  {"buscar", latmongo_op_buscar, 2},
  {"actualizar", latmongo_op_actualizar, 3},
  {"eliminar", latmongo_op_eliminar, 2},
  {"contar", latmongo_op_contar, 2},
  {NULL, NULL, 0},
};

LATINO_API void latC_abrir_liblatino_mongo(lat_mv* mv) {
  latC_abrir_liblatino(mv, "mongo", lib_mongo);
}
