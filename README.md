# DESCRIPTION
This repository contains the source code for a simple stand-alone zero-config Linux network monitor.
This program has _very_ minimal RAM/CPU footprint and might come in handy if you are using [i3blocks](https://github.com/vivien/i3blocks).

# COMPILATION/INSTALLATION
Run `make` in your command line to compile.

Note: A C11-capable compiler is required.

The compiled binary can be copied to the `/usr/local/bin/` directory using the following command.

```
sudo make install
```

The installed binary can be removed from the `/usr/local/bin/` directory using the following command.

```
sudo make uninstall
```

# USAGE

## Command line
The program's help message can be viewed using the `netmon -h` command.

To start monitoring network connection x, run:

```
netmon -c x
```

## i3blocks
Add the following block to the [i3blocks](https://github.com/vivien/i3blocks) config file (substitute x for your connection name):

```
[netmon]
command=netmon -c x
interval=persist
```

# LICENSE
Copyright Nezametdinov E. Ildus 2021.

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
