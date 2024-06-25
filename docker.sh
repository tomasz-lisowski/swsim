#!/bin/bash
set -o nounset;  # Abort on unbound variable.
set -o pipefail; # Don't hide errors within pipes.
set -o errexit;  # Abort on non-zero exit status.

docker build --progress=plain . -t tomasz-lisowski/swsim:1.0.0 2>&1 | tee docker.log;
docker run -v .:/opt/swsim --tty --interactive --rm tomasz-lisowski/swsim:1.0.0;
