# Change Request: JSON results encoding and console output for `hipercontracer`

- **Branch:** `json-output` in `alojz-gomola/hipercontracer`, commit `c7fd5a6`
- **Base:** upstream `master`, commit `bf77b7a`
- **Scope:** 6 files changed, 254 insertions, 27 deletions

## 1. Summary

`hipercontracer` can now write its Ping and Traceroute results as JSON, either
to results files or to stdout. Two new options select this. When they are not
used, the existing HPCT file output is unchanged and remains the default.

## 2. Motivation

Getting JSON today takes two steps. First, write `.hpct.xz` files:

```
hipercontracer -# 1234 --resultsdirectory test --traceroute --destination 8.8.8.8 \
  --iterations 1 --tracerouteduration 30000
```

Then convert them with a Python script:

```
src/python/traceroute-to-json Traceroute-ICMP-#1234-0.0.0.0-20260917T125353.016593-000000001.hpct.xz
```

With this change, one command produces the JSON:

```
hipercontracer -# 1234 --traceroute --destination 8.8.8.8 --iterations 1 --tracerouteduration 30000 \
  --resultsencoding JSON --resultsoutput CONSOLE > results.json
```

This removes the intermediate files and the conversion step. Results can also be
piped straight into other tools.

## 3. New options

| Option              | Values            | Default | Meaning                   |
|---------------------|-------------------|---------|---------------------------|
| `--resultsencoding` | `HPCT`, `JSON`    | `HPCT`  | Encoding of the results   |
| `--resultsoutput`   | `FILE`, `CONSOLE` | `FILE`  | Where the results go      |

Values are case-insensitive. An invalid value makes the program exit with an
error at startup, for example `ERROR: Invalid results encoding: XML`.

|          | `FILE`                                         | `CONSOLE`                  |
|----------|------------------------------------------------|----------------------------|
| **HPCT** | `.hpct` files in `--resultsdirectory` (as today) | HPCT lines on stdout       |
| **JSON** | `.json` files in `--resultsdirectory`          | One JSON array on stdout   |

- **FILE:**
  - JSON files follow the same rules as HPCT files: naming, directory hierarchy
    (`-z`), rotation (`-x`) and compression (`-C`). Only the extension differs.
    With the default XZ compression a file is named, for example,
    `Traceroute-ICMP-#1234-0.0.0.0-<time>-000000001.json.xz`.
  - Every file, including each rotated one, is a complete JSON array.
- **CONSOLE:**
  - `--resultsdirectory` is not needed, and there is no rotation or compression.
  - Each result is written and flushed as soon as it is available. `[` comes
    with the first result and `]` when `hipercontracer` shuts down normally.
    A run without results prints `[]`.
- **Default (FILE without `--resultsdirectory`):** the existing readable
  summary on stdout is unchanged.

## 4. JSON format

The top level is an array of objects: one per Ping result, and one per
Traceroute run. It is indented by 3 spaces, identical to Python's
`json.dumps(..., indent=3)`.

### Fields common to both services

| Field                 | Type   | Meaning                                   |
|-----------------------|--------|-------------------------------------------|
| `service`             | string | `"Ping"` or `"Traceroute"`                |
| `type`                | string | Protocol, e.g. `"ICMP"`, `"UDP"`          |
| `measurement_id`      | int    | Measurement ID (`-#`)                     |
| `source_address`      | string | Source IP address                         |
| `destination_address` | string | Destination IP address                    |
| `timestamp`           | int    | Nanoseconds since the Unix epoch          |

### Ping

`burst_seq`, `traffic_class`, `packet_size`, `response_size`, `checksum`,
`source_port`, `destination_port`, `status`, `time_source`,
`delay_app_send`, `delay_queuing`, `delay_app_receive`, `rtt_app`, `rtt_sw`, `rtt_hw`

### Traceroute

`round`, `total_hops`, `traffic_class`, `packet_size`, `checksum`,
`source_port`, `destination_port`, `status_flags`, `path_hash`, and `hops`.
`hops` is an array with one object per hop, holding these fields:

`send_timestamp`, `hop_number`, `response_size`, `status`, `time_source`,
`delay_app_send`, `delay_queuing`, `delay_app_receive`, `rtt_app`, `rtt_sw`,
`rtt_hw`, `hop_address`

### Conventions

- All delays and RTTs are in nanoseconds.
- Fields written as hex in HPCT (timestamps, `traffic_class`, `checksum`,
  `time_source`, `path_hash`) are plain decimal integers in JSON.
- Field meanings are the same as in `ping-to-json` and `traceroute-to-json`.
  The names are the snake_case versions of theirs (e.g. `measurementId` →
  `measurement_id`, `rttSw` → `rtt_sw`), and `service` is added.

### Example

One Traceroute run, shortened to one hop. The values are illustrative.

```json
[
   {
      "service": "Traceroute",
      "type": "ICMP",
      "measurement_id": 1234,
      "source_address": "0.0.0.0",
      "destination_address": "8.8.8.8",
      "timestamp": 1789650833016593000,
      "round": 0,
      "total_hops": 11,
      "traffic_class": 0,
      "packet_size": 64,
      "checksum": 48879,
      "source_port": 0,
      "destination_port": 0,
      "status_flags": 256,
      "path_hash": 12297829382473034410,
      "hops": [
         {
            "send_timestamp": 1789650833016593000,
            "hop_number": 1,
            "response_size": 92,
            "status": 1,
            "time_source": 1426063360,
            "delay_app_send": 21000,
            "delay_queuing": 4000,
            "delay_app_receive": 18000,
            "rtt_app": 1203000,
            "rtt_sw": 1160000,
            "rtt_hw": 0,
            "hop_address": "192.168.1.1"
         }
      ]
   }
]
```

