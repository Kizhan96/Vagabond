#!/usr/bin/env bash
set -euo pipefail

if [[ $(id -u) -ne 0 ]]; then
  echo "Please run as root or via sudo so dependencies can be installed."
  exit 1
fi

apt-get update
apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  qt6-base-dev \
  qt6-base-dev-tools \
  qt6-tools-dev \
  qt6-tools-dev-tools \
  qt6-multimedia-dev \
  qt6-declarative-dev \
  qt6-l10n-tools \
  libqt6multimediawidgets6 \
  libqt6multimedia6 \
  ffmpeg \
  libavcodec-dev \
  libavformat-dev \
  libavdevice-dev \
  libswscale-dev \
  libswresample-dev \
  libavutil-dev
