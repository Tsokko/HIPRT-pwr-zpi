# Podział Zadań w Zespole i Kroki do Wykonania

Ten dokument stanowi bezpośrednią instrukcję pracy dla członków zespołu na najbliższy sprint. Pracujemy na nowo utworzonych gałęziach. Zanim zaczniesz, wykonaj `git fetch --all`.

---

## 👨‍💻 Osoba 1: Testy (Quality Assurance & CI)
**Twój obszar roboczy:** branch `feature/tests`
**Cel:** Zapewnienie, że zrefaktoryzowany system przejdzie kontrolę jakości i działa bezbłędnie z najnowszymi wersjami środowiska APU.

**Kroki do wykonania:**
1. Zmień gałąź: `git checkout feature/tests`
2. Zaktualizuj pliki testowe znajdujące się w folderze `test/` (np. `test_orochi.cpp`, `test_dlopen.cpp`, unit testy w `test/hiprtT*.cpp`). Część z nich może polegać na starszych wersjach API, które zmieniły się przy hybrydzie. Upewnij się, że testy wykorzystują mechanizm "Dual Dispatch" / `ManagedMemory`.
3. Napraw integrację Continuous Integration (CI). Zmodyfikuj skrypty budujące potok (pipeline) tak, aby podczas wykonywania testów na systemach Linux z układem zintegrowanym (APU) ustawiana była zmienna środowiskowa `HSA_XNACK=1`. Bez niej testy na `ManagedMemory` zwrócą błędy dostępu.
4. Uruchom testy statycznej analizy na plikach nagłówkowych w `hiprt/impl` i zredukuj ostrzeżenia (warnings). Po zakończeniu pracy, stwórz Pull Request do gałęzi `adam` / `main`.

---

## 🎮 Osoba 2: Twórca Pokazowego Dema (Game Dev / Rendering)
**Twój obszar roboczy:** branch `feature/demo`
**Cel:** Przekucie potencjału hybrydowego silnika (CPU+GPU) w jedno ustrukturyzowane, zaawansowane demo.

**Kroki do wykonania:**
1. Zmień gałąź: `git checkout feature/demo`
2. Przejdź do folderu `demos/` i zapoznaj się z tym, w jaki sposób w `demo_scenarios.cpp` działa wywołanie `hiprtTraceHybridClosest` oraz `HybridTraceConfig`.
3. Dodaj plik `demo_advanced.cpp` (lub rozbuduj obecne moduły).
4. Napisz pełnoprawną scenę – np. wczytanie skomplikowanego statycznego modelu otoczenia wraz z jakimś dynamicznym elementem. Zaimplementuj śledzenie promieni uwzględniające odbicia (reflections) lub proste cienie (shadow rays) korzystając z silnika.
5. Zmodyfikuj funkcję renderującą tak, aby generowała nie tylko płaski `.ppm`, ale ewentualnie spięła to pod prosty wyświetlacz (np. bibliotekę SDL2/GLFW), jeżeli zajdzie taka potrzeba.

---

## 📊 Osoba 3: Benchmarking i Optymalizacja (Performance Engineer)
**Twój obszar roboczy:** branch `feature/demo` (współpraca z Osobą 2)
**Cel:** Opracowanie rygorystycznych pomiarów wydajności architektury RYZEN AI MAX+ PRO.

**Kroki do wykonania:**
1. Zmień gałąź: `git checkout feature/demo` i ściągnij ewentualne zmiany od Osoby 2.
2. Twoim zadaniem jest dobudowanie do pracy Osoby 2 (Demo zaawansowane) potężnego "Benchmakru Hybrydowego".
3. Skrypt benchmarku powinien automatycznie uruchomić scenę Osoby 2 i przetworzyć ją przy proporcjach: 100% CPU, potem 75% CPU/25% GPU, aż do 100% GPU, mierząc FPS oraz opóźnienie (latency) dystrybucji `ManagedMemory`.
4. Wygeneruj wyniki wydajności (zapisane do pliku `.csv` lub `.json`) prezentujące zalety zunifikowanej pamięci.
5. *Zadanie opcjonalne:* Jeśli masz czas, napisz skrypt w Pythonie (np. w folderze `scripts/`), który zwizualizuje te dane na wykresie. Współpracuj z Osobą 2, by w pełni zintegrować pomiary. Przed wciśnięciem zmian wykonujcie wspólne Code Review.