## 5. Design decisions

1. **New option instead of extending `-F`/`--resultsformat`.** `-F` already
   selects the HPCT format version as an integer (1 or 2). Changing its type
   would break existing scripts, and encoding and version are independent of
   each other. JSON always uses the version 2 fields and ignores `-F`.
2. **Default behaviour unchanged.** Existing deployments, packaging and the
   importer pipeline work exactly as before.
3. **snake_case field names.** The intended consumers are Python tools, and
   snake_case is the PEP 8 convention. The existing camelCase scripts are left
   unchanged.
4. **`service` field.** On the console, Ping and Traceroute objects share one
   array, and `type` already holds the protocol, so each object needs its own
   marker. File output includes the field too, so objects look the same
   everywhere.
5. **No new dependency.** The JSON text is built with `boost::format`, as the
   HPCT lines already are. Every value is an integer or an IP address string,
   so no escaping is needed. Not using Boost.JSON avoids a new Boost component
   and changes to CMake and the Debian, RPM and FreeBSD packaging.
6. **Array framing in `ResultsWriter`.** Services produce only the object
   text. `ResultsWriter` adds the array brackets and commas, and handles file
   naming, rotation and compression as before.
7. **One Traceroute run is one `insert()`.** The header and hops of a run are
   collected in `Traceroute::JSONEntry` and written in a single call. This way
   objects from different service threads can't interleave on the shared
   console.
8. **One console writer shared by all services.** For `CONSOLE`,
   `makeResultsWriter()` returns the same writer to every service, so stdout
   gets a single valid array instead of several arrays back to back.
   - `insert()` is protected by a mutex, because each service runs in its own
     thread.
   - `prepare()` opens stdout only once. Services are prepared and started one
     after another, so an earlier service may already be writing when a later
     one is prepared.
9. **Logs stay on stderr** (`std::clog`), so stdout holds only results. `-q`
   reduces the log output. In console mode the startup log shows
   `Output = console (JSON)` or `(HPCT)`.

## 6. Files changed

| File                                     | Change                                                                                         |
|------------------------------------------|------------------------------------------------------------------------------------------------|
| `src/resultswriter.h`, `src/resultswriter.cc` | `ResultsEncodingType` (`RET_HPCT`, `RET_JSON`), console mode, JSON array framing, `.json` extension, mutex, shared console writer |
| `src/traceroute.h`, `src/traceroute.cc`  | JSON object per Traceroute run                                                                 |
| `src/ping.cc`                            | JSON object per Ping result                                                                    |
| `src/hipercontracer.cc`                  | New options, validation, passing them to `makeResultsWriter()`, startup log                    |

The new parameters of the `ResultsWriter` constructor and of
`makeResultsWriter()` have default values, so no other caller had to change.

## 7. Testing

**Verified:**
- Builds without warnings on macOS 26.5 (arm64) with Apple clang 15 and
  Boost 1.92. Only the `hipercontracer` tool was enabled in this build.
- `--help` lists both new options.
- Invalid option values are rejected.

**Still open.** These need root, because ICMP uses raw sockets:
- JSON on the console: the output parses, and it equals
  `json.dumps(json.loads(output), indent=3)`.
- JSON to files: the `.json.xz` file decompresses to valid JSON.
- Default HPCT run: the output is unchanged compared with `master`.
- Ping with `--resultsencoding JSON`.
- Builds on Linux and FreeBSD.

Commands for the JSON checks:

```
sudo ./build/src/hipercontracer -# 1234 --traceroute --destination 8.8.8.8 --iterations 3 \
  --tracerouteinterval 1000 --tracerouteintervaldeviation 0 --tracerouteduration 3000 \
  --tracerouteinitialmaxttl 20 --traceroutefinalmaxttl 20 \
  --resultsencoding JSON --resultsoutput CONSOLE > out.json
python3 -c 'import json; t=open("out.json").read(); assert json.dumps(json.loads(t), indent=3)+"\n" == t; print("ok")'

sudo ./build/src/hipercontracer -# 1234 --resultsdirectory test --traceroute --destination 8.8.8.8 \
  --iterations 1 --tracerouteduration 3000 --resultsencoding JSON
xz -dc test/*.json.xz | python3 -m json.tool > /dev/null && echo valid
```

## 8. Limitations and not included

- **Jitter:** no JSON output, because the Jitter service is disabled in
  `hipercontracer` (`#if 0`).
- **Manpage:** `src/hipercontracer.1` does not document the new options yet.
- **HPCT on the console with several services:** with `--ping --traceroute`,
  only one `#? HPCT` header line is written, and it names only one of the
  services.
- **`path_hash` precision:** it is an unsigned 64-bit value. Python reads it
  exactly, but JavaScript JSON parsers lose precision above 2^53.
- **String timestamps:** there is no equivalent of the scripts' `-T` option;
  timestamps are always integers.
- **Abnormal termination:** if the process is killed (e.g. `SIGKILL`), the
  console array is not closed with `]`.
