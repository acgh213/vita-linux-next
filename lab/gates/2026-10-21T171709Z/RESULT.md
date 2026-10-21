# Hardware gate 2026-10-21T171709Z

**Result: PASS**

- host: `192.168.18.43`
- seconds to SSH: 286.3
- repo: `8f50680-dirty`
- submodule checkout: `96c225f` (context only - not necessarily what built the payload)
- payload source ref: `435791f`

The authoritative identity of each input is its SHA-256 below. The checkout and
source ref are provenance hints; a hash is the thing that can be re-verified.

## Inputs

| role | file | bytes | sha256 |
|---|---|---|---|
| payload | `vita-baremetal-linux-loader.bin` | 105,928 | `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab` |
| zImage | `zImage` | 31,965,752 | `8715816a9076d7dbe4ea33ba73e054c181a74f9fa63f73fdc282468c29ae160d` |
| dtb | `pstv.dtb` | 6,133 | `72ddd25262dcbeb827059eea246fa9ea03cf56020f375665549d147106ee0317` |

## Deploy

- previous_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- previous_bytes: `105928`
- parked_as: `payload.bin.prev-f57121842084-3`
- deployed_sha256: `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`
- deployed_bytes: `105928`

- launch: 2026-10-21T17:17:52.416333+00:00

## Boot report

```
Linux 6.12.0-g37b9348710df armv7l
4
               total        used        free      shared  buff/cache   available
Mem:             480          36         340           1         113         443
/dev/loop7      7.8G  1.5G  5.9G  20% /
lo               UNKNOWN        127.0.0.1/8 ::1/128 
sit0@NONE        DOWN           
enx00d9d1531abf  DOWN           fe80::2d9:d1ff:fe53:1abf/64 
mlan0            UP             192.168.18.43/24 fd24:d93:ab13:6c45:4639:c4ff:fed6:eef6/64 fe80::4639:c4ff:fed6:eef6/64 
 17:22:51 up 4 min,  0 user,  load average: 2.30, 1.60, 0.69
```
