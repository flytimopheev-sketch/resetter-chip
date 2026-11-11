# Сборка RPM для RED OS 7.3

Пакет `fc40` собран в Fedora 40 с Qt 6.8 и glibc 2.39 и не устанавливается
на RED OS 7.3, где используются Qt 6.6 и glibc 2.28. Для RED OS нужен
отдельный RPM, собранный внутри RED OS 7.3.

Проверенная среда: RED OS 7.3.4 (MUROM), Qt 6.6.3, glibc 2.28, RPM 4.18.

## 1. Зависимости

```bash
sudo dnf install rpm-build redhat-rpm-config qt6-qtbase-devel libusbx-devel \
    cmake gcc-c++ pkgconf-pkg-config tar gzip make
```

В RED OS пакет разработки libusb называется `libusbx-devel`, а в Fedora —
`libusb1-devel`. В spec используется `pkgconfig(libusb-1.0)`, поэтому один
и тот же spec собирается в обоих дистрибутивах.

## 2. Подготовка исходников

Из корня репозитория:

```bash
VERSION="$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
TOPDIR="$HOME/rpmbuild"

mkdir -p "/opt/pkg/resetter-chip-${VERSION}" "${TOPDIR}/SOURCES"
cp -r src tests packaging CMakeLists.txt README.md CHANGELOG.md LICENSE \
      "/opt/pkg/resetter-chip-${VERSION}/"
tar czf "${TOPDIR}/SOURCES/resetter-chip-${VERSION}.tar.gz" \
      -C /opt/pkg "resetter-chip-${VERSION}"
```

## 3. Сборка

```bash
QT_QPA_PLATFORM=offscreen rpmbuild \
    --define "_topdir ${TOPDIR}" \
    --define "dist .redos7" \
    --define "debug_package %{nil}" \
    -ba packaging/resetter-chip.spec
```

`debug_package` отключён: в RED OS 7.3 `find-debuginfo` формирует пустой
список debugsource и сборка пакета debugsource падает. Для установки
пользователю нужен только основной RPM.

Готовый пакет:

```
${TOPDIR}/RPMS/x86_64/Resetter-chip-${VERSION}-1.redos7.x86_64.rpm
```

## 4. Проверка установки

```bash
sudo dnf install ./Resetter-chip-${VERSION}-1.redos7.x86_64.rpm
rpm -q Resetter-chip
ldd /usr/bin/Resetter-chip | grep 'not found'
```

`ldd` не должен показывать отсутствующие библиотеки. Для проверки запуска
без графического сеанса:

```bash
timeout 5 env QT_QPA_PLATFORM=offscreen /usr/bin/Resetter-chip
```

Код возврата `124` означает, что приложение запустилось и было остановлено
по таймауту.
