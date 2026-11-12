# syntax=docker/dockerfile:1.7
# tibia desktop image. Libraries come from cmake/TibiaDeps.cmake. Qt is the
# pinned SDK from tools/qt/install-qt.sh.

FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV TZ=Etc/UTC
ENV QT_ROOT=/opt/qt

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
	--mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
	apt-get update && apt-get install -y --no-install-recommends \
	build-essential \
	ca-certificates \
	cmake \
	git \
	libgl1-mesa-dev \
	libx11-dev \
	libx11-xcb-dev \
	libxkbcommon-dev \
	libxkbcommon-x11-dev \
	ninja-build \
	pkg-config \
	python3 \
	python3-venv \
	&& ln -snf "/usr/share/zoneinfo/${TZ}" /etc/localtime \
	&& echo "${TZ}" > /etc/timezone \
	&& apt-get clean \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /srv
COPY CMakeLists.txt CMakePresets.json /srv/
COPY cmake /srv/cmake
COPY src /srv/src
COPY cip /srv/cip
COPY qml /srv/qml
COPY tools/qt /srv/tools/qt

RUN bash tools/qt/install-qt.sh \
	&& prefix="$(bash tools/qt/qt-prefix.sh)" \
	&& cmake --preset linux-release \
		-DTOGGLE_BIN_FOLDER=ON \
		-DOPTIONS_ENABLE_IPO=OFF \
		-DTIBIA_BUILD_TESTS=OFF \
		-DCMAKE_PREFIX_PATH="${prefix}" \
	&& cmake --build --preset linux-release --target tibia

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV TZ=Etc/UTC
ENV QT_ROOT=/opt/qt

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
	--mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
	apt-get update && apt-get install -y --no-install-recommends \
	ca-certificates \
	libgl1 \
	libglib2.0-0 \
	libstdc++6 \
	libx11-6 \
	libx11-xcb1 \
	libxkbcommon0 \
	libxkbcommon-x11-0 \
	tzdata \
	&& ln -snf "/usr/share/zoneinfo/${TZ}" /etc/localtime \
	&& echo "${TZ}" > /etc/timezone \
	&& apt-get clean \
	&& rm -rf /var/lib/apt/lists/* \
	&& groupadd --system tibia \
	&& useradd --system --create-home --gid tibia --home-dir /home/tibia tibia \
	&& install -d -o tibia -g tibia /opt/tibia

COPY --from=build --chown=tibia:tibia /opt/qt /opt/qt
COPY --from=build --chown=tibia:tibia /srv/build/linux-release/bin/ /opt/tibia/
COPY --chown=tibia:tibia data /opt/tibia/data

WORKDIR /opt/tibia
USER tibia
ENV LD_LIBRARY_PATH=/opt/qt/6.10.3/gcc_64/lib
ENV QT_PLUGIN_PATH=/opt/qt/6.10.3/gcc_64/plugins
CMD ["./tibia"]
