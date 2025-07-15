# TP_PROTOS-grupo7

**Compilación**

Para compilar el proyecto, asegurate de tener ubicados todos los archivos provistos en la raíz del proyecto. Luego, ejecutá:

```bash
make clean all
```

IMPORTANTE Es necesario tener instaladas las versiones más recientes de GCC y Make.


**Ejecución**

*Servidor: socks5d*

Una vez compilado, el binario socks5d se encontrará en la carpeta ./bin/. Para ejecutarlo:

```bash
./bin/socks5d [argumentos]
```

Podés consultar todos los argumentos disponibles utilizando el flag -h:

```bash
./bin/socks5d -h
```

*Cliente: client*

El cliente también se encuentra en ./bin/ y se ejecuta de la siguiente forma:

```sh
./bin/client [host] [port]
```

host: Dirección IP del servidor de monitoreo (por defecto 127.0.0.1)

port: Puerto del servidor de monitoreo (por defecto 1081)

Podés ver la ayuda completa con:

```sh
./bin/client -h
```
