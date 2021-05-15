# Servidor web

Repositorio para realizar la practica I de redes II.

## Requisitos

Para poder ejecutar el servidor será necesario tener instalado:

- gcc
- make

## Instrucciones de uso
```sh
cat server.ini # Muestra la configuración del servidor
make
make run
```

Una vez ejecutado el servidor se puede acceder a una pagina de prueba
accediendo a la siguiente URL:

[localhost:3490/index.html](localhost:3490/index.html)

**Nota**: Si se modifica el puerto en el que se ejecuta el servidor en el
archivo de configuración el enlace no funcionará.

## Notas

La pagina web de ejemplo contiene un enlace a un video que debería estar
en la ruta [./www/media](./www/media) pero que no se encontraba en los archivos
suministrados por los profesores de la asignatura. No obstante, para probar
dicha funcionalidad seria suficiente con colocar cualquier video en la ruta
especificada anteriormente con el nombre *video.mpg*.
