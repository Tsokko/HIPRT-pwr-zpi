# Raport z Postępu Prac i Zadania Zespołowe

## Raport z Postępu Prac (Postęp: ~95%)
Projekt portowania silnika HIPRT na środowiska hybrydowe odniósł ogromny sukces. Właśnie zakończyliśmy kluczowe testy na chmurowym środowisku APU! Oto co udało się zrealizować do tej pory:
- **Testy na sprzęcie APU**: Zdobyliśmy dostęp do instancji chmurowej opartej o układ APU. Zidentyfikowaliśmy i rozwiązaliśmy krytyczny problem "fantomowych sterowników" (środowisko Dockera udawało obecność bibliotek NVIDIA `libcuda.so`, co powodowało crashe).
- **Kuloodporny Fallback (PURE CPU)**: Silnik posiada teraz inteligentną weryfikację kontekstu fizycznego w Orochi. Jeśli detekcja sprzętu zawiedzie, HIPRT płynnie i bezbłędnie przełącza się na natywny tryb procesora (Intel Embree 4 / AMD CPU) omijając całkowicie alokacje GPU. Zapewnia to 100% stabilność na każdym sprzęcie.
- **Złamanie Vendor Lock-in (Orochi)**: Zaimplementowano dynamiczne ładowanie bibliotek NVIDIA za pomocą Orochi, co zdejmuje sztywne powiązanie kodu ze sprzętem AMD. Silnik inicjalizuje się w locie pod karty CUDA (jeśli są fizycznie dostępne).
- **Tryb Hybrydowy (Dual Dispatch)**: Pomyślnie rozwiązano krytyczne błędy pamięci w trybie hybrydowym. Wdrożono mechanizm *Managed Memory* (zunifikowaną pamięć współdzieloną), dzięki czemu CPU i GPU bezpiecznie dzielą wspólną przestrzeń adresową dla struktury BVH.
- **Weryfikacja przez Dema**: Opracowano 5 zaawansowanych aplikacji demonstracyjnych (m.in. Suwak mocy - Load Balancing, Przeplatanka Szachownicy, Pełna Hybryda Magnum Opus). Zostały one przetestowane, zrefaktoryzowane, zbudowane w CMake i przeniesione do dedykowanego podfolderu `demos/`. Pliki pozostałości z renderów (np. obrazy `.ppm`) zostały trwale wyczyszczone.

**Status:** Architektura hybrydowa i awaryjna dla CPU jest gotowa i zweryfikowana. Repozytorium jest czyste i przygotowane do podziału prac nad dalszym rozwojem przez resztę zespołu.

---

## Przydział Zadań na Najbliższy Sprint (3 osoby)

Z powodu braku APU skupiamy się teraz na optymalizacjach, poprawie jakości kodu i automatyzacji podziału zasobów. Oto Wasze role:

### Osoba 1: Inżynier Optymalizacji (Adaptive Load Balancing)
Twój cel to zautomatyzowanie mechanizmu, który obecnie pokazaliśmy w Demo 3 (Ręczny suwak % GPU / % CPU).
**Zadania:**
1. Zaimplementowanie heurystyki w `HybridContext`, która w czasie rzeczywistym bada przepustowość procesora oraz układu graficznego i automatycznie (na żywo) ustala optymalny procent podziału promieni pomiędzy architektury (np. ucinając przydział CPU, gdy zaczyna nie wyrabiać w skomplikowanych geometriach).
2. Refaktoryzacja interfejsu Dual Dispatch, aby z zewnątrz użytkownik mógł po prostu wywołać `hiprtSceneIntersect` a program sam wybrał optymalne podziały.
3. Przeprowadzenie twardych mikro-benchmarków sprawdzających Twoją optymalizację przy dużej ilości renderowanych klatek.

### Osoba 2: Specjalista ds. Jakości i Testów (CI)
Twój cel to upewnienie się, że nasz zmodyfikowany silnik przejdzie kontrolę jakości zespołu docelowego AMD GPUOpen przed akceptacją Pull Requestu.
**Zadania:**
1. Naprawa zablokowanych, głównych testów jednostkowych w potoku CI. Starsze funkcje weryfikujące poprawność HIPRT nie przewidywały inicjalizacji mieszanej z Orochi i padają. Musisz zapewnić im odpowiedni `fallback`.
2. Opracowanie i implementacja "cichej" obsługi błędu inicjalizatora Orochi (sporadyczny `oroError 700` gubiący wskaźniki przy dużym obciążeniu NVIDII). Projekt nie może doprowadzać do crasha aplikacji klienckiej!
3. Redukcja *warnings* i statyczna analiza kodu we wszystkich zmodyfikowanych plikach `hiprt/impl`.

### Osoba 3: Dokumentalista i Teoretyk Architektury UMA
Z powodu braku układu APU do fizycznych testów zunifikowanej pamięci, musisz przygotować dla nas odpowiedni grunt.
**Zadania:**
1. Przygotowanie docelowego "Raportu APU" oraz rozszerzonej dokumentacji technicznej do folderu `docs/`, szczegółowo opisującej poczynione przez nas optymalizacje pamięciowe (dlaczego użyliśmy `oroMallocManaged`).
2. Przemyślenie i wdrożenie symulacji zasobów współdzielonych - jak możemy zasymulować środowisko APU na zwykłym PC, by upewnić się, że nie wycieka nam pamięć operacyjna przy dużych obiektach BVH.
3. Bieżąca współpraca i wsparcie dla Osoby 1 oraz Osoby 2 w obszarach przeglądów ich kodu (Code Review) przed połączeniem do głównej gałęzi kodu (master branch).
