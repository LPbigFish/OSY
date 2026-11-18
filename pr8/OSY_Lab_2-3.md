# OSY Lab 2-3

> **Class lecturer:** BAM015  
> **Deadline:** 11.11.2026, 15:30  
> **Prostředí:** Při startu počítače vybírejte v menu **OSY Lab**.  
> **Sestavení:** Používejte Makefile.

## Zadání

Vaším cílem je naprogramovat simulaci restaurace v zoologické zahradě. Kuchaři připravují jídla a ukládají hotové porce na společný výdejní pult. Návštěvníci zoo si jídla z pultu odebírají.

Příprava a výdej jídel musí být synchronizovány pomocí POSIX semaforů. Použijte pouze tři semafory (`mutex`, `empty`, `full`) a žádné další.

Výdejní pult má omezenou kapacitu a je implementován jako fronta.

## 1. Funkce výroby a spotřeby

Odstraňte z funkcí z domácí přípravy nekonečné smyčky `while`. Funkce zpracuje vždy jednu položku:

```c
void producer(char *jidlo);
void consumer(char *jidlo_out);
```

- `producer()` vloží jedno připravené jídlo do fronty.
- `consumer()` odebere jedno jídlo z fronty.
- Frontu lze implementovat libovolně (pole, kruhový buffer, …).
- Vkládání a odebírání musí být synchronizováno pouze uvedenými třemi semafory.

## 2. Socketový server

- Server inicializuje frontu a semafory.
- Při připojení každého klienta vytvoří nové vlákno.
- Klientovi pošle výzvu:

  ```text
  Role?
  ```

- Klient odpoví jednou z rolí:
  - `kuchar`
  - `navstevnik`

Server podle role obsluhuje klienta:

- **`kuchar`** – přijímá názvy připravených jídel a ukládá je do fronty pomocí `producer()`. Po uložení jídla pošle klientovi potvrzení `OK\n`.
- **`navstevnik`** – odebírá jídla z fronty pomocí `consumer()`, posílá jejich názvy klientovi a čeká na potvrzení `OK\n`.

Každý klient má vlastní obslužné vlákno. Všichni klienti sdílejí jednu společnou frontu.

## 3. Klient

### A) Klient – kuchař

- Po zvolení role `kuchar` vytvoří nové vlákno.
- Ze stdin načte počet jídel, která chce připravit.
- Posílá náhodně vybrané názvy jídel, například:
  - Smazeny syr
  - Kureci rizek s hranolky
  - Spagety Bolognese
  - Zeleninovy salat
  - Hamburger
  - Hot dog
  - Palacinky
  - Ovocny pohar
- Každé jídlo odešle jako samostatný řádek.
- Po každém odeslaném jídle čeká na potvrzení `OK\n`.
- Po odeslání zadaného počtu jídel ukončí svou komunikaci zprávou `QUIT\n`.

### B) Klient – návštěvník

- Po zvolení role `navstevnik` vytvoří nové vlákno.
- Přijímá názvy jídel ze serveru po řádcích.
- Každé přijaté jídlo vypíše na konzoli a odpoví `OK\n`.
- Rychlost odběru není omezena.

## 4. Paralelní běh

- Program musí podporovat současný běh více kuchařů i návštěvníků.
- Každý klient má vlastní vlákno na serveru.
- Přístup ke společné frontě musí být správně synchronizován.
- Jídlo vložené jedním kuchařem může odebrat kterýkoliv návštěvník.

## 5. Synchronizace

Výdejní pult má omezenou kapacitu:

- Kuchaři čekají, pokud je pult plný.
- Návštěvníci čekají, pokud je pult prázdný.
- Celou synchronizaci zajišťují pouze tři semafory:

| Semafor | Význam |
|---|---|
| `mutex` | Výhradní přístup ke společné frontě |
| `empty` | Počet volných míst ve frontě |
| `full` | Počet jídel uložených ve frontě |

## 6. Testovací scénář

1. Spusťte server.
2. Připojte jednoho nebo více kuchařů, kteří začnou připravovat jídla.
3. Připojte jednoho nebo více návštěvníků, kteří budou jídla odebírat a potvrzovat jejich převzetí.
4. Vyzkoušejte také situace, kdy:
   - kuchaři čekají na volné místo v plném výdejním pultu,
   - návštěvníci čekají na jídlo v prázdném výdejním pultu.
5. Po dokončení práce klienta lze připojit další kuchaře i návštěvníky.

Program musí podporovat více současně připojených klientů.

## 7. Ladicí výpisy (doporučené)

Server spusťte s parametrem `-d`:

```bash
./server 12345 -d
```

V ladicím režimu vypisujte při každém vložení a odebrání také:

- číslo klienta a jeho roli,
- pořadové číslo jídla,
- název jídla,
- aktuální obsazení výdejního pultu.

Díky tomu lze sledovat, jak jednotliví kuchaři společný pult plní a jak návštěvníci jídla odebírají.
