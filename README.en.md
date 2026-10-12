# PacketCaptureTool

[简体中文](README.md) | [English](README.en.md)

A network packet capture and custom protocol analysis tool built with Qt 6 / QML and C++17. It supports live capture, offline PCAP/PCAPNG reading, JSON field definitions, TCP stream reassembly, and rotating capture archives for device communication debugging and binary protocol validation.

Updated: 2026-10-11. This documentation describes the current source tree.

## Preview

![Main window and field-to-byte navigation](docs/screenshots/manual-main-en.png)

The screenshot uses synthetic packets from automated UI tests. Actual fields, counters, and language depend on your session. The UI supports instant Simplified Chinese / English switching, light / dark themes, and restored interface, column, and analysis display preferences.

## Features

| Feature | Current support |
| --- | --- |
| Live capture | Interface selection, start/stop, session continuation, driver receive/drop statistics |
| Offline analysis | Background PCAP/PCAPNG reading, packet-count progress, cancellation; failures or capacity overflow preserve the previous session |
| Custom protocols | One TCP or UDP JSON configuration, source/destination port matching, fixed or field-based message length |
| Field parsing | Signed/unsigned 8–64-bit integers, float32/float64, UTF-8 strings, byte arrays, big/little endian |
| Extended fields | Unsigned integer bit fields, integer enums, scale, bias, and units |
| Protocol editor | Form / JSON / Hex preview, advanced attributes, JSON saving, draft application and reparsing |
| TCP streams | Bidirectional reassembly of the current memory snapshot, segmented/coalesced messages, retransmissions, out-of-order packets, fixed/length-field framing, and gap diagnostics |
| Analysis views | Table/cards, search, status filters, AND/OR conditions, sorting, custom field columns, and field context-menu filters |
| Byte navigation | Field/Hex navigation in both directions, payload-only display, adjustable field/Hex heights, and copying plain Hex bytes |
| Storage | Save all packets in the current memory window to PCAP; automatic archives rotate by size, time, or link type |
| Desktop integration | App icon, system tray, instant bilingual UI, About dialog, and movable settings directory |

## Quick start

1. Run `PacketCaptureTool.exe` from its complete deployment directory.
2. Select **语言 → English** if you want the English UI.
3. Open **Protocol Editor → Import JSON**, load [the extended fields example](docs/extended_fields_example.json), choose **Hex preview**, enter `3F C0 00 00 21`, and click **Validate / Preview**. Expect voltage `4 V`, status bit-field value `2` (ready), and raw flags `33`.
4. While capture is stopped, click **Apply draft**, or load your own JSON directly through **Protocol Config**.
5. Select the interface carrying your traffic and click **Start**, or press `Ctrl+O` to open PCAP/PCAPNG.
6. Select a packet to inspect fields, errors, and Hex. Use **TCP Streams** when a message spans segments or a segment contains multiple messages.
7. Click **Stop**, then press `Ctrl+S` to save the current memory window. Save the protocol JSON separately.

See the [English user manual](docs/user_manual.en.md) / [中文用户使用手册](docs/user_manual.md) for detailed workflows, configuration rules, statistics, and troubleshooting.

## Behavior to understand

- **Display filters:** UDP/TCP/All, status, search, and structured conditions change visible rows without deleting packets. Structured field comparisons and sorting use raw parsed values; scale, bias, and enums affect display.
- **Pause display:** Capture, memory retention, and automatic archiving continue. Resuming fills the analysis list from the current memory window.
- **Sessions and saving:** Starting keeps existing data; capture can resume after stopping. Successful import replaces the session. Saving first stops capture and writes all raw packets in the window, including hidden rows. Clear, import, and exit check for unsaved packets.
- **Capacity:** 10,000 packets / 16 MiB, 50,000 / 64 MiB (default), or 100,000 / 128 MiB. MiB counts raw packet bytes; parsing and UI consume additional memory. Without auto archive, reaching either limit stops capture. Oversized offline imports are rejected as a whole.
- **Auto archive:** Stop, save, and clear the session before enabling. The memory window evicts safely archived older packets; the complete capture resides in the archive directory. Files are never overwritten or automatically deleted. The file limit applies per archive task. A full queue, write failure, or file limit stops capture. Enable archiving again after restarting the app.
- **TCP boundaries:** Per-packet parsing checks segment payloads; the separate stream analysis reconstructs application messages. It assumes the first segment in each direction starts at a message boundary. Limits: 50,000 matching segments / 16 MiB, 128 connections, 1,000 messages / 4 MiB; each message previews up to 64 fields and 512 Hex bytes.
- **Scope:** No automatic TLS decryption or application protocol library selected by name. Automatic multi-protocol detection, configurable CRC, dynamic arrays/nested structures, complete IP fragment reassembly, and PCAPNG export are not implemented.

## JSON protocol configuration

Field `offset` starts at byte zero of the application payload or individual reassembled message, excluding network headers. `port` matches either source or destination port; it does not create a listening service. This is a standalone example, not a copy of `docs/test.json`:

```json
{
  "protocolName": "DemoUDP",
  "transportType": "UDP",
  "port": 8080,
  "length": { "type": "fixed", "fixedValue": 6 },
  "fields": [
    { "name": "MessageType", "offset": 0, "length": 2, "type": "uint16", "endianness": "big" },
    { "name": "Value", "offset": 2, "length": 4, "type": "int32", "endianness": "big" }
  ]
}
```

Preview `00 01 FF FF FF FE` to obtain `MessageType=1`, `Value=-2`. Variable length uses `"length": { "type": "variable", "fieldName": "TotalLength" }`, with an unsigned integer field of that name in `fields`. Its raw value is the entire message length, including the length field itself; bit fields cannot supply message length.

