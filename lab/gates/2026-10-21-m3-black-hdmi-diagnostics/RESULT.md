# Hardware gate 2026-10-22T011549Z

**Result: PASS**

- host: `192.168.18.43`
- seconds to SSH: 188.7
- repo: `3d28fea-dirty`
- submodule checkout: `435791f` (context only - not necessarily what built the payload)
- payload source ref: `435791f`

The authoritative identity of each input is its SHA-256 below. The checkout and
source ref are provenance hints; a hash is the thing that can be re-verified.

## Inputs

| role | file | bytes | sha256 |
|---|---|---|---|
| payload | `vita-baremetal-linux-loader.bin` | 105,928 | `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab` |
| zImage | `zImage` | 32,191,416 | `f0934dda4087621acf96c76501f5407a4ae84b491241edcb6bf85e3f27e29349` |
| dtb | `pstv.dtb` | 6,408 | `1ff66ae21c994ed02d022f13967113ea081fa73d86fd9ce0f2cc5e99adce95c3` |

## Deploy

- previous_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- previous_bytes: `105928`
- parked_as: `payload.bin.prev-f57121842084-13`
- deployed_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- deployed_bytes: `105928`

- launch: 2026-10-22T01:16:53.473153+00:00

## Boot report

```
Linux 6.12.0-gcf2caedc3e0e armv7l
4
               total        used        free      shared  buff/cache   available
Mem:             480          43         334           4         117         437
/dev/loop7      7.8G  1.5G  5.9G  20% /
lo               UNKNOWN        127.0.0.1/8 ::1/128 
sit0@NONE        DOWN           
enx00d9d1531abf  DOWN           fe80::2d9:d1ff:fe53:1abf/64 
mlan0            UP             192.168.18.43/24 fd24:d93:ab13:6c45:4639:c4ff:fed6:eef6/64 fe80::4639:c4ff:fed6:eef6/64 
 01:20:10 up 2 min,  0 user,  load average: 1.97, 1.30, 0.53
```
