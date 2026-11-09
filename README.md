<div align="center">

<img src="packaging/ressetter-chip.svg" width="104" height="104" alt="Ressetter Chip">

# Ressetter Chip

**Сброс счётчиков чипов картриджей лазерных принтеров и МФУ через программатор CH341A (I2C)**

[![CI](https://github.com/flytimopheev-sketch/ressetter-chip/actions/workflows/ci.yml/badge.svg)](https://github.com/flytimopheev-sketch/ressetter-chip/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/flytimopheev-sketch/ressetter-chip?sort=semver&style=flat-square&color=2f855a)](https://github.com/flytimopheev-sketch/ressetter-chip/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/flytimopheev-sketch/ressetter-chip/total?style=flat-square)](https://github.com/flytimopheev-sketch/ressetter-chip/releases)
[![License](https://img.shields.io/github/license/flytimopheev-sketch/ressetter-chip?style=flat-square&color=2f855a)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6-41cd52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%C2%B7%20RPM-0b7285?style=flat-square)](https://github.com/flytimopheev-sketch/ressetter-chip/releases)

[Возможности](#-возможности) · [Установка](#-установка) · [Использование](#-использование) · [Профили](#-профили-чипов) · [Сборка](#-сборка-из-исходников) · [Документация](docs/) · [Участие](CONTRIBUTING.md)

</div>

![Интерфейс Ressetter Chip](docs/screenshot.png)

<p align="center"><sub>Макет интерфейса, повторяющий компоновку окна приложения (<a href="packaging/app_mockup.html">packaging/app_mockup.html</a>).</sub></p>

---

## 📖 О проекте

**Ressetter Chip** — графическое приложение на C++17 и Qt 6 для обслуживания
чипов EEPROM, которые устанавливаются в картриджи лазерных принтеров и МФУ.
Приложение работает с популярным USB-программатором **CH341A** в режиме I2C:
читает дамп, обнуляет счётчик страниц по профилю картриджа, записывает
исправленный дамп обратно и проверяет результат.

Проект рассчитан на инженеров и сервисные центры, которым нужен предсказуемый
инструмент с журналом операций, резервными копиями и защитой от записи
в служебные области чипа.

```
Дамп EEPROM  →  HEX-редактор  →  сброс счётчика  →  запись + верификация
```

## ✨ Возможности

| | Возможность |
|---|---|
| 📖 | Чтение дампа EEPROM `24C01 … 24C1024` с учётом геометрии и адресации из профиля |
| 🔄 | Обнуление счётчика страниц по смещению, заданному в профиле картриджа |
| ✍️ | Запись дампа постранично с проверкой защищённых зон |
| ✅ | Верификация записанных данных и сравнение с исходным дампом |
| 🧮 | HEX-редактор с обратным разбором правок в байты и подсветкой изменений |
| 💾 | Резервное копирование и восстановление в собственном контейнере `RSCHIPD1` |
| 🔍 | Сканирование шины I2C и автоопределение адреса устройства |
| 📋 | Журнал операций в окне и лог-файл, отмена длительной операции |
| 🧩 | Профили чипов и картриджей в JSON без пересборки приложения |

## 🔌 Поддерживаемое оборудование

| Компонент | Поддержка |
|---|---|
| Программатор | CH341A в режиме I2C (VID `1a86`, PID `5512`; также `5523`, `5535`, `5572`) |
| Шина | I2C, 7-битная адресация, адреса `0x50–0x57` |
| Чипы | EEPROM семейства `24Cxx`: 24C01, 24C02, 24C04, 24C08, 24C16, 24C32, 24C64, 24C128, 24C256, 24C512, 24C1024 |
| ОС | Linux с RPM-пакетами: Fedora, RED OS и совместимые дистрибутивы |
| Зависимости | Qt 6 (Core, Widgets), libusb-1.0 |

> [!IMPORTANT]
> Протокол CH341A реализован по документации и открытым реализациям, но
> **не проверялся на физическом программаторе в среде разработки** — оборудования
> не было. Перед работой с боевыми картриджами проверьте чтение и запись на
> тестовом чипе и всегда сохраняйте резервную копию.
## 🚀 Установка

### Готовый RPM-пакет

Скачайте пакет из [последнего релиза](https://github.com/flytimopheev-sketch/ressetter-chip/releases/latest)
и установите его:

```bash
sudo dnf install ./ressetter-chip-1.0.0-1.*.x86_64.rpm
```

Для доступа к программатору без `root` установите udev-правило (оно входит в пакет):

```bash
sudo cp /usr/lib/udev/rules.d/99-ch341a.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

После этого переподключите программатор и добавьте пользователя в группу `plugdev`
(если она используется в вашем дистрибутиве).

### Из исходников

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel libusb1-devel pkgconf-pkg-config
git clone https://github.com/flytimopheev-sketch/ressetter-chip.git
cd ressetter-chip
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

## 🧭 Использование

1. Подключите программатор CH341A к USB и вставьте чип картриджа в переходник.
2. Выберите профиль чипа (например, `24C16`) и I2C-адрес `0x50`.
3. Нажмите **«Сканировать шину»**, чтобы убедиться, что чип отвечает.
4. **«Читать чип»** — приложение прочитает дамп и покажет его в HEX-редакторе.
5. **«Обнулить счётчик»** — счётчик страниц будет обнулён по смещению из профиля.
6. **«Записать в чип»** — изменения записываются, затем выполняется верификация.
7. **«Резервная копия»** — сохраните оригинал до записи, чтобы иметь путь к откату.

Приложение не перезаписывает блоки, которые не изменились, — это снижает износ
чипа. Изменения в защищённых диапазонах блокируют запись.

## 🧩 Профили чипов

Профили лежат в `src/profiles/*.json` и устанавливаются в
`/usr/share/ressetter-chip/profiles`. Каталог можно переопределить переменной
окружения `RESSETTER_CHIP_PROFILES`.

Формат — массив профилей:

```json
{
  "id": "24C16",
  "name": "EEPROM 24C16 (16 Kbit / 2048 байт)",
  "size_bytes": 2048,
  "i2c_address": 80,
  "two_byte_address": false,
  "block_bits": 3,
  "page_size": 16,
  "write_cycle_ms": 5,
  "family": "24Cxx",
  "counter_offset": 255,
  "counter_size": 2,
  "protected_ranges": [ { "start": 0, "end": 16 } ]
}
```

Поля адресации:

- `two_byte_address` — два байта адреса памяти (24C32 и крупнее);
- `block_bits` — сколько старших бит адреса передаётся в байте устройства
  (24C04 → 1, 24C08 → 2, 24C16 → 3);
- `counter_offset` — смещение счётчика страниц; `4294967295` означает «не задано»;
- `protected_ranges` — диапазоны `[start, end)`, которые нельзя изменять.

Значения записываются в десятичном виде — JSON не поддерживает `0x`-литералы.
Подробнее: [docs/PROFILES.md](docs/PROFILES.md).

## 🏗 Архитектура

```
src/
├── main.cpp            — точка входа приложения
├── gui/                — интерфейс (QtWidgets): панели, HEX-редактор, журнал
├── core/               — логика: профили, дампы, CRC32, счётчик, валидация, воркер
├── adapters/           — CH341A через libusb-1.0 (протокол I2C)
├── profiles/           — JSON-профили чипов и картриджей
└── utils/              — логгер, файловые утилиты, коды ошибок
```

Логика собрана в статическую библиотеку `ressetter_core` и не зависит от GUI —
её можно тестировать отдельно от интерфейса. Подробнее:
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## 🔨 Сборка из исходников

Требования: C++17, CMake ≥ 3.16, Qt 6 (Core, Widgets), libusb-1.0, pkg-config.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

### RPM через Docker

```bash
./build-docker.sh            # образ + тесты + RPM в ./output
```

Либо вручную:

```bash
docker build -t ressetter-chip-builder .
docker run --rm -v "$(pwd)/output:/output" ressetter-chip-builder
```

Внутри образа последовательно выполняются сборка, `ctest` и `rpmbuild -ba`.
Тесты запускаются дважды — при сборке образа и в секции `%check` спецификации.

## 🔐 Безопасность

- перед записью исходное содержимое чипа читается и сравнивается с целевым дампом;
- изменения в защищённых зонах блокируются валидатором;
- неизменённые блоки не перезаписываются — меньше износ EEPROM;
- операцию можно отменить кнопкой «Отменить операцию»;
- найденные уязвимости принимаются по процедуре из [SECURITY.md](SECURITY.md).

## 🗺 Roadmap

- [x] Чтение, запись и верификация EEPROM 24Cxx через CH341A (I2C)
- [x] Обнуление счётчика по профилю картриджа
- [x] Резервные копии, сравнение дампов, журнал операций
- [ ] Проверка на реальном программаторе и расширение набора профилей
- [ ] Поддержка CH341A в режиме SPI (25Qxx) для других типов картриджей
- [ ] Автоматическое определение типа чипа по сигнатуре

## 🤝 Участие в разработке

Баг-репорты, идеи и pull request'ы приветствуются. Перед началом работы
прочитайте [CONTRIBUTING.md](CONTRIBUTING.md) и [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

```bash
git clone https://github.com/flytimopheev-sketch/ressetter-chip.git
cd ressetter-chip
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build -j"$(nproc)" && ctest --test-dir build --output-on-failure
```

## 📄 Лицензия

Проект распространяется по лицензии [MIT](LICENSE).