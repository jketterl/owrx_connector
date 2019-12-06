ARG DIST
FROM debian:$DIST

RUN apt-get update && \
    apt-get install -y cmake build-essential debsigs file librtlsdr-dev libsoapysdr-dev && \
    rm -rf /var/lib/apt/lists/*

ADD . /package
WORKDIR /package

CMD [ "/package/docker/build-package.sh" ]