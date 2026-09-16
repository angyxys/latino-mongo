cmake_minimum_required(VERSION 3.10)
project(latino-mongo C)

if(WIN32)
  set(LATINO_PATH    "C:/Program Files/Latino"         CACHE FILEPATH "Ruta de latino.exe")
  set(LATINO_INCLUDE "C:/Program Files/Latino/include" CACHE FILEPATH "Ruta de include")
  include_directories(${LATINO_INCLUDE})
  link_directories("C:/Program Files/Latino/lib")
endif()

add_library(latino-mongo SHARED
  src/latino-mongo.c
  src/latmongo-state.c
  src/latmongo-convert.c
  src/latmongo-ops.c
)
set_target_properties(latino-mongo PROPERTIES PREFIX "")

target_link_libraries(latino-mongo PRIVATE
  mongoc2
  bson2
  latino
)

install(TARGETS latino-mongo DESTINATION ${LATINO_PATH})
