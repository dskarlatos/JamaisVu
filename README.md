# Jamais Vu

This repository contains a gem5 reference implementation of **Jamais Vu: Thwarting Microarchitectural Replay Attacks**. 

Please check our paper for details:

```
@inproceedings{Skarlatos:JamaisVu:ASPLOS21,
author = {Skarlatos, Dimitrios and Zhao, Zirui Neil and Paccagnella, Riccardo and Fletcher, Christopher W. and Torrellas, Josep},
title = {Jamais Vu: Thwarting Microarchitectural Replay Attacks},
year = {2021},
publisher = {Association for Computing Machinery},
booktitle = {Proceedings of the Twenty-Sixth International Conference on Architectural Support for Programming Languages and Operating Systems},
series = {ASPLOS ’21}
}
```

[You can find the Jamais Vu paper here!](http://www.cs.cmu.edu/~dskarlat/#pubs) (To appear at ASPLOS'21, coming soon)


## Environment
To build and run Jamais Vu, you will need all libraries that are required by
[gem5](https://www.gem5.org/documentation/learning_gem5/part1/building/).
Note that due to a [bug](https://gem5.atlassian.net/browse/GEM5-631) in gem5,
the simulator might crash on some benchmarks on systems that are newer than Ubuntu 16.04.

To run our epoch analyzer and experiment scripts, you will need
`Python >= 3.6` and Python libraries that are described in
`util/epoch/requirements.txt` and `scripts/requirements.txt`.
You also need [radare2](https://github.com/radareorg/radare2) installed to
run epoch analyzer.

We also provide a Dockerfile under `docker/`, which captures all the dependencies.
To build the docker image, execute
```bash
cd docker && docker build -t jamaisvu .
```
Since the building process will compile Jamais Vu, it may take about 5 to
30 minutes to build, depends on your machines specs. So, grab a cup of coffee or
take a nap, it's your choice :)

Once the Docker image is built, execute
```bash
docker run -it jamaisvu
```
to launch a Docker container that comprises `X86_MESI_Two_Level/gem5.fast`
and required Python environment.

For more advanced Docker usage, please refer to Docker's
[official documents](https://docs.docker.com/engine/reference/commandline/docker/).

## Run Jamais Vu
We provide scripts that run Jamais Vu for all the schemes proposed in our paper.
For more details, please refer to this [README](scripts/README.md).

## Reproducibility
All the results in the paper can be reproduced. For more information about
re-running our experiments, please refer to this section in the [README](scripts/README.md#Reproducibility).

## Epoch Analyzer
The Epoch scheme that we proposed leverages information provided by the compiler.
The implementation of Epoch analyzer is under `util/epoch`. For more information
about the static analyzer, please refer to this [README](util/epoch/README.md).

## Acknowledgement
We used bloom filter implementation from project [ArashPartow/bloom](https://github.com/ArashPartow/bloom)
and counting bloom filter implementation from project [mavam/libbf](https://github.com/mavam/libbf).
Special thanks to contributors of [ArashPartow/bloom](https://github.com/ArashPartow/bloom/graphs/contributors)
and [mavam/libbf](https://github.com/mavam/libbf/graphs/contributors)!
