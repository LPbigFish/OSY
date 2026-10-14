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

## Pravidlo pred `exec`

1. Zavrit nepouzite konce rour.
2. `dup2()` spravne konce na `stdin` a `stdout`.
3. Zavrit puvodni deskriptory rour.
4. Zavolat `execlp()`.

Jinak externi program muze drzet zapisovaci konec otevreny. Ctenar pak nikdy neuvidi EOF a pipeline se zasekne.

Zdroj: Linux man-pages, odkazy v tabulce.
