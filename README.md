# cc-fun
various covert channels. work-in-progress.

## Side-channels
- channels/drng     -- cc that uses the Intel HW digital random-number-generator
- channels/pp-l1d   -- same-core L1d covert-channel

## misc
-- misc/lib/l1d     -- taking from mastik (https://cs.adelaide.edu.au/~yval/Mastik/). Used for same-core L1D CC.
-- misc/bench_drng  -- used to microbenchmark intel drng implementation and determine limits

## pending
-- misc/profile     -- used to profile a skylake-server machine and determine thresholds for all ccs (drng,l1d)
-- channels/pp-l1d  -- still adding finishing touches
