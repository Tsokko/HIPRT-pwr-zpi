# HIPRT Hybryda - Szybki Start (Quickstart)

Niniejszy dokument opisuje w bardzo przystępny sposób jak skompilować i uruchomić zrefaktoryzowane dema silnika HIPRT na środowiskach: Windows (NVIDIA CUDA / CPU) oraz Linux (APU ze wsparciem Unified Memory).

## 🚀 Uruchamianie na Linuksie (Środowisko APU)

Głównym środowiskiem docelowym jest Docker na serwerze z układem RYZEN AI MAX+ PRO 395. 

### Kompilacja:
Aby zbudować dema na Linuksie, użyj komendy Make na root folderze (wcześniej wejdź do środowiska `build` lub użyj Makefile jeśli jest w korzeniu, lub użyj standardowego CMake):

```bash
mkdir build
cd build
cmake ..
cmake --build . --target showcase --config Release -j $(nproc)
```

### Uruchamianie Demówek:
Aby układ zintegrowany prawidłowo alokował pamięć dzieloną, **MUSISZ** wyeksportować flagę środowiskową `HSA_XNACK=1` przed włączeniem pliku wykonywalnego.

```bash
export HSA_XNACK=1
./dist/bin/Release/showcase <NUMER_DEMA> [OPCJONALNIE: /sciezka/do/modelu.obj]
```

**Dostępne dema:**
- `1` - Fallback na PURE CPU, gdy GPU nie jest dostępne.
- `2` - AVX Turbo (porównanie prędkości wektoryzacji CPU z trybem skalarnym).
- `3` - Suwak Mocy (Renderuje 5 klatek, od 0% GPU do 100% GPU).
- `4` - Przeplatanka szachownicy (Złożone wysyłanie asynchroniczne GPU i CPU naprzemiennie).
- `5` - Magnum Opus (Podsumowanie pełnej hybrydy - Dual Dispatch na współdzielonym BVH).
- `6` - Stress Test (Identyczne jak 3, ale na potężnej, proceduralnej chmurze trójkątów do sprawdzania skalowalności).

---

## 🪟 Uruchamianie na Windowsie (Środowisko CUDA / Embree)

Na systemie Windows inicjator biblioteki dynamicznej Orochi podepnie pod silnik API NVIDII (CUDA).

### Kompilacja (Visual Studio / CMake):
Możesz wykorzystać standardową ścieżkę CMake dla Windowsa:
```powershell
mkdir build
cd build
cmake ..
cmake --build . --target showcase --config Release
```

### Uruchamianie:
Na systemie Windows nie trzeba eksportować żadnych flag pamięci zunifikowanej.
```powershell
.\dist\bin\Release\showcase.exe 5
```

---

## Wczytywanie Własnej Geometrii

Silnik domyślnie ładuje pojedynczy, trywialny trójkąt. Aby wczytać zaawansowaną siatkę trójkątów korzystając z biblioteki `tinyobjloader`, po prostu dopisz argument na końcu wywołania dema:

```bash
./dist/bin/Release/showcase 3 ./przykladowy_model.obj
```
