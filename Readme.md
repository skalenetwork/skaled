SKALED - Cliente SKALE C ++
Discordia

Skaled es un cliente de blockchain de prueba de participación de SKALE, compatible con el ecocistema ETH, que incluye EVM, Solidity, Metamask y Truffle. Utiliza el motor de consenso SKALE BFT . Actualmente, SKALE Labs lo desarrolla y mantiene activamente, y está destinado a ser utilizado para cadenas SKALE (cadenas laterales elásticas).

Forklessness
Skaled no tiene horquillas, lo que significa que blockchain es una cadena lineal (y no un árbol de horquillas como con ETH 1.0). Evidentemente, cada bloque se finaliza en un tiempo finito.

Producción de bloques asincrónicos
Skaled es asincrónico, lo que significa que el consenso sobre el siguiente bloque comienza inmediatamente después de que se finaliza el bloque anterior. No hay un intervalo de tiempo de bloque establecido. Esto permite la producción de bloques en un segundo en caso de una red rápida, lo que permite Dapps interactivos.

Seguridad demostrable
Skaled es el único cliente de PoS compatible con ETH comprobadamente seguro. La seguridad se demuestra bajo el supuesto de un máximo de t nodos maliciosos, donde el número total de nodos N es mayor o igual a 3t + 1.

Supervivencia
Se supone que la red es totalmente asincrónica, lo que significa que no hay límite superior para el tiempo de entrega de paquetes. En caso de una división temporal de la red, el protocolo puede esperar indefinidamente hasta que se resuelva la división y luego reanudar la producción normal de bloques.

Orígenes históricos
Históricamente, skaled comenzó bifurcando Aleth (anteriormente conocido como el proyecto cpp-ethereum ). Agradecemos al equipo original de cpp-ethereum por sus contribuciones.

Construyendo desde la fuente
Requisitos del sistema operativo
Skaled se compila y se ejecuta en Ubuntu 16.04 y 18.04

Repositorio de clonación
git clone --recurse-submodules https://github.com/skalenetwork/skaled.git
cd skaled
⚠️Nota: Debido a que este repositorio depende de submódulos adicionales, es importante pasar --recurse-submodulesal git clonecomando.

Si ya ha clonado el repositorio y olvidó pasar --recurse-submodules, ejecutegit submodule update --init --recursive

Instale los paquetes de Ubuntu necesarios
sudo apt-get update
sudo apt-get install autoconf build-essential cmake libprocps-dev libtool texinfo wget yasm flex bison btrfs-progs
Construir dependencias
cd deps
./build.sh
Configurar y construir skaled
# Configure el proyecto y cree un directorio de construcción.
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE = Depurar
# Construya todos los objetivos predeterminados usando todos los núcleos. 
cmake --build build - -j $ ( nproc )
Nota: Actualmente, solo se admite la compilación de depuración.

Pruebas
Para ejecutar las pruebas:

cd build/test
./testeth -- --all
Documentación
en proceso

Contribuyendo
¡Buscamos activamente colaboradores y tenemos grandes recompensas!

Lea CONTRIBUTING y CODING_STYLE detenidamente antes de realizar modificaciones en la base del código. Este proyecto se adhiere al código de conducta de SKALE. Al participar, se espera que respete este código.

Usamos problemas de GitHub para rastrear solicitudes y errores, así que consulte nuestras preguntas generales de desarrollo y discusión sobre Discord .

¡Todas las contribuciones son bienvenidas! Intentamos mantener una lista de tareas que sean adecuadas para los recién llegados bajo la etiqueta ayuda deseada . Si usted tiene alguna pregunta, por favor pregunte.

Discordia

Todo el desarrollo va en la rama de desarrollo.

Nota sobre minería
La red SKALE utiliza Proof-of-Stake, por lo que este proyecto no es adecuado para la minería de Ethereum .

Para más información
Sitio web de SKALE Labs
Twitter de SKALE Labs
Blog de SKALE Labs
Obtenga más información sobre la comunidad SKALE en Discord .

Licencia
Licencia

Todas las contribuciones se realizan bajo la Licencia Pública General GNU v3 . Ver LICENCIA .

Todo el código original cpp-ethereum Copyright (C) Autores Aleth.
Todas las modificaciones de cpp-ethereum Copyright (C) SKALE Labs.
Todo el código skaled Copyright (C) SKALE Labs.
