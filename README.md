# MyOs

Минимальная 64-битная ОС: мультизагрузочный загрузчик через GRUB, `boot.asm` переводит CPU в long mode, ядро инициализирует VGA-терминал, таблицу IDT, таймер PIT (100 Гц), клавиатуру, простой менеджер памяти и минимальный shell.

Во время работы:
- Shell (`myos>`) читает ввод с клавиатуры, поддерживает команды `help`, `clear`, `uptime`, `mem`, `echo`.
- PIT считает тики для вывода аптайма.
- Исключения CPU выводят диагностическое сообщение и останавливают систему.

## Требования

- `x86_64-elf-gcc`, `x86_64-elf-ld`
- `nasm`
- `grub-mkrescue`
- `qemu-system-x86_64`

## Сборка ISO

```bash
cd /home/kavo/MyOs
make
```

Образ появится в `build/MyOs.iso`.

## Запуск в QEMU

```bash
cd /home/kavo/MyOs
make run
```

## Очистка

```bash
cd /home/kavo/MyOs
make clean
```

