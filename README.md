Cyglauncher
===========

Cyglauncher allows multiple Cygwin applications to be executed from a single
parent process - initiated by Windows commands. It consists of two parts:
**cyglaunch** is a small Windows client that requests that a Cygwin
command be executed, and **cyglauncher** is a Cygwin server that
spawns the requested commands. cyglaunch will automatically start cyglauncher
if it isn't already running.

cyglaunch commands can be placed in Start Menu shortcuts instead of running
the Cygwin X-windows application directly from the shortcut.
This means that the Cygwin environment (`.profile` etc) is only created once.
This approach has several advantages.

  - It is slightly faster.
  - It allows sharing of configuration information (eg. ssh-agent socket).
  - There is only one console shared between all X-windows clients -
instead of one per client, or none (if `run` is used).
  - The Cygwin setup is done in the cyglauncher shortcut or batch file, so
does not need to be duplicated in each application's shortcut
(and if changed, only needs to be changed once).

The cyglaunch<tt>-&gt;</tt>cyglauncher communication uses [Windows DDE](http://msdn.microsoft.com/library/en-us/winui/winui/windowsuserinterface/dataexchange/dynamicdataexchangemanagementlibrary.asp"),
so this should allow Cygwin applications to started from the Windows Explorer,
based on file-type associations. However this currently has to be set up
manually - and I have not been able to get it to automatically start
cyglauncher using this method (in principle, that should be possible).

## Installation

Currently this is pre-release software, so I haven't really made any proper
documentation or nice setup. If you are interested in trying this, please
let me know and I'll try to make something a little better.

Download [binaries](https://hepunx.rl.ac.uk/~adye/software/cygwin/cyglauncher.zip) or [source](https://hepunx.rl.ac.uk/~adye/software/cygwin/cyglauncher-src.zip).

Unzip the binaries into a common directory. Modify the `cyglauncher-start`
shortcut (or replace it with a `cyglauncher-start.bat` batch file)
to specify the command to start `cyglauncher` with your desired setup.
An example is in `cyglauncher-start-cmd` (the existing `cyglauncher-start`
shortcut is for my own setup, which starts
the [Exceed X-server](http://connectivity.hummingbird.com/products/nc/exceed/)).

To start an application, run

```
cyglaunch *applicationName*
```

from a shortcut, DOS box, or bash prompt.
*`applicationName`* can contain a Cygwin path (or else is located using the Cygwin `$PATH`).
The command `cyglaunch-cygwin` is identical to `cyglaunch`,
except that it is a Cygwin application itself.
That might make it a bit faster to run when already in Cygwin (eg. bash prompt), though
I didn't notice any difference.

The cyglauncher application can run in a DOS box, rxvt, xterm, or whatever
(depends on how you start it in `cyglaunch-start`).
It can be stopped with **`^C`** or `cyglaunch -e`.
Its children continue to run after it dies (except, for some reason, when its running
in a DOS box).
