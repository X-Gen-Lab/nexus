# syntax=docker/dockerfile:1@sha256:4edf897a3ffa55b89f906fc8cc78afdb3f1834cc9c7083565e611a8a7d5fe99e
# Preparation may use network; firmware execution uses the sealed image offline.
FROM python:3.12-slim@sha256:a6e34c598f2467ed0e9a8d349809fcd8b5c603269512df273a0bb1784edc11b1
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc g++ git make binutils && rm -rf /var/lib/apt/lists/*
COPY dependencies/environment-tools.txt /opt/nexus/environment-tools.txt
RUN --mount=type=secret,id=proxy_ca,required=true \
    PIP_CERT=/run/secrets/proxy_ca python -m pip install --no-cache-dir \
    -r /opt/nexus/environment-tools.txt
ENV PATH="/toolchain/bin:/usr/local/bin:/usr/bin:/bin"
WORKDIR /work
CMD ["python", "--version"]
