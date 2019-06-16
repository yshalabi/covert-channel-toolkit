# cc-fun
various covert channels. work-in-progress.

to build: 
```sh
cd cc-fun; mkdir build; cd build; cmake ..
make
```
Binaries will be in build/channels

## Side-channels
- channels/drng     -- cc that uses the Intel HW digital random-number-generator
  -- ./cc_drng thread 1 0.0025 0 0.03 0.03
  -- ./cc_drng process 1 0.0025 0 0.03 0.03
  
- channels/pp-l1d   -- same-core L1d covert-channel
  -- use taskset/numactl to pin sender and receiver to same core.
## misc
-- misc/lib/l1d     -- taking from mastik (https://cs.adelaide.edu.au/~yval/Mastik/). Used for same-core L1D CC.
-- misc/bench_drng  -- used to microbenchmark intel drng implementation and determine limits

## pending
-- misc/profile     -- used to profile a skylake-server machine and determine thresholds for all ccs (drng,l1d)
-- channels/pp-l1d  -- still adding finishing touches
