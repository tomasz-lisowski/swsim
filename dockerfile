# docker build --progress=plain . -t tomasz-lisowski/swsim:1.0.0 2>&1 | tee build.log;
# docker run -v ./build:/opt/swsim/build/host --tty --rm tomasz-lisowski/swsim:1.0.0;

FROM ubuntu:22.04 AS base

RUN set -eux; \
    apt-get -qq update; \
    apt-get -qq --yes dist-upgrade;

FROM base AS base__swicc
COPY . /opt/swsim
ENV DEP="cmake gcc gcc-multilib make"
RUN set -eux; \
    apt-get -qq --yes --no-install-recommends install ${DEP}; \
    cd /opt/swsim; \
    make clean; \
    make -j $(nproc) main-static test-static; \
    apt-get -qq --yes purge ${DEP};

FROM base
COPY --from=base__swicc /opt/swsim/build /opt/swsim/build/local
ENTRYPOINT [ "/bin/bash", "-c", "(cp -r /opt/swsim/build/local/*.a /opt/swsim/build/host) && (cp -r /opt/swsim/build/local/*.elf /opt/swsim/build/host)" ]
