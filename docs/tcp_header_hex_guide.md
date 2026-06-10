# TCP 首部 16 进制报文对照说明

配套图：`tcp_header_hex_guide.svg`

TCP 使用网络字节序，也就是大端序。看 16 进制报文时，从 TCP 首部起点开始，每 8 个 hex 字符是一行 32 bit，也就是 4 字节。

## 固定 TCP 首部

| TCP 偏移 | 长度 | 字段 | 16 进制切片 | 说明 |
| --- | ---: | --- | --- | --- |
| `0x00` | 2 字节 | 源端口 Source Port | byte 0-1 | 例如 `C0 23` = 49187 |
| `0x02` | 2 字节 | 目的端口 Destination Port | byte 2-3 | 例如 `01 BB` = 443 |
| `0x04` | 4 字节 | 序列号 Sequence Number | byte 4-7 | TCP 字节流序号 |
| `0x08` | 4 字节 | 确认号 Acknowledgment Number | byte 8-11 | `ACK=1` 时有效 |
| `0x0C` | 4 bit | Data Offset | byte 12 高 4 bit | TCP 首部长度，单位 4 字节 |
| `0x0C` | 3 bit | Reserved | byte 12 中间 3 bit | 通常为 0 |
| `0x0C` | 1 bit | NS | byte 12 低 1 bit | ECN 相关，较少手工看 |
| `0x0D` | 8 bit | Flags | byte 13 | `CWR ECE URG ACK PSH RST SYN FIN` |
| `0x0E` | 2 字节 | Window Size | byte 14-15 | 接收窗口大小 |
| `0x10` | 2 字节 | Checksum | byte 16-17 | TCP 校验和 |
| `0x12` | 2 字节 | Urgent Pointer | byte 18-19 | `URG=1` 时有效 |
| `0x14` | 可变 | Options + Padding | byte 20 起 | 长度为 `Data Offset * 4 - 20` |

## 标志位速查

第 13 字节通常就是 TCP Flags，按 bit 从高到低是：

```text
bit:   7   6   5   4   3   2   1   0
flag: CWR ECE URG ACK PSH RST SYN FIN
```

常见值：

| Flags | 含义 |
| --- | --- |
| `0x02` | SYN |
| `0x10` | ACK |
| `0x12` | SYN + ACK |
| `0x18` | PSH + ACK |
| `0x11` | FIN + ACK |
| `0x04` | RST |
| `0x14` | RST + ACK |

## 手工拆包例子

假设 TCP 首部前 20 字节是：

```text
C0 23 01 BB 12 34 56 78 9A BC DE F0 50 18 72 10 1A 2B 00 00
```

按字段拆：

| 字段 | hex | 结果 |
| --- | --- | --- |
| 源端口 | `C0 23` | 49187 |
| 目的端口 | `01 BB` | 443 |
| 序列号 | `12 34 56 78` | `0x12345678` |
| 确认号 | `9A BC DE F0` | `0x9ABCDEF0` |
| Data Offset | `50` 的高 4 bit = `5` | 首部 20 字节 |
| Flags | `18` | PSH + ACK |
| Window Size | `72 10` | 29200 |
| Checksum | `1A 2B` | `0x1A2B` |
| Urgent Pointer | `00 00` | 0 |

## 对照口诀

```text
0-1 源端口，2-3 目的端口
4-7 序列号，8-11 确认号
12 高半字节看首部长度，13 看 Flags
14-15 窗口，16-17 校验和，18-19 紧急指针
20 起是否还有 Options，看 Data Offset 决定
```
