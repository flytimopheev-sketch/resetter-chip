Name:           ressetter-chip
Version:        1.0.0
Release:        1%{?dist}
Summary:        Сброс счётчиков чипов картриджей (CH341A, I2C)

License:        MIT
URL:            https://github.com/flytimopheev-sketch/ressetter-chip
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel >= 6.2
BuildRequires:  libusb1-devel
BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  pkgconf-pkg-config

Requires:       qt6-qtbase
Requires:       libusb1

%description
Графическое приложение на C++/Qt 6 для работы с чипами картриджей лазерных
принтеров и МФУ через программатор CH341A (режим I2C).
Позволяет читать дампы EEPROM 24Cxx, обнулять счётчик страниц,
записывать и верифицировать данные, сохранять резервные копии.

%prep
%setup -q

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
%cmake_build

%check
ctest --test-dir redhat-linux-build --output-on-failure

%install
%cmake_install
mkdir -p %{buildroot}%{_licensedir}/%{name}
install -pm 0644 LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE

%files
%{_bindir}/ressetter-chip
%{_datadir}/applications/ressetter-chip.desktop
%{_datadir}/icons/hicolor/scalable/apps/ressetter-chip.svg
%{_datadir}/ressetter-chip/profiles
%doc %{_datadir}/doc/ressetter-chip/README.md
%doc %{_datadir}/doc/ressetter-chip/CHANGELOG.md
%license %{_licensedir}/%{name}/LICENSE
/usr/lib/udev/rules.d/99-ch341a.rules

%changelog
* Sun Nov 09 2026 Ressetter Chip contributors <flytimopheev-sketch@users.noreply.github.com> - 1.0.0-1
- Первый публичный выпуск: чтение, сброс счётчика, запись и верификация EEPROM 24Cxx


