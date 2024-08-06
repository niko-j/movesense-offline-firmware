#!/bin/bash

docker run -it --rm -v `pwd`:/movesense:delegated movesense/sensor-build-env:2.2
