# Testing ServalRPC
This document helps installing all required software to test ServalrRPC. Everythin is done on Ubuntu 16.04 with a user called `artur`. On other OSs the steps may be different. I also assume everything is installed in you home directory.

## Core
ServalRPC was tested with the [*Common Open Research Emulator*](http://www.nrl.navy.mil/itd/ncs/products/core), short **CORE**.
At the where ServalRPC was written the Ubuntu package was outdated so I built it from source.

First, clone the source from the [CORE Github page](https://github.com/coreemu/core).

The next step is to install all dependencies:

```
~$ sudo apt-get install bash bridge-utils ebtables iproute libev-dev python tcl8.5 tk8.5 libtk-img autoconf automake gcc libev-dev make python-dev libreadline-dev pkg-config imagemagick help2man
```

You also need a custom repo, where the service scripts are and a patch which fixes a problem with broadcast addresses. Clone the following repository and apply the included patch:

```
~$ git clone https://github.com/adur1990/servalrpc-tests-core-scripts.git
~$ patch core/daemon/core/netns/vnode.py servalrpc-tests-core-scripts/broadcast-fix.patch
```

In order to log network traffic later on you need two python packages: `pcap` and `dpkt` which both can be install with

```
~$ sudo apt-get install python-pypcap
~$ sudo apt-get install python-dpkt
```

Then go to the CORE folder and run the folowing commands to prepare, configure, build and install CORE.

```
~/core$ ./bootstrap.sh
~/core$ ./configure
~/core$ make
~/core$ sudo make install
```

Now you have to edit some files in the `core-scripts` folder to meet your local paths. You need a folder where later on all configs for Serval and ServalRPC go to. For simplicity name it `serval-conf` and make it in you home directory:

```
~$ mkdir serval-conf
```

Now you can replace all occurences of `meshadmin` in `~/core-scripts/myservices/servalrpc.py` with your username. All other folders you see are not required per-se, but created ad-hoc by CORE temporarily.

That's it. CORE is successfully installed.

## Serval
Now we have to install Serval. Again, first clone it from Github. For stability reasons you will need to checkout a stable release. Also we need a patch which fixes a problem with generating SIDs. All this was done by some students from the Philipps-University of Marburg and has been commited to their forked Serval repo in the `asserts` branch. So just clone this repo and all requirements are met.

```
~$ git clone https://github.com/umr-ds/serval-dna.git
~$ cd servald-dna
serval-dna$ git checkout asserts
```

No build Serval. Make sure to provide the `--prefix` in `configure` which have to be the `serval-conf` folder created earlier. It has to be an absolute path!

```
serval-dna$ autoreconf -i -f -I m4
serval-dna$ ./configure --prefix=/home/artur/serval-conf
serval-dna$ make
```

Now the Serval binary `servald` has to be in your `$PATH`. The easiest way is to symlink it to `/usr/local/bin` or similar: `sudo ln -s /home/artur/serval-dna/servald /usr/local/bin/`.

## ServalRPC
Now ServalRPC itself has to be installed.

The only dependency for ServalRPC is `libcurl` which can be installed with `sudo apt-get install libcurl3-gnutls-dev`.

Again, clone it from Github. Since this is part of the evaluation, checkout the `eval` branch. Everything will work with the master branch, too. But then no ServalRPC related logs where produced.

```
~$ git clone https://github.com/adur1990/ServalRPC.git
~$ cd ServalRPC
ServalRPC$ git checkout eval
```

In order to build ServalRPC we need first the static Serval library. After that run we can build ServalRPC. Again, ServalRPC has to be in your path under `rpc`. Just symlink it again. Make sure you run `make-lib.sh` and `configure` with the same prefix as serval-dna

```
ServalRPC$ ./make-lib.sh /home/artur/serval-conf
ServalRPC$ ./configure --prefix=/home/artur/serval-conf
ServalRPC$ make
ServalRPC$ sudo ln -s /home/artur/ServalRPC/servalrpc /usr/local/bin/rpc
```

## Tests and Suite
The two last steps are some test scripts which are executed on the nodes and the suite which contains all required scripts to run the tests.

This scripts are also required to monitor overall performance of Serval and ServalRPC. This is done mostly with standard GNU tools which are preinstalled on most Linux systems. One exception is `pidstat` which is part of the `sysstat` package and has to be installed seperately with `sudo apt-get install sysstat`.

Clone the last two repositories from Github:

```
~$ git clone https://github.com/adur1990/servalrpc-tests-helpers.git
~$ git clone https://github.com/adur1990/servalrpc-tests-suite.git
```

Here are also some changes required. First, the `auto-scenario` script copies finished tests out of `/tmp`. This has to be changed to a location of your choise in line 103.
Another change has to be made is in the `run_core_topology.py` script int the line `myservices_path` has to be changed to the `core-scripts/myservices` path downloaded earlier.

The test sripts have to be in `/serval-tests`. It is recommended to symlink the cloned folder to that location.

## Running
Now everything should be set up. To run tests just define them in config files and run the `auto-scenario` script to fire up some tests.

Example configs can be generated with `mk_autorun_confs.sh`. For more information see the READMEs in the respective folders.
