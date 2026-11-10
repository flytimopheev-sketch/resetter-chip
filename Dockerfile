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

WORKDIR /opt/resetter-chip
COPY . /opt/resetter-chip/

# 1. Сборка приложения
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
    && cmake --build build -j"$(nproc)"

# 2. Модульные тесты
RUN ctest --test-dir build --output-on-failure

# 3. Подготовка исходников для rpmbuild (имя каталога/архива — как в Source0)
RUN VERSION="$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)" \
    && sed -i "s/^Version:.*/Version:        ${VERSION}/" packaging/resetter-chip.spec \
    && mkdir -p "/opt/pkg/resetter-chip-${VERSION}" /root/rpmbuild/SOURCES \
    && cp -r src tests packaging CMakeLists.txt README.md CHANGELOG.md LICENSE \
             "/opt/pkg/resetter-chip-${VERSION}/" \
    && tar czf "/root/rpmbuild/SOURCES/resetter-chip-${VERSION}.tar.gz" \
           -C /opt/pkg "resetter-chip-${VERSION}"

# 4. Сборка RPM (внутри также прогоняются тесты через %check)
RUN rpmbuild -ba packaging/resetter-chip.spec

VOLUME ["/output"]
CMD ["bash", "-c", "cp -v /root/rpmbuild/RPMS/*/*.rpm /output/ 2>/dev/null || true; ls -l /output"]
