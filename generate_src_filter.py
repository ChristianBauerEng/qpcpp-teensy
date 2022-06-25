Import("env")
import click
import glob
from pathlib import Path
import os.path
import os,  sys


click.secho("Building source filter...", fg='yellow')

# Find <qpcpp.hpp> file and determine the root path of qp.
searchPath= '../**/qpcpp.hpp'
click.echo("Looking for qpcpp.hpp in: {}".format(searchPath))

for f in glob.iglob(searchPath, recursive=True):
    qpPath = Path(f).parent.parent.as_posix()
    click.echo("qp path found: {}".format(qpPath))


src_path = env.get('PROJECT_SRC_DIR')
click.echo("Current WorkDir: {}".format(os.getcwd()))
click.echo("src_dir: {}".format(src_path))
qpRelPath = os.path.relpath(qpPath, src_path)
click.echo("qp-path relative to src_dir: {}".format(qpRelPath))
qpPath = qpRelPath

# Attention: source filter has to be specified relative to src_dir, not to the path of this file.


def checkForBuildFlag(flag):
    flags = env.get('BUILD_FLAGS', None)
    if flags is None:
        return False
    for word in flags:
        if flag in word:
            return True
    return False


# Make sure that we override the default source file list...
env.Replace(SRC_FILTER = ["-<{}/*>".format(qpPath)])
# env.Append(SRC_FILTER = ["+<src/>"])
# Add event system
env.Append(SRC_FILTER = ["+<{}/src/qf/>".format(qpPath)])
# Add includes (qstamp.cpp)
env.Append(SRC_FILTER = ["+<{}/include/>".format(qpPath)])

# Check, whether the configuration to build needs unit-test support.
if checkForBuildFlag("Q_UTEST"):
    click.echo("Config is a unit test.")

    # Add QUTEST port to source-list
    env.Append(SRC_FILTER = ["+<{}/ports/arm-cm/qutest/>".format(qpPath)])
    click.secho("Added qutest-port to source filter", fg='green')

    # Add Q_UTEST port to includes.
    env.Append(BUILD_FLAGS = ["-I {}/ports/arm-cm/qutest/".format(qpPath)])

    # Config is unit test. Make sure that Q_SPY is also defined.
    # if not checkForBuildFlag("Q_SPY"):
    #     click.secho("Config is a unit test, but Q_SPY is not defined. Adding it to build flags.", fg='yellow')
    #     env.Append(BUILD_FLAGS = ["-D Q_SPY"])

else:
    click.echo("Config is NOT a unit test.")
    env.Append(SRC_FILTER = "+<{}/ports/arm-cm/qv/gnu/>".format(qpPath))
    env.Append(BUILD_FLAGS = ["-I {}/ports/arm-cm/qv/gnu/".format(qpPath)])
    click.secho("Added qv-kernel to source filter", fg='green')
    # Add qv kernel
    env.Append(SRC_FILTER = "+<{}/src/qv/>".format(qpPath)) 



if checkForBuildFlag("Q_SPY"):
    click.secho("Adding Q-SPY support by including qs-directory in source filter...")
    env.Append(SRC_FILTER = "+<{}/src/qs/>".format(qpPath))

click.secho("source-filter is now: {}".format(env["SRC_FILTER"]), fg='green')
click.secho("Done building source filter", fg='yellow')

# Add additional includes...
click.echo("Adding additional include directories...")
includes = ["{}/src/".format(qpPath), "{}/include/".format(qpPath)]
for word in includes:
    click.echo("Adding: -I {}".format(word))
    env.Append(BUILD_FLAGS = ["-I {}".format(word)])

click.secho("Includes are now: {}".format(env["BUILD_FLAGS"]), fg='green')

# SRC_FILTER

#print(env.Dump())