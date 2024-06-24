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
COPY --from=base__swicc /opt/swsim/build /opt/swsim/build
COPY --from=base__swicc /opt/swsim/data /opt/swsim/data

RUN set -eux; \
    rm -r /opt/swsim/build/swsim; \
    rm -r /opt/swsim/build/swicc; \
    rm -r /opt/swsim/build/test;

ENTRYPOINT [ "bash", "-c", "cp -r /opt/swsim/* /opt/out" ]
