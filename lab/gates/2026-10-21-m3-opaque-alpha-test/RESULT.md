# Hardware gate 2026-10-22T012430Z

**Result: PASS**

- host: `192.168.18.43`
- seconds to SSH: 173.1
- repo: `3d28fea-dirty`
- submodule checkout: `435791f` (context only - not necessarily what built the payload)
- payload source ref: `435791f`

The authoritative identity of each input is its SHA-256 below. The checkout and
source ref are provenance hints; a hash is the thing that can be re-verified.

## Inputs

| role | file | bytes | sha256 |
|---|---|---|---|
| payload | `vita-baremetal-linux-loader.bin` | 105,928 | `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab` |
| zImage | `zImage` | 32,191,304 | `1f37351ce97f16291c946f9452e80d2222375a43c97889e56b52a1c9b0282ddb` |
| dtb | `pstv.dtb` | 6,432 | `b178404f82a2b554a91158764cea7d7783da6de0e2a8d1b4bb19093fdcb6faa6` |

## Deploy

- previous_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- previous_bytes: `105928`
- parked_as: `payload.bin.prev-f57121842084-14`
- deployed_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- deployed_bytes: `105928`

- launch: 2026-10-22T01:25:35.713436+00:00

## Boot report

```
Linux 6.12.0-gc69dda67b908 armv7l
4
               total        used        free      shared  buff/cache   available
Mem:             480          41         336           4         117         439
/dev/loop7      7.8G  1.5G  5.9G  20% /
lo               UNKNOWN        127.0.0.1/8 ::1/128 
sit0@NONE        DOWN           
enx00d9d1531abf  DOWN           fe80::2d9:d1ff:fe53:1abf/64 
mlan0            UP             192.168.18.43/24 fd24:d93:ab13:6c45:4639:c4ff:fed6:eef6/64 fe80::4639:c4ff:fed6:eef6/64 
 01:28:47 up 2 min,  0 user,  load average: 2.01, 0.99, 0.39
```
