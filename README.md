# MiniHTTP (Exipiretisi)


MiniHTTP es un servidor web HTTP/1.1 desarrollado en C que implementa un modelo de concurrencia basada en event-driven por medio de sockets no bloquenates y de epoll. El servidor web unicamente cuenta con un solo hilo de ejecución. Para almacenar las solicitudes utiliza
una estructura de datos con un buffer circular.El unico metodo permitido es GET y esta diseñado para responder con archivos estaticos.

## Caracteristicas

- En caso de recibir la ruta / entrega el archivo por defecto index.html.
- Las conexiones son persistentes por defecto. El cliente puede solicitar el cierre con el uso del encabezado Connection: close.
- El servidor determina el encabezado Content-Type a partir de la extensión del archivo es capaz de enviar el Mime correcto de archivos HTML, CSS, JavaScript, PNG y JPEG. Para otras extensiones usa text/plain.
- Uso de openat2 para el control de la resolucion de la ruta para evitar ataques de Directory Traversal
- Uso de metodos seguros para el manejo de strings con el fin de evitar BufferOverFlow
- Comprobacion 

## Requisitos

- Kernel Linux
- GCC

## Compilar

Desde la raíz del repositorio, ejecuta make para generar main.out.

```sh
make
```
o con GCC.

```sh
gcc -I. -o main.out src/*.c
```

make clean elimina el ejecutable generado.

## Ejecución

Inicia el servidor desde la raíz del repositorio para que encuentre `./www/`.

```sh
./main.out
```

make run inicia el servidor y compila en caso que no exista. El proceso escucha en el puerto `8025` de todas las interfaces IPv4 (`0.0.0.0`). (http://127.0.0.1:8025/) el puerto y la interfaz se configura en el archivo server con las variables IP_SERVER y SERVICE_OR_PORT

Para cambiar la ruta de trabajo se modifica la variable web_cd en files y para el archivo por defecto default_file


## Estructura


```text
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── server.h
│   ├── http.h
│   ├── files.h
│   └── mime.h
├── src/
│   ├── main.c
│   ├── server.c
│   ├── http.c
│   ├── files.c
│   └── mime.c
└── www/
    ├── index.html
    ├── style.css
    ├── script.js
    └── image.png
```

- server.c: Socket de escucha, conexiones de clientes, bucle de eventos y manejo de epoll.
- main.c: Punto de entrada.
- http.c: Bufer circular, analisis de solicitudes y encabezados de respuesta.
- files.c: Resolucion de rutas, metadatos y envio de archivos.
- mime.c: Seleccion del tipo de contenido por extensión.
- www/: Carpeta de la cual se buscan y entrega los recursos.

## Como funciona

El servidor abre el directorio de trabajo, crea el socket TCP de escucha y lo registra en epoll, al aceptar una conexión, registra el socket del cliente y reserva su bufer circular. Los datos recibidos se acumulan hasta encontrar el fin de los encabezados (\r\n\r\n).El analizador valida la línea de solicitud y el resto de la solicitud. Para luego responder a la solicitud en base a los codigos de respuesta de HTTP permitidos.

## Limitaciones

- El servidor web no responde a URI que contengan fragmentos o querrys.
- El servidor no descarta el body de una solicitud por lo que descarta la siguiente peticion despues del body
- El servidor no implementa un mecanismo para cerrar conexiones en caso que estos no envien el cierre ordenado de TCP
- El manejo de escrituras parciales es incompleto. Una transferencia puede interrumpirse si el buffer de salida del socket se llena cuando 
se envia el encabezado.
- El servidor no implementa un manejo de signals para la administracion
- El servidor no usa syslog para registrar evento o errores.


## Autor
Carrillo Cristhian
[CriOwl](https://github.com/CriOwl)

## Referencias

- [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112)
- [HTTP: The Definitive Guide](https://learning.oreilly.com/library/view/http-the-definitive/1565925092/)
- [The Linux Programming Interface](https://learning.oreilly.com/library/view/the-linux-programming/9781593272203/)
