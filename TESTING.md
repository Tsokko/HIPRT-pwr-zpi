# Uruchamianie Testów HIPRT

## Wymagania

### Windows
- Visual Studio 2022
- CMake 3.19+
- CUDA Toolkit 13.0 (lub nowszy)

### Linux / APU (RYZEN AI MAX+ PRO)
- ROCm (`/opt/rocm`)
- CMake 3.19+
- GCC / Clang

---

## Budowanie

### Windows

```powershell
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target unittest --config Release
```

Plik wykonywalny pojawi się w `dist/bin/Release/unittest64.exe`.

### Linux / APU

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DHIP_PATH=/opt/rocm
cmake --build . --target unittest -j$(nproc)
```

Plik wykonywalny pojawi się w `dist/bin/Release/unittest64`.

---

## Uruchamianie testów

### Windows

Przed uruchomieniem ustaw dwie zmienne środowiskowe:

```powershell
$env:HIPRT_PATH = "C:\Users\RODO\Desktop\zpi"          # ścieżka do korzenia repo
$env:PATH = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0\bin\x64;" + $env:PATH
```

Uruchomienie wszystkich testów:

```powershell
.\dist\bin\Release\unittest64.exe --width=512 --height=512 --referencePath=.\test\references\
```

Uruchomienie wybranych grup testów:

```powershell
# Testy podstawowe (jednostkowe):
.\dist\bin\Release\unittest64.exe --width=512 --height=512 --referencePath=.\test\references\ --gtest_filter="hiprtTest*"

# Testy z modelami OBJ:
.\dist\bin\Release\unittest64.exe --width=512 --height=512 --referencePath=.\test\references\ --gtest_filter="Obj*"

# Wszystkie naraz:
.\dist\bin\Release\unittest64.exe --width=512 --height=512 --referencePath=.\test\references\ --gtest_filter="hiprtTest*:Obj*"
```

### Linux / APU

> **WAŻNE:** Na układach APU z zunifikowaną pamięcią (gfx1151) flaga `HSA_XNACK=1` jest **obowiązkowa**. Bez niej testy korzystające z ManagedMemory zwrócą błędy dostępu.

```bash
export HSA_XNACK=1
export HIPRT_PATH=$(pwd)          # uruchom z korzenia repo, nie z build/
./dist/bin/Release/unittest64 --width=512 --height=512 --referencePath=./test/references/ --gtest_filter="hiprtTest*:Obj*"
```

Jedna komenda (build + uruchomienie) jeśli jesteś w katalogu `build/`:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --target unittest -j$(nproc) && cd .. && export HSA_XNACK=1 && export HIPRT_PATH=$(pwd) && ./dist/bin/Release/unittest64 --width=512 --height=512 --referencePath=./test/references/ --gtest_filter="hiprtTest*:Obj*"
```

---

## Grupy testów

| Filtr | Opis | Liczba testów |
|---|---|---|
| `hiprtTest*` | Testy jednostkowe: BVH, ray casting, geometria | 24 |
| `Obj*` | Testy z modelami OBJ: cienie, AO, transformacje | 15 |
| `PerformanceTestCases*` | Testy wydajności (wymagają dużych modeli z LFS) | kilkanaście |

---

## Wyniki testów

Wygenerowane obrazy PNG trafiają do katalogu `test_results/` (lub do bieżącego katalogu roboczego jeśli nie istnieje).

Testy porównują wyjście z obrazami referencyjnymi w `test/references/`. Niewielkie różnice pikselowe (`Pixel difference < 1%`) są akceptowalne i wynikają z różnic między GPU a architekturą referencyjną.

---

## Rozwiązywanie problemów

| Objaw | Przyczyna | Fix |
|---|---|---|
| `No such file or directory` dla `unittest64` | Brak builda | Uruchom `cmake --build` |
| `Unable to open BvhBuilderKernels.h` | Zły `HIPRT_PATH` | Ustaw `HIPRT_PATH` na korzeń repo |
| `SEH exception 0xc0000005` (Windows) | Brak `nvrtc64_130_0.dll` w PATH | Dodaj `CUDA\v13.0\bin\x64` do PATH |
| Błędy dostępu do pamięci (Linux/APU) | Brak `HSA_XNACK=1` | `export HSA_XNACK=1` przed uruchomieniem |
