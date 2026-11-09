#!/usr/bin/env bash
# Сборка образа, тестов и RPM-пакета Ressetter Chip
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="ressetter-chip-builder"
OUTPUT_DIR="${SCRIPT_DIR}/output"

mkdir -p "${OUTPUT_DIR}"

echo "=== Сборка образа ${IMAGE_NAME} ==="
docker build -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

echo "=== Запуск контейнера: тесты уже пройдены при сборке, копируем RPM ==="
docker run --rm -v "${OUTPUT_DIR}:/output" "${IMAGE_NAME}"

echo "=== Готово. RPM-пакеты: ${OUTPUT_DIR} ==="
ls -l "${OUTPUT_DIR}"
