Import("env")
import click

#print(env.Dump())

click.secho("Building source filter...", fg='yellow')

def checkForBuildFlag(flag):
    flags = env.get('BUILD_FLAGS', None)
    if flags is None:
        return False
    for word in flags:
        if flag in word:
            return True
    return False


# Make sure that we override the default source file list...
env.Replace(SRC_FILTER = ["-<*>"])
# env.Append(SRC_FILTER = ["+<src/>"])
# Add event system
env.Append(SRC_FILTER = ["+<src/qf/>"])
# Add includes (qstamp.cpp)
env.Append(SRC_FILTER = ["+<include/>"])

# Check, whether the configuration to build needs unit-test support.
if checkForBuildFlag("Q_UTEST"):
    click.echo("Config is a unit test.")

    # Add QUTEST port to source-list
    env.Append(SRC_FILTER = ["+<ports/arm-cm/qutest/>"])
    click.secho("Added qutest-port to source filter", fg='green')

    # Add Q_UTEST port to includes.
    env.Append(BUILD_FLAGS = ["-I ./ports/arm-cm/qutest/"])

    # Config is unit test. Make sure that Q_SPY is also defined.
    # if not checkForBuildFlag("Q_SPY"):
    #     click.secho("Config is a unit test, but Q_SPY is not defined. Adding it to build flags.", fg='yellow')
    #     env.Append(BUILD_FLAGS = ["-D Q_SPY"])
else:
    click.echo("Config is NOT a unit test.")
    env.Append(SRC_FILTER = "+<ports/arm-cm/qv/gnu/>")
    env.Append(BUILD_FLAGS = ["-I ./ports/arm-cm/qv/gnu/"])
    click.secho("Added qv-kernel to source filter", fg='green')
    # Add qv kernel
    env.Append(SRC_FILTER = "+<src/qv/>") 



if checkForBuildFlag("Q_SPY"):
    click.secho("Adding Q-SPY support by including qs-directory in source filter...")
    env.Append(SRC_FILTER = "+<src/qs/>")

click.secho("source-filter is now: {}".format(env["SRC_FILTER"]), fg='green')
click.secho("Done building source filter", fg='yellow')

# Add additional includes...
click.echo("Adding additional include directories...")
includes = ["./src/", "./include/"]
for word in includes:
    click.echo("Adding: -I {}".format(word))
    env.Append(BUILD_FLAGS = ["-I {}".format(word)])

click.secho("Includes are now: {}".format(env["BUILD_FLAGS"]), fg='green')

# SRC_FILTER

