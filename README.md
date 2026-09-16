# latino-mongo

Librería nativa (escrita en C) que conecta el lenguaje de programación
[Latino](https://github.com/lenguaje-latino/latino) con bases de datos
[MongoDB](https://www.mongodb.com/), usando el
[mongo-c-driver](https://github.com/mongodb/mongo-c-driver) oficial.

```mermaid
flowchart LR
    A[Tu programa .lat] -->|"mongo.conectar(...)"| B[latino-mongo.dll]
    B -->|API C de mongoc/bson| C[(MongoDB)]
```

## Estado del proyecto

| Función | Estado |
|---|---|
| ```mongo.conectar(uri, db)``` | ✅ Funcionando |
| ```mongo.desconectar()``` | ✅ Funcionando |
| ```mongo.insertar(col, doc)``` | ✅ Funcionando |
| ```mongo.buscar(col, filtro)``` | ✅ Funcionando |
| ```mongo.actualizar(col, filtro, doc)``` | ✅ Funcionando |
| ```mongo.eliminar(col, filtro)``` | ✅ Funcionando |

> Actualmente solo soporta **Windows** (MSYS2 UCRT64). El soporte para Linux
> está pendiente.

---

## Requisitos

- **Windows 10/11** (x64)
- [Latino](https://github.com/lenguaje-latino/latino) instalado
  (por defecto en ```C:\Program Files\Latino```)
- [MSYS2](https://www.msys2.org) con el entorno **UCRT64**
- Un servidor MongoDB corriendo (local con Docker o instalación propia)

## 1. Preparar el entorno de compilación

Abre la terminal **MSYS2 UCRT64** (importante: no la ```MSYS``` ni la ```MINGW64```,
debe ser la **UCRT64**) e instala el toolchain y el driver de MongoDB:

```bash
pacman -Syu    # actualiza MSYS2 (pide reiniciar la terminal)
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-mongo-c-driver
```

El paquete ```mongo-c-driver``` instala las librerías dinámicas que usaremos:
```libmongoc2.dll``` y ```libbson2.dll``` (junto con sus dependencias).

## 2. Compilar la librería

Desde la misma terminal UCRT64, dentro de la carpeta del proyecto:

```bash
git clone https://github.com/angyxys/latino-mongo.git
cd latino-mongo

cmake -G Ninja -B build
cmake --build build
```

Si todo sale bien, encontrarás la librería en ```build/latino-mongo.dll```.

> **Nota:** el ```CMakeLists.txt``` asume Latino instalado en
> ```C:\Program Files\Latino```. Si lo tienes en otra ruta, edita las variables
> ```LATINO_PATH``` / ```LATINO_INCLUDE``` al inicio del archivo.

## 3. Instalar la librería (y sus dependencias)

⚠️ **Este paso es clave.** En Windows, los DLLs se buscan junto al
ejecutable, por eso **todas** las librerías deben copiarse a la carpeta de
Latino, no solo la nuestra: ```latino-mongo.dll``` depende de ```libmongoc2.dll```
y ```libbson2.dll```, y estas a su vez de otras (zlib, zstd, snappy, etc.).

Pasos:

1. **Cierra** cualquier ventana UCRT64 abierta.
2. Clic derecho en *MSYS2 UCRT64* → **Ejecutar como administrador**
   (se necesitan permisos para escribir en ```C:\Program Files```).
3. Dentro de esa terminal, desde la carpeta del proyecto, ejecuta:

```bash
# la librería compilada
cp -v build/latino-mongo.dll "/c/Program Files/Latino/"

# TODAS las dependencias dinámicas del árbol del driver
for bin in build/latino-mongo.dll /ucrt64/bin/libmongoc2.dll /ucrt64/bin/libbson2.dll; do
  ldd "$bin" | grep -i ucrt64 | awk '{print $3}'
done | sort -u | while read -r f; do
  cp -v "$f" "/c/Program Files/Latino/"
done
```

Al terminar, en ```C:\Program Files\Latino``` debe existir el DLL de la librería
más aproximadamente estos 9:

```text
latino-mongo.dll     <- nuestra librería
libmongoc2.dll       <- driver MongoDB (cliente)
libbson2.dll         <- driver MongoDB (documentos BSON)
libzstd.dll          <- compresión
zlib1.dll            <- compresión
libsnappy.dll        <- compresión
libstdc++-6.dll      <- runtime de GCC
libwinpthread-1.dll  <- runtime de GCC
libutf8proc.dll      <- UTF-8
libgcc_s_seh-1.dll   <- runtime de GCC
```

> 💡 **Si actualizas MSYS2** (```pacman -Syu```) y el driver cambia de versión,
> repite este paso: las dependencias deben coincidir con lo que se compiló
> la librería.

## 4. Probar la conexión

Levanta un MongoDB local (por ejemplo con Docker):

```bash
docker run -d --name mongo -p 27017:27017 mongo
```

Crea un archivo ```test_mongo.lat``` en cualquier carpeta:

```latino
incluir("mongo")

d = {"a": 1}
escribir(d.a)
escribir(d["a"])

// [1] conectar
mongo.conectar("mongodb://localhost:27017", "tienda")
escribir("[1] conectar: OK")

// [2] ping
mongo.ping()
escribir("[2] ping: OK")

// [3] insertar — un documento como diccionario
mongo.insertar("users", {"user": "test", "edad": 30})
escribir("[3] insertar: OK")

// [4] insertar_varios — una lista de diccionarios
mongo.insertar_varios("users", [
  {"user": "ana", "edad": 25},
  {"user": "luis", "edad": 40}
])
escribir("[4] insertar_varios: OK")

// [5] buscar_uno — filtro diccionario, devuelve un diccionario
u = mongo.buscar_uno("users", {"user": "test"})
escribir("[5] buscar_uno:")
escribir(u.user)   // -> test
escribir(u.edad)   // -> 30

// [6] buscar — devuelve una lista de diccionarios
todos = mongo.buscar("users", {})
primero = todos[0]
escribir("[6] buscar: primer resultado:")
escribir(primero.user)

// [7] desconectar
mongo.desconectar()
escribir("[7] desconectar: OK")
```

Ejecútalo:

```bash
latino ejemplo.lat
```

Salida esperada:

```text
✓ Conectado a MongoDB
```

---

## API

### ```mongo.conectar(uri, db)```

Establece la conexión con el servidor MongoDB.

| Parámetro | Tipo | Descripción |
|---|---|---|
| ```uri``` | cadena | URI de conexión (ej. ```"mongodb://localhost:27017"```) |
| ```db``` | cadena | Nombre de la base de datos a usar |

Retorna ```verdadero``` si la conexión fue exitosa. Si el servidor no responde,
la ejecución se detiene con un mensaje de error descriptivo.

```latino
mongo.conectar("mongodb://localhost:27017", "mi_base")
```

### ```mongo.desconectar()```

Cierra la conexión activa y libera los recursos. Siempre llámala al terminar.

```latino
mongo.desconectar()
```

---

## Solución de problemas

### ```"The specified module could not be found"``` al usar ```incluir("mongo")```

Falta una DLL del árbol de dependencias junto a ```latino.exe```. Diagnóstico:

```bash
# en UCRT64, lista las dependencias de cada pieza
ldd build/latino-mongo.dll     | grep -i ucrt64
ldd /ucrt64/bin/libmongoc2.dll | grep -i ucrt64
ldd /ucrt64/bin/libbson2.dll   | grep -i ucrt64
```

Cada DLL listado en ```/ucrt64/bin/...``` debe existir en ```C:\Program Files\Latino```.
Copia las que falten y vuelve a probar.

### Compila pero la conexión falla

- Verifica que el servidor esté arriba: ```docker ps``` o ```mongosh```
- Prueba el URI con: ```mongosh "mongodb://localhost:27017"```

### Verificar qué exporta la librería

Si sospechas que el DLL está desactualizado o corrupto:

```bash
objdump -p build/latino-mongo.dll | grep abrir
# debe mostrar: latC_abrir_liblatino_mongo
```

### Cargar la librería fuera de Latino (prueba aislada)

En PowerShell, para saber si el problema es de dependencias o del intérprete:

```powershell
Add-Type -Name K -Namespace W -MemberDefinition '[DllImport("kernel32", SetLastError=true)] public static extern IntPtr LoadLibrary(string p);'
[W.K]::LoadLibrary("C:\Program Files\Latino\latino-mongo.dll")
```

- Devuelve un número distinto de ```0``` → la librería carga bien
- Devuelve ```0``` → falta una dependencia (revisa el paso 3)

---

## Cómo está hecho

- ```src/latino-mongo.c``` — el bridge en C: expone funciones C al intérprete de
  Latino siguiendo la [plantilla oficial de librerías](https://github.com/lenguaje-latino/latino-lib-ejemplo)
  (```lat_CReg```, ```latC_desapilar```, ```latC_apilar_*```), y habla con MongoDB
  mediante el mongo-c-driver.
- ```CMakeLists.txt``` — genera ```latino-mongo.dll``` enlazado contra ```mongoc2```,
  ```bson2``` y el intérprete de Latino.

## Créditos

- [Lenguaje Latino](https://github.com/lenguaje-latino/latino) y su creador
  [primitivorm](https://github.com/primitivorm), por la plantilla de
  librerías nativas.
- [MongoDB C Driver](https://github.com/mongodb/mongo-c-driver).

## Licencia

[MIT](LICENSE) — haz lo que quieras, mantén el aviso de copyright.
