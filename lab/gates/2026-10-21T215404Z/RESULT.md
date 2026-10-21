# Hardware gate 2026-10-21T215404Z

**Result: PASS**

- host: `192.168.18.43`
- seconds to SSH: 267.1
- repo: `93d8bff-dirty`
- submodule checkout: `435791f` (context only - not necessarily what built the payload)
- payload source ref: `435791f`

The authoritative identity of each input is its SHA-256 below. The checkout and
source ref are provenance hints; a hash is the thing that can be re-verified.

## Inputs

| role | file | bytes | sha256 |
|---|---|---|---|
| payload | `vita-baremetal-linux-loader.bin` | 105,928 | `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab` |
| zImage | `zImage` | 32,190,816 | `8e89899c1d48533d492baec642ae0cd2aec047bdb7df552a098bb595660d1857` |
| dtb | `pstv.dtb` | 6,388 | `89b1dd57e9f7fd5c6ff71deb86d7ca3d67a7e9394ff08ddccaec164df1593ebe` |

## Deploy

- previous_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- previous_bytes: `105928`
- parked_as: `payload.bin.prev-f57121842084-4`
- deployed_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- deployed_bytes: `105928`

- launch: 2026-10-21T21:54:45.110240+00:00

## Boot report

```
Linux 6.12.0-g168f0269fe71 armv7l
4
               total        used        free      shared  buff/cache   available
Mem:             480          38         338           1         114         442
/dev/loop7      7.8G  1.5G  5.9G  20% /
lo               UNKNOWN        127.0.0.1/8 ::1/128 
sit0@NONE        DOWN           
enx00d9d1531abf  DOWN           fe80::2d9:d1ff:fe53:1abf/64 
mlan0            UP             192.168.18.43/24 fd24:d93:ab13:6c45:4639:c4ff:fed6:eef6/64 fe80::4639:c4ff:fed6:eef6/64 
 21:59:20 up 4 min,  0 user,  load average: 1.58, 1.38, 0.61
```
