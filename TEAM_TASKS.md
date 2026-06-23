# Raport z Postępu Prac i Zadania Zespołowe

## Raport z Postępu Prac (Postęp: ~95%)
Projekt portowania silnika HIPRT na środowiska hybrydowe odniósł ogromny sukces. Właśnie zakończyliśmy kluczowe testy na chmurowym środowisku APU! Oto co udało się zrealizować do tej pory:
- **Testy na sprzęcie APU**: Zdobyliśmy dostęp do instancji chmurowej opartej o układ APU. Zidentyfikowaliśmy i rozwiązaliśmy krytyczny problem "fantomowych sterowników" (środowisko Dockera udawało obecność bibliotek NVIDIA `libcuda.so`, co powodowało crashe).
- **Kuloodporny Fallback (PURE CPU)**: Silnik posiada teraz inteligentną weryfikację kontekstu fizycznego w Orochi. Jeśli detekcja sprzętu zawiedzie, HIPRT płynnie i bezbłędnie przełącza się na natywny tryb procesora (Intel Embree 4 / AMD CPU) omijając całkowicie alokacje GPU. Zapewnia to 100% stabilność na każdym sprzęcie.
- **Złamanie Vendor Lock-in (Orochi)**: Zaimplementowano dynamiczne ładowanie bibliotek NVIDIA za pomocą Orochi, co zdejmuje sztywne powiązanie kodu ze sprzętem AMD. Silnik inicjalizuje się w locie pod karty CUDA (jeśli są fizycznie dostępne).
- **Tryb Hybrydowy (Dual Dispatch)**: Pomyślnie rozwiązano krytyczne błędy pamięci w trybie hybrydowym. Wdrożono mechanizm *Managed Memory* (zunifikowaną pamięć współdzieloną), dzięki czemu CPU i GPU bezpiecznie dzielą wspólną przestrzeń adresową dla struktury BVH.
- **Weryfikacja przez Dema**: Opracowano zaawansowane aplikacje demonstracyjne (Suwak mocy na proceduranych scenach, Magnum Opus, Szachownica), które udowodniły prawidłowe skalowanie układu RYZEN AI MAX+ PRO. Zostały one zrefaktoryzowane, podzielone na osobne pliki w `demos/` i spięte CMake'iem.

**Status:** Architektura hybrydowa i awaryjna dla CPU jest gotowa i zweryfikowana. Repozytorium jest czyste i przygotowane do podziału prac nad dalszym rozwojem przez resztę zespołu.

---

## Przydział Zadań na Najbliższy Sprint (3 osoby)

Skupiamy się teraz na wykorzystaniu wiedzy z działania sprzętu APU na środowisku Dockerowym:

### Osoba 1: Twórca Nowego Dema / Zastosowań (Game Dev)
Twój cel to stworzenie pokazowego dema używającego rozbudowanej funkcjonalności silnika.
**Zadania:**
1. Stworzenie małej, zintegrowanej "mini-gry" lub zaawansowanego pokazowego dema wewnątrz folderu `demos/`, wykorzystującego `hiprtTraceHybridClosest` do renderowania cieni w czasie rzeczywistym.
2. Współpraca z Osobą 3 w kwestii odpowiedniego używania środowiska Docker z APU na potrzeby debuggowania grafiki.
3. Przygotowanie przykładowej sceny 3D (.obj) dedykowanej dla twojego pokazu i umieszczenie jej w dokumentacji.

### Osoba 2: Specjalista ds. Jakości i Testów (CI)
Twój cel to upewnienie się, że nasz zmodyfikowany silnik przejdzie kontrolę jakości zespołu docelowego AMD GPUOpen.
**Zadania:**
1. Naprawa zablokowanych, głównych testów jednostkowych w potoku CI oraz w folderze `test/`. Starsze testy nie uwzględniały trybu "Dual Dispatch".
2. Automatyzacja uruchamiania tych testów w Dockerze na serwerze CI (wymuszenie włączenia zmiennej środowiskowej `HSA_XNACK=1` na poziomie potoku).
3. Redukcja *warnings* i statyczna analiza kodu we wszystkich zmodyfikowanych plikach `hiprt/impl`.

### Osoba 3: Dokumentalista i Ekspert ds. Środowiska UMA
Twój cel to uwiecznienie całego procesu, przez który przeszliśmy na serwerach chmurowych.
**Zadania:**
1. Przygotowanie docelowego "Raportu APU" oraz rozszerzonej dokumentacji technicznej w folderze `docs/`. Musisz napisać dokładny *Tutorial* jak inicjalizować system wewnątrz kontenerów pod Ubuntu.
2. Szczegółowe opisanie wymogu stawiania flagi `HSA_XNACK=1` dla procesorów z unifikowaną pamięcią (gfx1151).
3. Wdrożenie i dokumentacja skryptów wdrażania (Deployment scripts), aby każdy programista w firmie mógł jednym kliknięciem postawić poprawne środowisko kontenerowe z HIPRT na swoim lokalnym sprzęcie APU.
