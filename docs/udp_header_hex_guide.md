# UDP 首部 16 进制报文对照说明

配套图：`udp_header_hex_guide.svg`

UDP 首部固定 8 字节，没有 TCP 的序列号、确认号、标志位、窗口大小和 Options。看 16 进制报文时，从 UDP 首部起点开始，每 8 个 hex 字符是一行 32 bit，也就是 4 字节。

## UDP 首部字段

| UDP 偏移 | 长度 | 字段 | 16 进制切片 | 说明 |
| --- | ---: | --- | --- | --- |
| `0x00` | 2 字节 | 源端口 Source Port | byte 0-1 | 发送方端口 |
| `0x02` | 2 字节 | 目的端口 Destination Port | byte 2-3 | 接收方端口 |
| `0x04` | 2 字节 | 长度 Length | byte 4-5 | UDP 首部 + UDP 数据的总长度 |
| `0x06` | 2 字节 | 校验和 Checksum | byte 6-7 | IPv4 可为 `00 00`，IPv6 必须使用 |
| `0x08` | 可变 | Payload | byte 8 起 | 应用层数据 |

## 手工拆包例子

假设 UDP 报文开头是：

```text
C0 23 00 35 00 20 1A 2B ...
```

按字段拆：

| 字段 | hex | 结果 |
| --- | --- | --- |
| 源端口 | `C0 23` | 49187 |
| 目的端口 | `00 35` | 53，常见为 DNS |
| UDP Length | `00 20` | 32 字节 |
| Checksum | `1A 2B` | `0x1A2B` |
| Payload | 从 byte 8 开始 | 32 - 8 = 24 字节 |

## 对照口诀

```text
0-1 源端口，2-3 目的端口
4-5 UDP 总长度，6-7 校验和
8 起是应用层数据
```

## 和 TCP 对比

| 对比项 | UDP | TCP |
| --- | --- | --- |
| 首部长度 | 固定 8 字节 | 最短 20 字节，可带 Options |
| 连接 | 无连接 | 面向连接 |
| 可靠性 | 不保证重传、不保证顺序 | 有确认、重传、排序 |
| 常见字段 | 端口、长度、校验和 | 端口、序列号、确认号、Flags、窗口、校验和 |
| 常见协议 | DNS、DHCP、NTP、QUIC | HTTP/HTTPS、SSH、FTP、SMTP |

## 注意

UDP 的 Length 是 UDP 层自己的长度，不是 IP Total Length。它包含 8 字节 UDP 首部：

```text
UDP Payload 长度 = UDP Length - 8
```
