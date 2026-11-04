# Prace se systemovymi funkcemi

Tok programu:

```text
data.txt -> Chat 1 -> pipe -> sort -> pipe -> tr -> pipe -> Chat 4
```

| Funkce | Co dela | Bezny zpusob pouziti | V `pr5` |
|---|---|---|---|
| [`fork()`](https://man7.org/linux/man-pages/man2/fork.2.html) | Vytvori kopii procesu. Vrati `0` diteti, PID rodici, `-1` pri chybe. | Spusteni workera nebo samostatneho programu. | Rodic vytvori Chat 1 az Chat 4. |
| [`wait()`](https://man7.org/linux/man-pages/man2/wait.2.html) | Ceka na zmenu stavu potomka. Po ukonceni uvolni jeho zaznam, tedy nevznikne zombie proces. | Rodic ceka na dokonceni vsech workeru; pres `waitpid()` lze cekat na konkretni PID a cist status. | `while (wait(nullptr) > 0)` ceka na vsechny 4 chaty. |
| [`pipe()`](https://man7.org/linux/man-pages/man2/pipe.2.html) | Vytvori jednosesmerny kanal: `pipefd[0]` cte, `pipefd[1]` zapisuje. | `pipe()`, `fork()`, zavrit nepouzite konce, jeden proces zapisuje, druhy cte. | Tri roury spojuji 4 etapy. |
| [`dup2()`](https://man7.org/linux/man-pages/man2/dup.2.html) | Necha `newfd` odkazovat na stejny otevreny objekt jako `oldfd`; puvodni `newfd` atomicky zavre. | Presmerovani souboru/roury na `STDIN_FILENO`, `STDOUT_FILENO` nebo `STDERR_FILENO`. | Chat 2 a 3 dostanou roury na `stdin` a `stdout`; Chat 4 muze dostat `out.txt` na `stdout`. |
| [`execlp()`](https://man7.org/linux/man-pages/man3/exec.3.html) | Nahradi aktualni proces jinym programem. Pri uspechu se nevrati; `p` hleda program v `PATH`. | Po `fork()` nastavit deskriptory a spustit prikaz. Argumenty musi koncit `nullptr`. | Spousti `sort` a `tr a-z A-Z`. |
| [`open()`](https://man7.org/linux/man-pages/man2/open.2.html) | Otevre nebo vytvori soubor a vrati file descriptor. | `O_RDONLY` pro cteni; `O_WRONLY | O_CREAT | O_TRUNC` a prava pri tvorbe vystupu. | Chat 1 otevre vstup. Chat 4 vytvori volitelny vystup s pravy `0644`. |
| [`read()`](https://man7.org/linux/man-pages/man2/read.2.html) | Nacte nanejvys pozadovany pocet bytu. `0` znamena EOF, `-1` chybu; muze vratit i mene bytu. | Cist ve smycce az do `0`; zpracovat skutecny navratovy pocet. | Chat 1 cte `data.txt`; Chat 4 cte vysledek z posledni roury. |
| [`write()`](https://man7.org/linux/man-pages/man2/write.2.html) | Zapise nanejvys pozadovany pocet bytu. Muze zapsat mene bytu nebo vratit `-1`. | Pro obecna data opakovat zapis, dokud nejsou odeslany vsechny byty. | Chat 1 posila bloky do prvni roury a kontroluje pocet zapsanych bytu. |
| [`close()`](https://man7.org/linux/man-pages/man2/close.2.html) | Zavre descriptor; posledni zavreny zapisovaci konec roury zpusobi ctenari EOF. | Zavrit kazdy otevreny descriptor, hlavne nepouzite konce rour po `fork()`. | Kazdy chat zavre vsechny konce rour, ktere nepouziva. Tim dalsi etapa korektne skonci. |

## Ukazky kodu

Zkracene vypisky z `src/main.cpp`.

### `fork()`

```cpp
children[1] = fork();
if (children[1] == 0) {
    sort_messages(first, second, third); // dite: spusti funkci sve etapy
}
if (children[1] < 0) {
    perror("fork"); // -1: dite nevzniklo
    return 1;
}
// rodic pokracuje dalsim fork()
```

### `wait()`

```cpp
// vraci PID ukonceneho ditete, -1 kdyz uz zadne dite nebezi
while (wait(nullptr) > 0) {
}
```

### `pipe()`

```cpp
int first[2], second[2], third[2];
if (pipe(first) < 0 || pipe(second) < 0 || pipe(third) < 0) {
    perror("pipe");
    return 1;
}
// pipefd[0] = cteni, pipefd[1] = zapis
```

### `dup2()`

```cpp
// Chat 2 (sort): predchozi roura na stdin, dalsi roura na stdout
if (dup2(first[0], STDIN_FILENO) < 0 || dup2(second[1], STDOUT_FILENO) < 0) {
    perror("dup2");
    _exit(1);
}
close(first[0]); // puvodni deskriptory uz nejsou potreba
close(second[1]);
```

### `execlp()`

```cpp
execlp("sort", "sort", nullptr);           // argv[0] = nazev programu
execlp("tr", "tr", "a-z", "A-Z", nullptr); // seznam argumentu konci nullptr
perror("exec"); // sem se dostaneme jen kdyz exec selhal
_exit(1);
```

### `open()`

```cpp
int input = open(path, O_RDONLY); // Chat 1: vstupni soubor

// Chat 4: volitelny vystup, 0644 = prava souboru pri vytvoreni
int output = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

### `read()`

```cpp
char buffer[256];
ssize_t count;
while ((count = read(input, buffer, sizeof(buffer))) > 0) {
    // count = skutecne precteny pocet bytu
}
if (count < 0) {
    perror("read");
}
// count == 0 znamena EOF
```

### `write()`

```cpp
if (write(first[1], buffer, count) != count) {
    perror("write"); // zapsano mene bytu nez jsme chteli
    _exit(1);
}
```

### `close()`

```cpp
// kazdy proces zavre konce rour, ktere nepouziva
close(first[0]);
close_pipe(second);
// zavreni posledniho zapisovaciho konce roury => ctenar dostane EOF
```

## Pravidlo pred `exec`

1. Zavrit nepouzite konce rour.
2. `dup2()` spravne konce na `stdin` a `stdout`.
3. Zavrit puvodni deskriptory rour.
4. Zavolat `execlp()`.

Jinak externi program muze drzet zapisovaci konec otevreny. Ctenar pak nikdy neuvidi EOF a pipeline se zasekne.

```cpp
// Chat 2 (sort) - spravne poradi kroku
close(first[1]);                 // 1. nepouzite konce rour
close(second[0]);
close_pipe(third);

dup2(first[0], STDIN_FILENO);    // 2. roury na stdin a stdout
dup2(second[1], STDOUT_FILENO);

close(first[0]);                 // 3. puvodni deskriptory
close(second[1]);

execlp("sort", "sort", nullptr); // 4. exec - pri uspechu se nevrati
```

Zdroj: Linux man-pages, odkazy v tabulce.
