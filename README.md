# fuse_ll boilerplate

Boilerplate FUSE file system using the low-level interface.

# Testing

The file system is regularly tested with a variety of workloads. The following
workloads are tested for each travis-ci.org build:

* Run the Tuxera POSIX test suite (test/posix.sh)
* Build Git and run unit tests (test/git.sh)
* samtools

An additional set of larger workloads (in addition to those listed above) are
run prior to each release:

* Build the Linux kernel (test/kernel.sh)
* Build the Ceph storage system (test/ceph.sh)
* Multiple configurations of iozone (test/iozone.sh)
* Build PostgreSQL and run tests (test/postgres.sh)

[![Build Status](https://travis-ci.org/noahdesu/fuse-boilerplate.svg?branch=master)](https://travis-ci.org/noahdesu/fuse-boilerplate)

| Distribution     | Status |
| ------------     | ------ |
| CentOS 7         | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=centos:7&label=centos:7)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Debian Jessie    | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=debian:jessie&label=debian:jessie)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Ubuntu 14.04 LTS | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=ubuntu:trusty&label=ubuntu:trusty)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Ubuntu 16.04 LTS | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=ubuntu:xenial&label=ubuntu:xenial)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Ubuntu 16.10     | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=ubuntu:yakkety&label=ubuntu:yakkety)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Ubuntu 17.04     | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=ubuntu:zesty&label=ubuntu:zesty)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Fedora 23        | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=fedora:23&label=fedora:23)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Fedora 24        | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=fedora:24&label=fedora:24)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
| Fedora 25        | [![status](https://badges.herokuapp.com/travis/noahdesu/fuse-boilerplate?env=DOCKER_IMAGE=fedora:25&label=fedora:25)](https://travis-ci.org/noahdesu/fuse-boilerplate) |
