# mochi-quintain

mochi-quintain is a microservice (i.e., a Mochi provider) that responds to
parameterized RPCs to generate synthetic concurrent server load.

## Provider
The provider portion of mochi-quintain can be started with bedrock. bedrock
will need to be able to find the libquintain-bedrock.so library, so either
install quintain via spack or set up your `LD_LIBRARY_PATH` apropriately.

    configure --prefix=${QUINTAIN_DIR}
    make install
    # spack does this for us...
    export LD_LIBRARY_PATH=${QUINTAIN_DIR}/lib:$LD_LIBRARY_PATH}
    bedrock -c tests/mochi-quintain-provider.json tcp
    ./src/quintain-benchmark -g quintain.ssg -j ../tests/quintain-benchmark-example.json -o test-output
    bedrock-shutdown -s quintain.ssg tcp://


The test script 'basic.sh' does all this for you.


### Bedrock and JX9

Bedrock allows us to dynamically configure a service.  For example, tests/mochi-quintain-provider.jx9  can be used to deploy a server with five RPC-handling threads

    bedrock --jx9 -c tests/mochi-quintain-provider.jx9 --jx9-context "num_rpc_xstreams=5" <protocol>

Or none at all:

    bedrock --jx9 -c tests/mochi-quintain-provider.jx9 <protocol>

## Client

The primary client is an MPI program that generates a workload for the
provider and measures it's performance.

## Example execution by hand

Note that this example assumes that you are running from within the build
tree and thus need to set an explicit library path for bedrock to be able to
find the provider libraries for quintain.

In one terminal (for a server):
```
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/home/carns/working/src/mochi/mochi-quintain/build/src/.libs"

bedrock --jx9 -c ~/working/src/mochi/mochi-quintain/tests/mochi-quintain-provider.jx9 --jx9-context "num_rpc_xstreams=5" na+sm://
```

In another terminal (for a client):
```
src/quintain-benchmark -g quintain.flock.json -j ../tests/quintain-benchmark-example.json  -o foo
```
