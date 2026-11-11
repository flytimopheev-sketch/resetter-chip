Name:           Resetter-chip
Version:        1.1.0
Release:        1%{?dist}
Summary:        Сброс счётчиков чипов картриджей (CH341A, I2C, самодельные программаторы)

License:        MIT
URL:            https://github.com/flytimopheev-sketch/resetter-chip
Source0:        resetter-chip-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel >= 6.2
# libusb1-devel в Fedora, libusbx-devel в RED OS; оба предоставляют pkgconfig.
BuildRequires:  pkgconfig(libusb-1.0)
BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  pkgconf-pkg-config

Requires:       qt6-qtbase

%description
Графическое приложение на C++/Qt 6 для работы с чипами картриджей лазерных
принтеров и МФУ. Поддерживает программатор CH341A (режим I2C),
CH341A-совместимые платы с произвольным VID/PID и самодельный
последовательный программатор на Arduino/ESP.
Позволяет читать дампы EEPROM 24Cxx, обнулять счётчик страниц,
записывать и автоматически верифицировать данные, сохранять резервные копии.

%prep
%setup -q -n resetter-chip-%{version}

%build
cmake -B redhat-linux-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build redhat-linux-build -j"$(nproc)"

%check
ctest --test-dir redhat-linux-build --output-on-failure

%install
DESTDIR=%{buildroot} cmake --install redhat-linux-build
mkdir -p %{buildroot}%{_licensedir}/%{name}
install -pm 0644 LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE

%files
%{_bindir}/Resetter-chip
%{_datadir}/applications/resetter-chip.desktop
%{_datadir}/icons/hicolor/scalable/apps/resetter-chip.svg
%{_datadir}/resetter-chip/profiles
%doc %{_datadir}/doc/resetter-chip/README.md
%doc %{_datadir}/doc/resetter-chip/CHANGELOG.md
%license %{_licensedir}/%{name}/LICENSE
/usr/lib/udev/rules.d/99-ch341a.rules

%changelog
* Tue Nov 10 2026 Resetter Chip contributors <flytimopheev-sketch@users.noreply.github.com> - 1.1.0-1
- Поддержка самодельного последовательного программатора, профили картриджей,
  безопасная запись с резервным копированием и верификацией, исправления CH341A.

* Mon Nov 09 2026 Resetter Chip contributors <flytimopheev-sketch@users.noreply.github.com> - 1.0.0-1
- Первый публичный выпуск: чтение, сброс счётчика, запись и верификация EEPROM 24Cxx
