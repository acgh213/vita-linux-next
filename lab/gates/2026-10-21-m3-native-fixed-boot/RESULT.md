# Hardware gate 2026-10-22T004804Z

**Result: PASS**

- host: `192.168.18.43`
- seconds to SSH: 398.3
- repo: `0fae9c7-dirty`
- submodule checkout: `435791f` (context only - not necessarily what built the payload)
- payload source ref: `435791f`

The authoritative identity of each input is its SHA-256 below. The checkout and
source ref are provenance hints; a hash is the thing that can be re-verified.

## Inputs

| role | file | bytes | sha256 |
|---|---|---|---|
| payload | `vita-baremetal-linux-loader.bin` | 105,928 | `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab` |
| zImage | `zImage` | 32,192,232 | `5de9c77e90ff62ddf52a0cf22f45e68aee21967afb6dbbb728a1b0deb5b9977e` |
| dtb | `pstv.dtb` | 6,388 | `89b1dd57e9f7fd5c6ff71deb86d7ca3d67a7e9394ff08ddccaec164df1593ebe` |

## Deploy

- previous_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- previous_bytes: `105928`
- parked_as: `payload.bin.prev-f57121842084-11`
- deployed_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- deployed_bytes: `105928`

- launch: 2026-10-22T00:49:08.132533+00:00

## Boot report

```
Linux 6.12.0-g2334ad948b1f armv7l
4
               total        used        free      shared  buff/cache   available
Mem:             480          36         341           1         113         444
/dev/loop7      7.8G  1.5G  5.9G  20% /
lo               UNKNOWN        127.0.0.1/8 ::1/128 
sit0@NONE        DOWN           
enx00d9d1531abf  DOWN           fe80::2d9:d1ff:fe53:1abf/64 
mlan0            UP             192.168.18.43/24 fe80::4639:c4ff:fed6:eef6/64 
 00:55:54 up 6 min,  0 user,  load average: 0.15, 0.80, 0.48
```
