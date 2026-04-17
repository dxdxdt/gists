FROM fedora:latest
COPY . /opt/dnf-bad-meta-checksum

RUN dnf -y copr enable rpmsoftwaremanagement/dnf-nightly
RUN dnf -y update
RUN dnf -y install make xpath xxd

WORKDIR /opt/dnf-bad-meta-checksum
RUN make test-ok
RUN make test-faulty-repomd
