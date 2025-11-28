# Руководство для разработчика

## Общая структура проекта

`Cedra` использует CMake в качестве основной системы сборки.
Все основные правила определены в модуле [`cmake/CdrHelpers.cmake`](../cmake/CdrHelpers.cmake).

Он предоставляет унифицированный интерфейс для добавления библиотек, тестов и бенчмарков:
- `cdr_cpp_library()` — добавить библиотеку (в том числе header-only)
- `cdr_cpp_test()` — добавить тест
- `cdr_cpp_executable()` — добавить исполняемый файл или бенчмарк

Эти функции по смыслу аналогичны Bazel-правилам `cc_library`, `cc_test` и `cc_binary` и реализуют те же принципы: минимальные декларации, понятные зависимости, единый стиль именования.


## Префиксы и соглашения

- Все цели внутри проекта имеют префикс `cdr_` (например, `cdr_math`, `cdr_core`).
- Для каждой цели автоматически создаётся алиас `cdr::<имя>` для использования в `target_link_libraries`.
- Исходники библиотеки находятся в `cdr/<имя_модуля>/`.
- В корне модуля обычно создаётся `CMakeLists.txt`, в котором объявляется соответствующая цель через одну из функций ниже.

---

## Добавление новой библиотеки

### 1. Обычная (компилируемая) библиотека

```cmake
cdr_cpp_library(
  NAME
    math
  HDRS
    "math_utils.h"
  SRCS
    "math_utils.cc"
  DEPS
    cdr::core
  COPTS
    "-Wall" "-Wextra"
  PUBLIC
)
```

**Что делает:**
- создаёт цель `cdr_math`
- добавляет алиас `cdr::math`
- подключает include-директории проекта
- линкует с `cdr::core`
- выставляет компиляционные опции
- экспортирует интерфейсы для установки, если включён `CDR_ENABLE_INSTALL`

**Важно:**
Файлы, оканчивающиеся на `.h`, `.hpp`, `.inc` и т. д., автоматически исключаются из списка `SRCS`.

---

### 2. Header-only библиотека

Если библиотека не содержит `.cc` или `.cpp` файлов, то `cdr_cpp_library()` создаёт **INTERFACE**-библиотеку:

```cmake
cdr_cpp_library(
  NAME
    traits
  HDRS
    "traits.h"
  DEPS
    cdr::core
)
```

Такой таргет не компилируется, но может быть подключён через `target_link_libraries()` другими библиотеками и исполняемыми файлами.

---

### 3. Только для тестов или бенчмарков

Иногда полезно создавать библиотеки, которые нужны только в тестах или бенчмарках.
Для этого используются флаги `TESTONLY` и `BENCHONLY`.

```cmake
cdr_cpp_library(
  NAME
    math_test_utils
  HDRS
    "math_test_utils.h"
  SRCS
    "math_test_utils.cc"
  TESTONLY
)
```

Эта библиотека соберётся **только**, если включён `CDR_BUILD_TESTS=ON`.

---

## Добавление тестов

Тесты создаются с помощью функции `cdr_cpp_test()`:

```cmake
cdr_cpp_test(
  NAME
    math_test
  SRCS
    "math_test.cc"
  DEPS
    cdr::math
    GTest::gmock
    GTest::gtest_main
  COPTS
    "-O0" "-g"
)
```

**Особенности:**
- создаёт исполняемый файл `math_test`
- добавляет его в список CTest под тем же именем
- не создаётся, если `CDR_BUILD_TESTS=OFF`

---

## Добавление бенчмарков

Бенчмарки оформляются как исполняемые файлы, но с опцией `BENCH`:

```cmake
cdr_cpp_executable(
  NAME
    math_bench
  SRCS
    "math_bench.cc"
  DEPS
    cdr::math
    benchmark::benchmark
  BENCH
)
```

**Особенности:**
- создаёт исполняемый файл `math_bench`
- компилируется только если `CDR_BUILD_BENCH=ON`

---

## Сборка и запуск

### 1. Конфигурация проекта

```bash
cmake -B build -DCDR_BUILD_TESTS=ON -DCDR_BUILD_BENCH=ON
```

### 2. Сборка

```bash
cmake --build build -j
```

Все исполняемые файлы (тесты, бенчмарки, утилиты) будут собраны в:
```
build/bin/
```

### 3. Запуск тестов

```bash
cd build
ctest --output-on-failure
```

CTests автоматически регистрируются через `add_test()` при вызове `cdr_cpp_test()`.

---

## Пример структуры модуля

```
cdr/
 └── math/
     ├── CMakeLists.txt
     ├── math_utils.h
     ├── math_utils.cc
     ├── math_test.cc
     └── math_bench.cc
```

### Пример `CMakeLists.txt` для такого модуля:

```cmake
cdr_cpp_library(
  NAME
    math
  HDRS
    "math_utils.h"
  SRCS
    "math_utils.cc"
  DEPS
    cdr::core
)

cdr_cpp_test(
  NAME
    math_test
  SRCS
    "math_test.cc"
  DEPS
    cdr::math
    GTest::gtest_main
)

cdr_cpp_executable(
  NAME
    math_bench
  SRCS
    "math_bench.cc"
  DEPS
    cdr::math
    benchmark::benchmark
  BENCH
)
```

---

## Советы и best practices

- Все новые библиотеки следует оформлять через `cdr_cpp_library()` — даже header-only.
- Не добавляйте `.h/.hpp` в `SRCS`. Это делает функция автоматически.
- Используйте алиасы (`cdr::name`) вместо прямых имён целей.
- Для тестов и бенчмарков **всегда** используйте соответствующие опции `TESTONLY` и `BENCH` при необходимости.
- Собирайте проект с `-DCDR_BUILD_TESTS=ON` только локально или в CI, чтобы не тащить тесты в релизные сборки.
- Для всех библиотек рекомендуется указывать `PUBLIC`, если они предназначены для дальнейшей установки.

---

## Быстрая шпаргалка

| Тип цели | Функция | Флаг | Сборка при | Пример |
|-----------|----------|------|-------------|---------|
| Обычная библиотека | `cdr_cpp_library` | — | Всегда | `cdr_math` |
| Header-only | `cdr_cpp_library` | — | Всегда | `cdr_traits` |
| Тестовая библиотека | `cdr_cpp_library` | `TESTONLY` | `CDR_BUILD_TESTS=ON` | `cdr_math_test_utils` |
| Тест | `cdr_cpp_test` | — | `CDR_BUILD_TESTS=ON` | `math_test` |
| Бенчмарк | `cdr_cpp_executable` | `BENCH` | `CDR_BUILD_BENCH=ON` | `math_bench` |

---
