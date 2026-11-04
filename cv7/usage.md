# ZOO chat

```sh
make
./zoo_server -d 12345
```

V dalších terminálech spusťte klienty a zadejte přezdívky:

```sh
./zoo_client 127.0.0.1 12345
```

Příkazy klienta:

```text
MSG Ahoj všem v ZOO!
WHERE
MOVE NORTH
MOVE SOUTH
MOVE WEST
MOVE EAST
INFO lion
QUIT
```

- `MSG <text>` vypíše `nick: text` na serveru a u ostatních klientů.
- Text z konzole serveru dostanou všichni klienti jako `Server: text`.
- Vlastní mapa používá `POS x y AREA ...`; polohy ostatních mají formát
  `POS nick x y`. Nový klient dostane i aktuální polohy přihlášených návštěvníků.
- Každé spojení obsluhuje samostatné odpojené POSIX vlákno ve stejném procesu.
  Vlákno přidá/odebere klienta ze sdíleného seznamu a zavře svůj soket.
- Seznam i zápisy do soketů jsou podle zadání zatím bez synchronizace.
- Neaktivní klient se odpojí po 10 sekundách. EOF serverové konzole server neukončí.
- Mapa je sestavená z lokálního zdroje; externí `libzoomap.so` není potřeba.

Integrační kontrola (Python 3, Linux):

```sh
make test
```

V Nix prostředí bez Pythonu: `nix shell nixpkgs#python3 --command make test`.
