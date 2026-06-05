# Libdivide Benchmark for Image Processing

Frame averaging logic: [iterative-div.c](iterative-div.c)

[showcase.webm](https://github.com/user-attachments/assets/0dfa1aa8-3d36-4d63-8638-f77cf413e27c)

<video controls src="showcase.webm" title="showcase.webm"></video>

## Benchmark (default)

```
make benchmark
```

### x86

https://github.com/dxdxdt/gists/actions/runs/26941520109/job/79640018031

```
./iterative-div -t5    < /dev/urandom > /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = processor
[         1.004980826]        231
[         1.003870566]        202
[         1.003282894]        202
[         1.003177936]        202
Alarm clock
142
./iterative-div -t5 -c < /dev/urandom > /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = magic
[         1.000159229]        290
[         1.002484904]        271
[         1.000352507]        270
[         1.003687507]        271
Alarm clock
142
./iterative-div -t5    < /dev/zero >    /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = processor
[         1.000779437]        388
[         1.001995052]        340
[         1.001924238]        340
[         1.001895649]        340
Alarm clock
142
./iterative-div -t5 -c < /dev/zero >    /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = magic
[         1.000113375]        638
[         1.001051881]        595
[         1.001669533]        595
[         1.001288613]        595
Alarm clock
142
```

For the first two cases, the process is bound by `/dev/urandom`. So `/dev/zero`
has been thrown in to eliminate the bottleneck. Linux kernel keeps a special
page filled with zeros and maps `calloc()` or `/dev/zero` to it. This is called
demand paging.

Github Runner VMs seem to be always contended. Up to 300% improvement has been
observed on bare-metal hardware.

### ARM (Apple M1)

https://github.com/dxdxdt/gists/actions/runs/26941520109/job/79640018237

```
./iterative-div -t5    < /dev/urandom > /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = processor
[         1.000233000]        497
[         1.001210000]        456
[         1.001731000]        429
[         1.000408000]        492
[         1.002005000]        486
/bin/sh: line 1:  2440 Alarm clock: 14         ./iterative-div -t5 < /dev/urandom > /dev/null
142
./iterative-div -t5 -c < /dev/urandom > /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = magic
[         1.000698000]        594
[         1.000713000]        611
[         1.000290000]        611
[         1.000040000]        617
[         1.000811000]        621
/bin/sh: line 1:  2561 Alarm clock: 14         ./iterative-div -t5 -c < /dev/urandom > /dev/null
142
./iterative-div -t5    < /dev/zero >    /dev/null || echo $?
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = processor
[         1.000378000]       1133
[         1.000091000]       1081
[         1.000299000]       1058
[         1.000291000]       1025
[         1.000585000]        986
/bin/sh: line 1:  2650 Alarm clock: 14         ./iterative-div -t5 < /dev/zero > /dev/null
142
iterative-div: frame_size = 691200
iterative-div: divisor    = 100
iterative-div: divide     = magic
./iterative-div -t5 -c < /dev/zero >    /dev/null || echo $?
[         1.000236000]       1542
[         1.000473000]       1583
[         1.000212000]       1632
[         1.000540000]       1634
[         1.000113000]       1629
/bin/sh: line 1:  2717 Alarm clock: 14         ./iterative-div -t5 -c < /dev/zero > /dev/null
142
```

Only ~25% improvement. No improvement or worse performance observed with other systems including
Raspberry Pi, Android phones, Ampere VMs(Oracle Cloud) and AWS Graviton VMs. Not sure why.

It's not memory-bound as the results are consistent with server hardware. The best guess is that ARM
processors are not equipped with as many ALUs.

### TODO

 - Use SIMD
 - Use `__builtin_prefetch()` ?

## Run Showcase

```
make showcase
```
