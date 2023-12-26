
```
./bin/llvm-profdata show --function=_Z11global_funcv -ic-targets ../testdata/thinlto_indirect_call_promotion_bigendian.profraw 
Counters:
  _Z11global_funcv:
    Hash: 0x07deb612ffffffff
    Counters: 1
    Indirect Call Site Count: 2
    Indirect Target Results:
	[  0, ,          1 ] (100.00%)
	[  1, ,          1 ] (100.00%)
Instrumentation level: IR  entry_first = 0
Functions shown: 1
Total functions: 4
Maximum function count: 1
Maximum internal block count: 0
Statistics for indirect call sites profile:
  Total number of sites: 2
  Total number of sites with values: 2
  Total number of profiled values: 2
  Value sites histogram:
	NumTargets, SiteCount
	1, 2
```
	

```
./bin/llvm-profdata show --function=_Z11global_funcv -ic-targets -swap-func-ptr ../testdata/thinlto_indirect_call_promotion_bigendian.profraw 
Counters:
  _Z11global_funcv:
    Hash: 0x07deb612ffffffff
    Counters: 1
    Indirect Call Site Count: 2
    Indirect Target Results:
	[  0, lib.cc;_ZL7callee0v,          1 ] (100.00%)
	[  1, _Z7callee1v,          1 ] (100.00%)
Instrumentation level: IR  entry_first = 0
Functions shown: 1
Total functions: 4
Maximum function count: 1
Maximum internal block count: 0
Statistics for indirect call sites profile:
  Total number of sites: 2
  Total number of sites with values: 2
  Total number of profiled values: 2
  Value sites histogram:
	NumTargets, SiteCount
	1, 2
```

