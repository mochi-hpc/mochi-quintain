# mochi-treadmill

mochi-treadmill is a microservice (i.e., a Mochi provider) that responds to
parameterized RPCs to generate synthetic concurrent server load.

The provider portion of mochi-treadmill can be started with bedrock.

The primary client is an MPI program that generates a workload for the
provider and measures it's performance.

