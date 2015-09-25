#!/bin/sh

./bootstrap && \
./configure --enable-colorgcc $@ && \
( gmake || make )
