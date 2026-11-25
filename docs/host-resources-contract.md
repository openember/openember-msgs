# Host resources contract

Topic `/host/resources` carries host/v1/HostResourceSnapshot. Existing field numbers remain unchanged. New consumers require online, stale_after_ms and field validity; legacy payloads without these fields are unknown, not zero or healthy.

header.robot_id identifies the product, host_id the configured host (default hostname), boot_id the OS boot, header.source_instance a fresh monitor process instance. Sequence starts at 1 and advances per snapshot. sample_time_monotonic_ns is source-local and must advance within an instance; it is never compared with a remote receiver's clock. Receivers bound payloads, check robot and source, select a host and expire both receive age and sequence progress. Offline snapshots invalidate the stream; publisher restart needs a new source_instance. A previously retired instance must not regain freshness.

uptime_sec is OS uptime, not application uptime. Device time is the target's sampled Unix timestamp; device_local_time includes UTC offset, timezone describes the target timezone. Consumers must not substitute browser time. No clock synchronization claim is made.

CPU ratios are 0..1 measured between two samples, with iowait counted idle. cpu.usage_valid and core_usage_valid gate their values. core_ids, core_usage_ratio and core_usage_valid are parallel arrays with explicit possibly sparse Linux CPU IDs. First samples and topology changes establish a baseline and do not claim 0% use. cpu_window_ms describes the measured interval.

memory.valid gates total/available/used; used = total - available. Missing MemAvailable can use an explicitly marked estimate. Zero available is a valid observation. Swap has separate validity. HostInfo independently identifies OS/kernel/CPU-list observations. Empty isolated_cpu_list with cpu_lists_valid means no isolated CPUs; otherwise unknown.

Disks are optional (disks_sampled); disks_valid denotes observation success and does not imply zero disk use when absent. Current Linux collector restricts optional statvfs collection to local filesystem types to avoid polling NFS. Resource values are observational data, not authorization or a health/safety permit.
