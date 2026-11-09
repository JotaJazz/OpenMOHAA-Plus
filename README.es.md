# OpenMOHAA Plus

![Estado: en desarrollo](https://img.shields.io/badge/estado-en%20desarrollo-orange)
![Licencia: GPL-2.0](https://img.shields.io/badge/licencia-GPL--2.0-blue)
![Plataformas: Linux | Windows | macOS](https://img.shields.io/badge/plataforma-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![MOHAA | Spearhead | Breakthrough](https://img.shields.io/badge/MOHAA-AA%20%7C%20Spearhead%20%7C%20Breakthrough-6b7a3a)
[![Fork de openmoh/openmohaa](https://img.shields.io/badge/fork%20de-openmoh%2Fopenmohaa-555)](https://github.com/openmoh/openmohaa)
[![Builds](https://github.com/JotaJazz/OpenMOHAA-Plus/actions/workflows/branches-build.yml/badge.svg?branch=main)](https://github.com/JotaJazz/OpenMOHAA-Plus/actions/workflows/branches-build.yml)
[![Último commit](https://img.shields.io/github/last-commit/JotaJazz/OpenMOHAA-Plus/main)](https://github.com/JotaJazz/OpenMOHAA-Plus/commits/main)

**Español** · [English](README.md)

**Medal of Honor: Allied Assault sigue vivo.** OpenMOHAA Plus es un fork de
[OpenMoHAA](https://github.com/openmoh/openmohaa), la reimplementación de código abierto de Medal of
Honor: Allied Assault, Spearhead y Breakthrough: el cliente, el servidor dedicado y la lógica del
juego, para Windows, Linux y macOS.

Existe por una razón: **avanzar con agentes de IA para programar**, algo que el proyecto original no
acepta. Aquí cada cambio se hace con un asistente, se reproduce en el juego real antes y después, y
queda en este fork. Sobre el trabajo de OpenMoHAA corregimos bugs reportados y sumamos funciones
nuevas, como lo hace el proyecto hermano [OpenCoD2-Plus](https://github.com/JotaJazz/OpenCoD2-Plus)
para Call of Duty 2 con su servidor a 40 Hz.

<table>
  <tr>
    <td><img src="docs/assets/images/v0.60.0-x86_64/mohdm1_1.png" alt="Southern France (mohdm1)"></td>
    <td><img src="docs/assets/images/v0.60.0-x86_64/training_1.png" alt="Entrenamiento de un jugador"></td>
  </tr>
  <tr>
    <td><img src="docs/assets/images/v0.60.0-x86_64/flughafen_1.png" alt="Flughafen"></td>
    <td><img src="docs/assets/images/v0.60.0-x86_64/mohdm2_1.png" alt="Freeze-Tag con bots"></td>
  </tr>
</table>
<p align="center"><sub>Capturas de OpenMoHAA; el motor y el código del juego son los mismos.</sub></p>

## Qué funciona hoy

- **Todo lo que hace OpenMoHAA:** la campaña completa de un jugador de los tres juegos, todos los
  modos multijugador oficiales (Free-For-All, Team Deathmatch, por rondas, Objetivo, Tug-of-War,
  Liberation), mods como Freeze-Tag, bots integrados, compatibilidad con los assets, scripts y
  servidores originales ([diferencias con el original](docs/markdown/01-intro/04-differences.md)).
- **Correcciones de bugs reportados en el proyecto original**, cada una reproducida antes y después
  en el juego real:
  - `s_dialogscale` mayor que 1 ahora sube el volumen de los diálogos (#885)
  - `com_maxfps` limita a la tasa pedida: 144 da 144 fps, no 166 (#530)
  - el cronómetro del HUD sigue visible al cargar una partida (#916)
  - los scripts de mapa ya no ejecutan de golpe sus esperas tras reiniciar el mismo mapa, lo que
    desconectaba a todos en servidores de un solo mapa (#287)
  - sin crash al cambiar de mapa cuando el módulo del juego queda cargado, como en Alpine/musl (#918)
  - `sv_pure 1` ya no rechaza a clientes que leyeron un pk3 extra en el menú (#659)
  - `rehashbans` ya no conserva bans borrados (#926)
  - scripts: los comandos de `registercmd` se ejecutan (#672), `amount`/`dmamount` de los ítems se
    pueden leer (#920), `waitForState` espera (#702)
  - cargan los sonidos cuyo alias omite `.wav` (#589), una doble liberación del cursor, y archivos de
    release ejecutables en Linux y macOS (#923)
- **Nuevo en Plus:**
  - servidores a 40 Hz (`sv_fps 40`) y tiempos por frame con `sv_perf`
  - eventos de script para cambiar el daño y rechazar cambios de equipo, eventos de ronda
    ([docs](docs/markdown/04-coding/02-scripting/03-plus-events.md)), funciones HTTP y JSON
    compatibles con NightFall ([docs](docs/markdown/04-coding/02-scripting/04-plus-libmoh.md)),
    scripts de servidor sin sobrescribir archivos del juego (`sv_scripts`), y un
    [mod de estadísticas](misc/mods/plus-stats/README.md) hecho con ellos
  - [cuentas de administrador en el juego](docs/markdown/03-configuration/04-plus-admin.md) con
    roles y permisos por comando, sin la contraseña de RCON (`ad_login`)
  - [tamaño del HUD y de la mira, y menús en 4:3](docs/markdown/03-configuration/05-plus-hud.md) en pantallas anchas (`ui_hudscale`, `cg_crosshair_size`, `ui_menuaspect`)
  - [favoritos, historial y búsqueda en el buscador de servidores](docs/markdown/03-configuration/06-plus-server-browser.md),
    y el juego ya no se congela cuando el DNS de los masters no responde (#376)
  - [fuentes con tildes y ñ](docs/markdown/03-configuration/09-plus-fonts.md), más nítidas en alta resolución
  - [traducciones de la comunidad](docs/markdown/03-configuration/07-plus-translations.md) para los textos del juego (`cl_language`)
  - un [mod de partidas competitivas](misc/mods/plus-match/README.md) (cambio de lado en el entretiempo, tiempos muertos)
  - [pausa de partida, sonidos y marcadores de impacto](docs/markdown/03-configuration/08-plus-match-control.md) controlados por el servidor
  - descarga automática de mods por HTTP (`sv_dlURL`), el motivo cuando un servidor te expulsa,
    audio HRTF para audífonos (`s_hrtf 1`)
  - un validador de scripts y una extensión de VS Code ([misc/scr-tools](misc/scr-tools),
    [misc/vscode-mohaa](misc/vscode-mohaa)), reportes de congelamientos y crashes, y una suite de
    tests sin ventanas

  Los números son [issues del proyecto original](https://github.com/openmoh/openmohaa/issues). La
  lista completa, con los riesgos de cada cambio, está en
  [docs/ai/SYSTEM_SUMMARY.md](docs/ai/SYSTEM_SUMMARY.md) y
  [docs/dev/roadmap/debt.md](docs/dev/roadmap/debt.md).

## Lo que viene

Funciones nuevas más allá del juego original, en el espíritu de OpenCoD2-Plus. Planeado, todavía no
hecho:

- **Modo competición** (estilo zPAM) con más eventos de gametype: puntaje por equipo, cambio de
  lado en el entretiempo.
- **Mejores bots** y compensación de lag.
- **Más bugs del proyecto original**, elegidos según lo que se pueda reproducir y verificar.
- **La misma corrección para el módulo del cliente en musl** que recibió el servidor en #918.

> [!NOTE]
> **Todavía en desarrollo.** Espera bugs y piezas faltantes. Los cambios de comportamiento respecto
> del original están en [docs/dev/roadmap/debt.md](docs/dev/roadmap/debt.md).

> [!IMPORTANT]
> **No incluye contenido del juego: trae tus propios datos.** Este repositorio es solo código
> fuente. Para jugar necesitas los archivos de una copia de Medal of Honor: Allied Assault (y
> opcionalmente Spearhead y Breakthrough) que **tengas legalmente**.

## Cómo jugar

Las builds de desarrollo se publican automáticamente en la
[pre-release `dev`](https://github.com/JotaJazz/OpenMOHAA-Plus/releases/tag/dev). Si no, compílalo
(ver [Compilar](#compilar)). En Linux y Steam Deck lo más simple es el AppImage: pon
`OpenMOHAA-Plus-x86_64.AppImage` en la carpeta del juego (la que tiene `main/`), dale `chmod +x` y
ejecútalo (`--dedicated`, `--spearhead` o `--breakthrough` para el servidor y las expansiones). La
instalación funciona igual que en OpenMoHAA (guías en inglés):

- [Instalación](docs/markdown/01-intro/01-installation.md), [ejecutar el juego y las expansiones](docs/markdown/02-running/01-running.md), [preguntas frecuentes](docs/markdown/02-running/03-faq.md)
- [Montar un servidor](docs/markdown/02-running/02-running-server.md), [bots](docs/markdown/03-configuration/03-configuration-bots.md), [configuración](docs/markdown/03-configuration/01-configuration.md)

Desde una build, apúntalo a la carpeta del juego, la que contiene `main/` (no hace falta si la build
está en esa carpeta, con una instalación de GOG en Windows, o con la carpeta `EA GAMES\MOHAA` del
instalador original, también dentro del prefijo de Wine por defecto):

```sh
./openmohaa +set fs_basepath /ruta/a/MOHAA
./omohaaded +set fs_basepath /ruta/a/MOHAA +map dm/mohdm1     # servidor dedicado
```

## Para contribuir

Jugar y reportar bugs, probar en otros sistemas, o código: toda ayuda cuenta. No envíes los cambios
de este fork al repositorio original: sus reglas prohíben contribuciones generadas con IA. La
documentación técnica, en inglés, empieza en [docs/dev/README.md](docs/dev/README.md);
[AGENTS.md](AGENTS.md) es el punto de entrada para agentes de IA.

## Compilar

CMake y Ninja. Dependencias para cada plataforma:
[docs/markdown/04-coding/01-compiling.md](docs/markdown/04-coding/01-compiling.md). En Linux:

```sh
cmake -S . -B .cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$HOME/omohaa
cmake --build .cmake
cmake --install .cmake        # -> $HOME/omohaa/lib/openmohaa: openmohaa, omohaaded, game.so, cgame.so...
```

Más detalles en [docs/dev/development/compiling.md](docs/dev/development/compiling.md).

## Tests

```sh
python3 -I -m unittest discover -s misc/docs-check     # tests del checker de documentación
python3 -I misc/docs-check/check_docs.py               # checker de documentación
OMOH_GAME_DIR=/ruta/a/MOHAA python3 -I -m unittest discover -s tests   # tests del juego, sin ventanas
```

Los bugs se verifican con los binarios reales sin abrir ventanas ni reproducir sonido: cliente
offscreen, audio nulo, comandos de consola por script, bots, capturas de pantalla y audio renderizado
a un archivo. Las recetas están en
[docs/dev/development/testing.md](docs/dev/development/testing.md).

## Estructura del repositorio

El árbol de código, los binarios que se generan y cómo encajan el motor, los módulos del juego y el
renderer: [docs/dev/architecture/overview.md](docs/dev/architecture/overview.md).

## Transparencia

- **Quién lo mantiene:** soy JotaJazz. Me gustan los juegos antiguos y quiero seguir jugándolos con
  mis amigos y mantenerlos vivos para la comunidad, con proyectos abiertos. Este está junto a
  [OpenCoD2-Plus](https://github.com/JotaJazz/OpenCoD2-Plus).
- **Cómo se trabaja:** trabajo con [Claude Code](https://claude.com/claude-code), el asistente de
  programación de Anthropic. Los cambios de este fork, incluido este README, se escriben con su
  ayuda. Los commits hechos así llevan una línea `Co-Authored-By: Claude`, para identificarlos con
  `git log`.
- **Relación con el original:** el motor y la reimplementación del juego son obra de los
  contribuidores de OpenMoHAA. Este fork sigue su `main` y lo integra; nunca les envía sus propios
  cambios, porque OpenMoHAA no acepta contribuciones generadas con IA
  ([ADR 0001](docs/dev/architecture/adr/0001-fork-keeps-ai-assisted-work.md)). Reporta los bugs de
  este fork aquí, no a OpenMoHAA.
- **Lo que no es:** no es un proyecto oficial ni está afiliado a Electronic Arts, 2015 Inc. ni al
  equipo de OpenMoHAA. No contiene archivos del juego.

## Aviso

OpenMOHAA Plus es un proyecto independiente. No está afiliado, autorizado, patrocinado ni respaldado
por Electronic Arts Inc. ni ninguna de sus filiales. "Medal of Honor" es una marca de Electronic Arts
Inc., usada aquí solo para identificar el juego con el que el código es compatible.

Este proyecto no proporciona ni ayuda a obtener datos del juego protegidos por derechos de autor. Las
capturas muestran contenido de Medal of Honor: Allied Assault, propiedad de Electronic Arts Inc.,
solo con fines de identificación.

## Licencia

GPL-2.0, como OpenMoHAA ([COPYING.txt](COPYING.txt)). Las bibliotecas de terceros conservan sus
propias licencias: [Flex](https://github.com/westes/flex),
[Bison](https://savannah.gnu.org/projects/bison/), [SDL](https://www.libsdl.org/),
[OpenAL Soft](https://openal-soft.org/), [libmad](https://www.underbit.com/products/mad/),
[cURL](https://curl.se/), [libogg y libvorbis](https://xiph.org/vorbis/),
[Opus](https://opus-codec.org/),
[Recast Navigation](https://github.com/recastnavigation/recastnavigation), zlib y libjpeg (fuentes
en `code/thirdparty`).

## Créditos

- [OpenMoHAA](https://github.com/openmoh/openmohaa) y sus contribuidores, la reimplementación sobre la que se construye este fork
- [ioquake3](https://github.com/ioquake/ioq3) e id Software, el motor
- El [F.A.K.K. 2 SDK](https://code.idtech.space/ritual/fakk2-sdk) de Ritual Entertainment
- 2015 Inc. y Electronic Arts, Medal of Honor: Allied Assault

## Recursos

[MOH-DB](https://www.moh-db.com/) · [333networks](https://333networks.com/) ·
[ModDB](https://www.moddb.com/games/medal-of-honor-allied-assault) ·
[GameBanana](https://gamebanana.com/games/720) ·
[Documentación de OpenMoHAA](https://openmoh.github.io/openmohaa) ·
[Discord de OpenMoHAA](https://discord.gg/NYtH58R) (comunidad del proyecto original)
