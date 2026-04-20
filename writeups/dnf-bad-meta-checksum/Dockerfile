# syntax=docker/dockerfile:1
FROM fedora:latest
ARG MASK_FAIL="false"
COPY . /opt/dnf-bad-meta-checksum

RUN dnf -y copr enable rpmsoftwaremanagement/dnf-nightly
RUN dnf -y update
RUN dnf -y install make xpath xxd

WORKDIR /opt/dnf-bad-meta-checksum
RUN ./set-debuglevel.sh
RUN make clean
RUN make test-ok || ${MASK_FAIL}
RUN make test-failover-404-ok || ${MASK_FAIL}
RUN make test-failover-checksum-ok-no-zchunk || ${MASK_FAIL}
RUN make test-failover-checksum-ok || ${MASK_FAIL}
