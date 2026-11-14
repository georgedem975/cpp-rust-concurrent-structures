# cpp-rust-concurrent-structures
Срвынение неблокирубщих конкурентных структур данных в C++ и Rust на основе бенчмарков

## Запуск бенчмарков

Для запуска нужны Python 3.11 или новее, Bazel (версия из `.bazelversion`),
Rust и MSVC Build Tools с поддержкой C++.

Запускайте команду из корня репозитория в PowerShell:

```powershell
python -m scripts.run --suite all
```

Команда собирает и запускает все бенчмарки для C++ и Rust из наборов
`primitives` и `libraries`. Результаты сохраняются в
`results/<идентификатор_запуска>/`, отдельно для каждого набора.

Можно запустить только один набор:

```powershell
python -m scripts.run --suite primitives
python -m scripts.run --suite libraries
```

По умолчанию бенчмарки запускаются с 2, 4, 8, 16 и 32 потоками, длительностью
1 секунда и 10 повторами. Параметры можно переопределить, например:

```powershell
python -m scripts.run --suite all --threads 2 4 8 --duration 2 --repeats 5
```

## Построение графиков

Скрипт построения графиков требует `matplotlib`, `numpy` и `pandas`:

```powershell
pip install "matplotlib>=3.8" "numpy>=1.26" "pandas>=2.1"
```

Укажите в `--results` каталог запуска, созданный скриптом бенчмарков:

```powershell
python -m scripts.plot.benchmark --results results/<идентификатор_запуска> --suite all
```

Основные параметры: `--suite` (по умолчанию `all`), `--out` (каталог для
графиков, по умолчанию `<results>/plots`), `--formats` (например `png,svg`),
`--dpi`, `--physical`, `--logical`, `--bar-threads`.

## Сервисы

В пакете `cpp/services` находятся TCP-сервисы обработки сделок и генератор
нагрузки. Сборка и запуск выполняются через Bazel.

| Цель | Порт | Описание |
| --- | --- | --- |
| `//cpp/services/blocking:blocking_service` | 9001 | Сервис сделок на блокирующей очереди |
| `//cpp/services/lockfree:lockfree_service` | 9002 | Сервис сделок на неблокирующей очереди |
| `//cpp/services/bombardier:bombardier` | — | Генератор нагрузки (клиент) |

Запуск сервиса:

```powershell
bazel run //cpp/services/blocking:blocking_service
bazel run //cpp/services/lockfree:lockfree_service
```

Запуск генератора нагрузки против одного или обоих сервисов. Аргументы
передаются после `--`, чтобы Bazel пробросил их в исполняемый файл:

```powershell
bazel run //cpp/services/bombardier:bombardier -- --target 127.0.0.1:9001 --target 127.0.0.1:9002 --duration 10
```

`--target` принимает адрес вида `host:port` и может повторяться;
`--duration` задаёт длительность в секундах (`0` — без ограничения).
