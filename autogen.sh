#!/bin/sh

./bootstrap && \
./configure $@ && \
( gmake || make )