See the [manual configuration reference](docs/user_manual.en.md#protocol-configuration-reference) for validation rules and advanced attributes. Current example parameters:

| File | Transport / port | Total payload length |
| --- | --- | ---: |
| [extended_fields_example.json](docs/extended_fields_example.json) | UDP / 8080 | 5 bytes |
| [tcp_stream_example.json](docs/tcp_stream_example.json) | TCP / 443 | 43 bytes |
| [test.json](docs/test.json) | UDP / 53744 | 347 bytes |

Examples may change during debugging; check the actual JSON. The TCP example's name `TCP4ByteMessage` does not mean its current length is four bytes.

## Tray, language, and settings directory

By default, closing the window hides it to the tray while capture and archiving continue. Single/double-click the tray icon to show the window; its context menu starts/stops capture or exits. Use **Settings → Exit** or the tray menu to terminate the app. You can disable **Close window to tray** in Settings. Without tray support, closing follows the normal exit workflow. Exit fully before rebuilding to release EXE/DLL file locks.

The **Language** menu switches between Simplified Chinese and English immediately and remembers the selection. Field names and enum labels retain their JSON text. On Windows, Chinese prefers Microsoft YaHei and English prefers Segoe UI; Hex remains monospaced. **About** shows the build version, author Rookie, copyright, and project link.

**Settings → Configuration directory…** displays, changes, or opens the directory containing `settings.ini`. The default is Qt's user application configuration directory, typically `%LOCALAPPDATA%/PacketCaptureTool/Packet Capture Tool` on Windows; use the actual dialog path. Changing directories migrates current settings immediately and retains the old file. A different existing settings file at the destination is not overwritten. Protocol JSON open/save dialogs default to this directory; existing protocol and capture files are not moved automatically.

## Build and test (Windows)

### Dependencies

- C++17 compiler, CMake 3.16+, Git 2.0+, and a Git checkout (configuration checks repository metadata).
- Qt 6.4+, with a kit matching your compiler and architecture. Include Core, Gui, Widgets, Network, Qml, Quick, QuickControls2, QuickDialogs2, PrintSupport, Test, and LinguistTools. Test targets are included by default.
- Windows live capture requires the Npcap driver. `3rdparty/npcap-sdk-1.15` is the compilation SDK and does not replace the installed driver. See the [Npcap download page](https://npcap.com/#download) and the installation illustration below.
- CMake uses FluentUI `main` and PcapPlusPlus `v25.05`. Initial configuration fetches them into `3rdparty/gitrep/` and requires network access. Dependency directories with an existing `.git` reuse local sources.

![Npcap installation options](docs/screenshots/img.png)

### Configure, build, and run

This PowerShell example uses a Qt MinGW kit and Ninja. Replace paths with your installation, and first put the kit's matching MinGW compiler, Qt `bin`, and Ninja on `PATH`:

```powershell
git clone https://github.com/RookieLinux/PacketCaptureTool.git
cd PacketCaptureTool
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:/Qt/6.6.2/mingw_64" -DWINDEPLOYQT_EXECUTABLE="D:/Qt/6.6.2/mingw_64/bin/windeployqt.exe"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
.\build\PacketCaptureTool.exe
```

Single-configuration generators such as Ninja / MinGW Makefiles usually place the EXE directly in the build directory. Multi-configuration generators usually use a `Release` subdirectory; follow your generator's actual output. Use separate build directories for different compilers, Qt kits, or generators.

When `WINDEPLOYQT_EXECUTABLE` is set, the build invokes windeployqt and copies the FluentUI QML module and third-party DLLs. Distribute the complete deployment directory with Qt DLLs, `platforms`, `qml`, and other runtime files. LinguistTools compiles the app's English translation into embedded resources.

For an existing `cmake-build-release` configuration:

```powershell
cmake --build cmake-build-release --parallel 4
ctest --test-dir cmake-build-release --output-on-failure
```

Test suites: `PacketCaptureTests`, `ConfigurationLoaderTests`, `ProtocolValidationTests`, `TcpStreamTests`, and `QmlInterfaceTests`. UI tests use offscreen / software rendering. Automated coverage does not establish sustained high-throughput performance on physical interfaces.

## Structure and architecture

```text
.
├── 3rdparty/       # Npcap SDK, third-party sources and runtime libraries
├── cmake/          # Git checks, dependency fetching and build helpers
├── docs/           # Bilingual manuals, examples, header guides and screenshots
├── src/
│   ├── backend/    # Capture, archive, parsing, streams, models, settings and tray
│   ├── ui/         # QML window, editor, analysis and Hex views
│   ├── i18n/       # English translation source app_en.ts
│   └── resources/  # SVG, PNG, ICO and Windows icon resources
├── tests/          # Qt Test automated checks
└── CMakeLists.txt
```

QML provides interaction and presentation. `CaptureController` coordinates sessions and background tasks; `PacketModel` supplies the analysis list. `PacketCaptureEngine`, `CaptureArchive`, `ProtocolParser`, and `TcpStreamAnalyzer` handle capture, archiving, field parsing, and reassembly respectively. `SettingsStorage` and `LanguageSettings` manage settings and language; `DesktopIntegration` manages the tray.

## Documentation

- [English user manual](docs/user_manual.en.md) / [中文用户使用手册](docs/user_manual.md)
- [TCP header Hex guide](docs/tcp_header_hex_guide.md) / [UDP header Hex guide](docs/udp_header_hex_guide.md) (Chinese)
- [Project on GitHub](https://github.com/RookieLinux/PacketCaptureTool)
