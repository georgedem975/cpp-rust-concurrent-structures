# Модель памяти

## Сравнительная таблица

Какие гарантии язык даёт относительно доступа к памяти, потоков, гонок данных и порядка операций.

| Аспект                                      | C++                                                                    | Rust                                                                  |
|---------------------------------------------|------------------------------------------------------------------------|-----------------------------------------------------------------------|
| модель памяти                               | Object-based memory model                                              | Access-based memory model                                             |
| Гонки данных (Data Races)                   | Неатомарные конфликтующие обращения без синхронизации приводят к UB    | То же правило; в Safe Rust такие гонки предотвращаются системой типов |
| Безопасный совместный доступ                | std::mutex, std::shared_mutex, std::atomic и другие средства           | Mutex<T>, RwLock<T>, атомики и другие средства                        |
| Отношение happens-before                    | Есть                                                                   | Есть                                                                  |
| Отношение synchronizes-with                 | Есть                                                                   | Есть соответствующая семантика синхронизации                          |
| Порядок операций внутри потока              | Sequenced-before                                                       | Sequenced-before                                                      |
| Атомарность операций                        | std::atomic<T>                                                         | std::sync::atomic                                                     |
| Порядки памяти                              | relaxed, consume (до 26 стандарта), acquire, release, acq_rel, seq_cst | Relaxed, Acquire, Release, AcqRel, SeqCst                             |
| Барьеры памяти                              | atomic_thread_fence, atomic_signal_fence                               | fence, compiler_fence                                                 |
| Управление доступом к памяти                | В основном ответственность программиста                                | Ownership и Borrowing                                                 |
| Совместное владение                         | std::shared_ptr, std::weak_ptr                                         | Arc<T>, Weak<T>                                                       |
| Безопасность передачи между потоками        | Необходимость синхронизации контролируется программистом               | Send                                                                  |
| Безопасность совместного доступа из потоков | Необходимость синхронизации контролируется программистом               | Sync                                                                  |
| Изменяемость при совместном доступе         | Контролируется программистом и средствами синхронизации                | Interior Mutability: Mutex, RwLock, Atomic*, Cell, RefCell            |
| Небезопасный доступ к памяти                | Указатели, ручное управление временем жизни и синхронизацией           | unsafe, raw pointers и ручная синхронизация                           |

## Отношение порядка выполнения

| Отношение                       | C++                         | Rust                        |
|---------------------------------|-----------------------------|-----------------------------|
| Порядок внутри потока           | Sequenced-before            | Sequenced-before            |
| Синхронизация между потоками    | Synchronizes-with           | Synchronizes-with           |
| Транзитивный порядок выполнения | Happens-before              | Happens-before              |
| Межпоточная связь               | Inter-thread happens-before | Inter-thread happens-before |

## Memory Ordering

| Порядок                 | C++                                    | Rust              |
|-------------------------|----------------------------------------|-------------------|
| Relaxed                 | memory_order_relaxed                   | Ordering::Relaxed |
| Acquire                 | memory_order_acquire                   | Ordering::Acquire |
| Release                 | memory_order_release                   | Ordering::Release |
| Acquire-Release         | memory_order_acq_rel                   | Ordering::AcqRel  |
| Sequentially Consistent | memory_order_seq_cst                   | Ordering::SeqCst  |
| Consume                 | memory_order_consume (до 26 стандарта) | Нет               |