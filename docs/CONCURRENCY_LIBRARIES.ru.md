# Сравниваемые библиотеки

| Язык | Библиотека     | Назначение                                             |
|------|----------------|--------------------------------------------------------|
| C++  | Boost.Lockfree | Lock-free очереди и стеки                              |
| C++  | libcds         | Конкурентные контейнеры и memory reclamation           |
| Rust | Crossbeam      | Очереди, deque, каналы, epoch-based memory reclamation |
| Rust | scc            | Очереди, стеки, hash- и tree-индексы                   |

# Алгоритмы

Тип: SPSC / MPSC / MPMC. Гарантия: wait-free / lock-free / blocking.

## Boost.Lockfree

| Алгоритм                        | Тип  | Гарантия  | Класс                                      |
|---------------------------------|------|-----------|--------------------------------------------|
| Michael & Scott                 | MPMC | lock-free | `boost::lockfree::queue<T>`                |
| Vyukov bounded MPMC (turnstile) | MPMC | lock-free | `boost::lockfree::bounded_ticket_queue<T>` |
| Vyukov MPSC                     | MPSC | lock-free | `boost::lockfree::mpsc_weak_queue<T>`      |
| Treiber                         | MPMC | lock-free | `boost::lockfree::stack<T>`                |
| Ringbuffer                      | SPSC | wait-free | `boost::lockfree::spsc_queue<T>`           |
| Triple buffer                   | SPSC | wait-free | `boost::lockfree::spsc_value<T>`           |

## libcds

| Алгоритм                                            | Тип  | Гарантия                       | Класс                                     |
|-----------------------------------------------------|------|--------------------------------|-------------------------------------------|
| Treiber                                             | MPMC | lock-free                      | `cds::container::TreiberStack<T>`         |
| Treiber + elimination (Hendler, Shavit, Yerushalmi) | MPMC | lock-free                      | `cds::container::EliminationStack<T>`     |
| Michael & Scott                                     | MPMC | lock-free                      | `cds::container::MSQueue<T>`              |
| Baskets Queue (Hoffman, Shalev, Shavit)             | MPMC | lock-free                      | `cds::container::BasketQueue<T>`          |
| Moir (Doherty и др.)                                | MPMC | lock-free                      | `cds::container::MoirQueue<T>`            |
| Optimistic (Ladan-Mozes, Shavit)                    | MPMC | lock-free                      | `cds::container::OptimisticQueue<T>`      |
| Segmented (Afek, Korland, Yanovsky)                 | MPMC | lock-free, квази-линеаризуемая | `cds::container::SegmentedQueue<T>`       |
| Vyukov bounded MPMC                                 | MPMC | blocking                       | `cds::container::VyukovMPMCCycleQueue<T>` |
| Michael & Scott blocking                            | MPMC | blocking                       | `cds::container::RWQueue<T>`              |
| Michael                                             | MPMC | lock-free                      | `cds::container::MichaelList<T>`          |
| Michael                                             | MPMC | lock-free                      | `cds::container::MichaelKVList<K,T>`      |
| Michael hash table                                  | MPMC | lock-free                      | `cds::container::MichaelMap<K,T>`         |
| Michael hash table                                  | MPMC | lock-free                      | `cds::container::MichaelSet<T>`           |
| Split-ordered list (Shalev, Shavit)                 | MPMC | lock-free                      | `cds::container::SplitListMap<K,T>`       |
| Split-ordered list (Shalev, Shavit)                 | MPMC | lock-free                      | `cds::container::SplitListSet<T>`         |
| Skip list (Herlihy, Shavit)                         | MPMC | lock-free                      | `cds::container::SkipListMap<K,T>`        |
| Skip list (Herlihy, Shavit)                         | MPMC | lock-free                      | `cds::container::SkipListSet<T>`          |
| Multi-level array hash (Feldman, LaBorde, Dechev)   | MPMC | wait-free                      | `cds::container::FeldmanHashMap<K,T>`     |
| Multi-level array hash (Feldman, LaBorde, Dechev)   | MPMC | wait-free                      | `cds::container::FeldmanHashSet<T>`       |
| Non-blocking BST (Ellen и др.)                      | MPMC | lock-free                      | `cds::container::EllenBinTreeMap<K,T>`    |
| Non-blocking BST (Ellen и др.)                      | MPMC | lock-free                      | `cds::container::EllenBinTreeSet<T>`      |
| Lazy list (Heller и др.)                            | MPMC | blocking                       | `cds::container::LazyList<T>`             |
| Lazy list (Heller и др.)                            | MPMC | blocking                       | `cds::container::LazyKVList<K,T>`         |
| Cuckoo hashing (Herlihy, Shavit)                    | MPMC | blocking                       | `cds::container::CuckooMap<K,T>`          |
| Cuckoo hashing (Herlihy, Shavit)                    | MPMC | blocking                       | `cds::container::CuckooSet<T>`            |
| Striped locks                                       | MPMC | blocking                       | `cds::container::StripedMap<K,T>`         |
| Striped locks                                       | MPMC | blocking                       | `cds::container::StripedSet<T>`           |
| AVL (Bronson и др.)                                 | MPMC | blocking                       | `cds::container::BronsonAVLTreeMap<K,T>`  |
| Heap (Hunt, Michael, Parthasarathy, Scott)          | MPMC | blocking                       | `cds::container::MSPriorityQueue<T>`      |
| Flat combining (Hendler и др.)                      | MPMC | blocking                       | `cds::container::FCQueue<T>`              |
| Flat combining (Hendler и др.)                      | MPMC | blocking                       | `cds::container::FCStack<T>`              |
| Flat combining (Hendler и др.)                      | MPMC | blocking                       | `cds::container::FCDeque<T>`              |
| Flat combining (Hendler и др.)                      | MPMC | blocking                       | `cds::container::FCPriorityQueue<T>`      |

