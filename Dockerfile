# Сборка Resetter Chip: зависимости, сборка, тесты, RPM
# Базовый образ содержит Qt 6 и libusb-1.0
FROM fedora:40

LABEL maintainer="Resetter Chip contributors"
LABEL description="Сборка и тестирование Resetter Chip, сборка RPM"

RUN dnf -y install \
        cmake \
        gcc-c++ \
        make \
        which \
        tar \
        gzip \
        pkgconf-pkg-config \
        qt6-qtbase-devel \
        libusb1-devel \
        rpm-build \
        redhat-rpm-config \
    && dnf clean all

WORKDIR /opt/Resetter-chip
COPY . /opt/Resetter-chip/

# 1. Сборка приложения
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
    && cmake --build build -j"$(nproc)"

# 2. Модульные тесты
RUN ctest --test-dir build --output-on-failure

# 3. Подготовка исходников для rpmbuild
RUN mkdir -p /opt/pkg/Resetter-chip-1.0.0 /root/rpmbuild/SOURCES \
    && cp -r src tests packaging CMakeLists.txt README.md CHANGELOG.md LICENSE \
             /opt/pkg/Resetter-chip-1.0.0/ \
    && tar czf /root/rpmbuild/SOURCES/Resetter-chip-1.0.0.tar.gz \
           -C /opt/pkg Resetter-chip-1.0.0

# 4. Сборка RPM (внутри также прогоняются тесты через %check)
RUN rpmbuild -ba packaging/Resetter-chip.spec

VOLUME ["/output"]
CMD ["bash", "-c", "cp -v /root/rpmbuild/RPMS/*/*.rpm /output/ 2>/dev/null || true; ls -l /output"]