## Crossbeam

| Алгоритм                                             | Тип                         | Гарантия                                       | Класс                              |
|------------------------------------------------------|-----------------------------|------------------------------------------------|------------------------------------|
| Chase & Lev work-stealing deque (владелец)           | MPMC (1 владелец + N воров) | не потокобезопасен: только из потока-владельца | `crossbeam_deque::Worker<T>`       |
| Chase & Lev work-stealing deque (вор)                | MPMC (1 владелец + N воров) | lock-free                                      | `crossbeam_deque::Stealer<T>`      |
| Vyukov bounded MPMC                                  | MPMC                        | lock-free                                      | `crossbeam_queue::ArrayQueue<T>`   |
| Сегментированная очередь (блоки со слотами и тегами) | MPMC                        | lock-free                                      | `crossbeam_queue::SegQueue<T>`     |
| Lock-free skip list                                  | MPMC, упорядоченный         | lock-free                                      | `crossbeam_skiplist::SkipMap<K,V>` |
| Lock-free skip list                                  | MPMC, упорядоченный         | lock-free                                      | `crossbeam_skiplist::SkipSet<T>`   |
| Каналы на парковке потоков (отправка)                | MPMC                        | blocking                                       | `crossbeam_channel::Sender<T>`     |
| Каналы на парковке потоков (приём)                   | MPMC                        | blocking                                       | `crossbeam_channel::Receiver<T>`   |

## scc

| Алгоритм                                           | Тип                 | Гарантия                                                     | Класс                 |
|----------------------------------------------------|---------------------|--------------------------------------------------------------|-----------------------|
| Очередь из `sdd`                                   | MPMC                | lock-free                                                    | `scc::Queue<T>`       |
| Стек из `sdd`                                      | MPMC                | lock-free                                                    | `scc::Stack<T>`       |
| Bag из `sdd`                                       | MPMC                | lock-free                                                    | `scc::Bag<T>`         |
| Linked list из `sdd`                               | MPMC                | lock-free                                                    | `scc::LinkedList`     |
| Lock-free стек массивов корзин + rwlock на корзину | MPMC                | resize lock-free, запись blocking                            | `scc::HashMap<K,V>`   |
| То же, значение `()`                               | MPMC                | resize lock-free, запись blocking                            | `scc::HashSet<T>`     |
| Read-optimized hash index                          | MPMC                | чтение lock-free, запись lock-free без структурных изменений | `scc::HashIndex<K,V>` |
| 32-полосный кэш поверх `HashMap`                   | MPMC                | resize lock-free, запись blocking                            | `scc::HashCache<K,V>` |
| B+ дерево                                          | MPMC, упорядоченный | чтение lock-free, запись blocking при split/merge            | `scc::TreeIndex<K,V>` |

# Примечания

1. Wait-free есть только у Boost (`spsc_queue`, `spsc_value`) и libcds (`FeldmanHash*`).
2. Crossbeam не заявляет lock-free для `ArrayQueue` и `SegQueue` — гарантия следует из алгоритма и реализации.
3. У scc lock-free только resize и чтение; запись в `HashMap`/`HashSet`/`HashCache` берёт rwlock на корзину.
4. У `scc::Queue`/`Stack`/`Bag`/`LinkedList` алгоритм в документации не назван - они приходят из крейта `sdd`.
5. `bounded_ticket_queue` в модуле `boost.lockfree` 1.90 нет (в поставке только `queue`, `stack`, `spsc_queue`, `spsc_value`), поэтому в замерах ограниченная очередь собрана из `boost::lockfree::queue<T, capacity<1024>>`: она MPMC и lock-free, но на алгоритме Michael & Scott, а не Vyukov.
6. `SegmentedQueue` в libcds даёт квази-линеаризуемость, а не строгую линеаризуемость.


# Сопоставление С++ и Rust

## Совпадают алгоритм, тип и гарантия

| Алгоритм                    | Тип                 | Гарантия  | C++                                        | Rust                               |
|-----------------------------|---------------------|-----------|--------------------------------------------|------------------------------------|
| Vyukov bounded MPMC         | MPMC                | lock-free | `boost::lockfree::bounded_ticket_queue<T>` | `crossbeam_queue::ArrayQueue<T>`   |
| Skip list (Herlihy, Shavit) | MPMC, упорядоченный | lock-free | `cds::container::SkipListMap<K,T>`         | `crossbeam_skiplist::SkipMap<K,V>` |
| Skip list (Herlihy, Shavit) | MPMC, упорядоченный | lock-free | `cds::container::SkipListSet<T>`           | `crossbeam_skiplist::SkipSet<T>`   |

## Совпадают тип и гарантия, алгоритм в Rust не назван

| Алгоритм C++    | Тип  | Гарантия  | C++                                                            | Rust                                         |
|-----------------|------|-----------|----------------------------------------------------------------|----------------------------------------------|
| Michael & Scott | MPMC | lock-free | `boost::lockfree::queue<T>`, `cds::container::MSQueue<T>`      | `scc::Queue<T>` (в `sdd` алгоритм не указан) |
| Treiber         | MPMC | lock-free | `boost::lockfree::stack<T>`, `cds::container::TreiberStack<T>` | `scc::Stack<T>` (в `sdd` алгоритм не указан) |

# Тестовые сценарии

Замеряются только пары из сопоставления выше. Одна схема для всех: сетка потоков
1, 2, 4, 8, 16, 32; 1 с полезной работы на точку; 10 повторов; пачка 100 операций;
прогрев 1 с. Метрики — из [METRICS.ru.md](./METRICS.ru.md).

## Сценарии

| Сценарий                | C++                         | Rust            | Одна операция                             | Нагрузка                                   |
|-------------------------|-----------------------------|-----------------|-------------------------------------------|--------------------------------------------|
| Bounded MPMC, смешанная | `bounded_ticket_queue<T>`   | `ArrayQueue<T>` | `push` или `pop`, 50/50                   | ёмкость 1024, все потоки и пишут, и читают |
| Очередь MS              | `boost::lockfree::queue<T>` | `scc::Queue<T>` | `push` или `pop`, 50/50                   | без ограничения ёмкости                    |
| Очередь MS              | `MSQueue<T>`                | `scc::Queue<T>` | `push` или `pop`, 50/50                   | без ограничения ёмкости                    |
| Стек                    | `stack<T>`                  | `scc::Stack<T>` | `push` или `pop`, 50/50                   | без ограничения ёмкости                    |
| Стек                    | `TreiberStack<T>`           | `scc::Stack<T>` | `push` или `pop`, 50/50                   | без ограничения ёмкости                    |
| Skip list, смешанная    | `SkipListMap<K,T>`          | `SkipMap<K,V>`  | `insert` / `get` / `erase`, 20/70/10      | ключи случайные из 10⁶                     |
| Skip set, смешанная     | `SkipListSet<T>`            | `SkipSet<T>`    | `insert` / `contains` / `erase`, 20/70/10 | ключи случайные из 10⁶                     |

## Что замеряем в каждой точке сетки

| Метрика                       | Как считаем                                             | Раздел в [METRICS](./METRICS.ru.md) |
|-------------------------------|---------------------------------------------------------|-------------------------------------|
| Пропускная способность        | $N / T$, ops/s                                          | Throughput                          |
| Средняя задержка (Average)    | $\sum t_i / m$ по всем пачкам прогона                   | Latency                             |
| Задержка p50, p95, p99, p99.9 | $t_{\text{пачки}} / k$ по гистограмме пачек             | Latency                             |
| Ускорение                     | $T(1) / T(N)$                                           | Scalability                         |
| Эффективность                 | Ускорение / $N$                                         | Scalability                         |
| Использованная память         | `sizeof` структуры, байт на элемент, пик RSS            | Memory usage                        |
| Загрузка процессора           | $(\text{user} + \text{sys}) / \text{wall} \times 100\%$ | CPU utilization                     |
