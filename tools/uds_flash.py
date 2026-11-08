#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Minimal UDS flashing client for the CAN bootloader (0x7E0 / 0x7E8).

依赖:
    pip install python-can

用法:
    # 刷写固件
    python uds_flash.py -i pcan -c PCAN_USBBUS1 -f app.bin --version 0x00010002 --date 0x20261103

    # 读取 Boot/APP 版本与刷写状态（0x22，默认会话即可）
    python uds_flash.py -i pcan -c PCAN_USBBUS1 --info

    # 只发一条原始 UDS 请求（自动 ISO-TP 分段）
    python uds_flash.py -i pcan -c PCAN_USBBUS1 --raw "10 02"
    python uds_flash.py -i pcan -c PCAN_USBBUS1 --raw "22 01 02"

流程:
    10 02 -> 27 01/02 -> 34 -> 36... -> 37 -> 31 01 FF01 -> 31 01 FF02

注意:
    - 服务端 0x27 使用固定 key 0x11223344。
    - 0x34 请求里的地址必须是 0x08000000，数据实际写入 Backup 区。
    - 每条 0x36 最多携带 256 字节，BSC 从 1 开始递增。
"""

import argparse
import sys
import time
import zlib

try:
    import can
except ImportError:  # pragma: no cover
    sys.stderr.write("缺少 python-can，请先执行: pip install python-can\n")
    sys.exit(2)

REQ_ID = 0x7E0
RESP_ID = 0x7E8

APP_ADDRESS = 0x08000000
SECURITY_KEY = 0x11223344
BLOCK_SIZE = 256                       # 0x36 每帧数据长度
MAX_BLOCK_LEN = 258                    # 1 SID + 1 BSC + 256 data

P2_MS = 50
P2_STAR_MS = 5000

PAD_BYTE = 0xAA


class IsoTpError(Exception):
    pass


class UdsError(Exception):
    def __init__(self, response):
        self.response = response
        super().__init__("negative response: " + " ".join("%02X" % b for b in response))


def _now_ms():
    return time.monotonic() * 1000.0


class IsoTpClient:
    """ISO-TP (ISO 15765-2) normal addressing, classic CAN, 8-byte frames."""

    def __init__(self, bus, req_id=REQ_ID, resp_id=RESP_ID, n_bs_ms=1000, n_cr_ms=1000):
        self.bus = bus
        self.req_id = req_id
        self.resp_id = resp_id
        self.n_bs_ms = n_bs_ms
        self.n_cr_ms = n_cr_ms

    # -- low level ---------------------------------------------------------

    def _send_frame(self, arb_id, payload):
        data = bytearray(payload)
        if len(data) < 8:
            data += bytes([PAD_BYTE]) * (8 - len(data))
        msg = can.Message(arbitration_id=arb_id, is_extended_id=False,
                          data=data, dlc=8)
        self.bus.send(msg)

    def _recv_frame(self, timeout):
        deadline = _now_ms() + timeout * 1000.0
        while True:
            remain = deadline - _now_ms()
            if remain <= 0:
                return None
            msg = self.bus.recv(remain / 1000.0)
            if msg is None:
                continue
            if msg.is_error_frame or msg.is_remote_frame:
                continue
            if msg.arbitration_id != self.resp_id:
                continue
            return bytes(msg.data)[:msg.dlc]

    # -- transmit ----------------------------------------------------------

    def send(self, payload):
        payload = bytes(payload)
        if not payload:
            raise IsoTpError("empty payload")

        if len(payload) <= 7:
            self._send_frame(self.req_id, bytes([len(payload)]) + payload)
            return

        total = len(payload)
        if total > 0xFFF:
            raise IsoTpError("payload too long for ISO-TP: %d" % total)

        # First frame
        self._send_frame(self.req_id,
                         bytes([0x10 | ((total >> 8) & 0x0F), total & 0xFF]) + payload[:6])

        bs, stmin = self._wait_flow_control()
        pos = 6
        sn = 1
        block = 0

        while pos < total:
            chunk = payload[pos:pos + 7]
            self._send_frame(self.req_id, bytes([0x20 | (sn & 0x0F)]) + chunk)
            pos += len(chunk)
            sn = (sn + 1) & 0x0F
            block += 1

            if pos >= total:
                break

            if bs and block >= bs:
                bs, stmin = self._wait_flow_control()
                block = 0

            self._sleep_stmin(stmin)

    def _wait_flow_control(self):
        deadline = _now_ms() + self.n_bs_ms
        while True:
            remain = deadline - _now_ms()
            if remain <= 0:
                raise IsoTpError("timeout waiting for flow control")
            frame = self._recv_frame(remain / 1000.0)
            if frame is None:
                raise IsoTpError("timeout waiting for flow control")
            if (frame[0] >> 4) != 0x3:
                continue
            fs = frame[0] & 0x0F
            if fs == 0:                      # CTS
                return frame[1], frame[2]
            if fs == 1:                      # WAIT
                deadline = _now_ms() + self.n_bs_ms
                continue
            raise IsoTpError("flow control overflow")

    @staticmethod
    def _sleep_stmin(stmin):
        if stmin <= 0x7F:
            delay = stmin / 1000.0
        elif 0xF1 <= stmin <= 0xF9:
            delay = (stmin - 0xF0) / 10000.0
        else:
            delay = 0.0
        if delay > 0:
            time.sleep(delay)

    # -- receive -----------------------------------------------------------

    def recv(self, timeout=1.0):
        deadline = _now_ms() + timeout * 1000.0
        while True:
            remain = deadline - _now_ms()
            if remain <= 0:
                return None
            frame = self._recv_frame(remain / 1000.0)
            if frame is None:
                return None
            if not frame:
                continue

            pci = (frame[0] >> 4) & 0x0F
            if pci == 0x0:                   # single frame
                length = frame[0] & 0x0F
                if length == 0 or length > len(frame) - 1:
                    continue
                return frame[1:1 + length]
            if pci == 0x1:                   # first frame
                total = ((frame[0] & 0x0F) << 8) | frame[1]
                data = bytearray(frame[2:])
                self._send_frame(self.req_id, bytes([0x30, 0, 0]))   # CTS, BS=0
                sn = 1
                got = len(data)
                while got < total:
                    remain = deadline - _now_ms()
                    if remain <= 0:
                        raise IsoTpError("timeout waiting for consecutive frame")
                    cf = self._recv_frame(remain / 1000.0)
                    if cf is None:
                        raise IsoTpError("timeout waiting for consecutive frame")
                    if (cf[0] >> 4) != 0x2:
                        continue
                    if (cf[0] & 0x0F) != sn:
                        raise IsoTpError("wrong sequence number")
                    data += cf[1:]
                    got += len(cf) - 1
                    sn = (sn + 1) & 0x0F
                return bytes(data[:total])
            if pci == 0x3:
                continue                     # flow control from the server
            continue

    # -- request / response ------------------------------------------------

    def request(self, payload, timeout=5.0):
        self.send(payload)
        deadline = _now_ms() + timeout * 1000.0
        while True:
            remain = deadline - _now_ms()
            if remain <= 0:
                raise IsoTpError("timeout waiting for response")
            resp = self.recv(remain / 1000.0)
            if resp is None:
                raise IsoTpError("timeout waiting for response")
            if len(resp) >= 3 and resp[0] == 0x7F and resp[2] == 0x78:
                deadline = _now_ms() + P2_STAR_MS    # responsePending
                continue
            return resp


# ---------------------------------------------------------------------------
# UDS helpers
# ---------------------------------------------------------------------------

def require_positive(resp, sid):
    if not resp:
        raise UdsError(resp or b"")
    if resp[0] != (sid + 0x40):
        raise UdsError(resp)
    return resp


def u32(value):
    return int(value).to_bytes(4, "little")


def parse_int(text):
    return int(text, 0)


def crack_seed(seed):
    """固定 key: 服务端只认 0x11223344，种子仅用于确认链路。"""
    del seed
    return SECURITY_KEY


def uds_session(tp, sub=0x02):
    resp = require_positive(tp.request(bytes([0x10, sub])), 0x10)
    print("  session 0x%02X: %s" % (sub, resp.hex(" ").upper()))


def uds_security(tp):
    resp = require_positive(tp.request(bytes([0x27, 0x01])), 0x27)
    seed = resp[2:]
    print("  seed: %s" % seed.hex(" ").upper())

    key = crack_seed(seed)
    resp = require_positive(tp.request(bytes([0x27, 0x02]) + u32(key)), 0x27)
    print("  key accepted: %s" % resp.hex(" ").upper())


def uds_download(tp, image):
    request = bytes([0x34, 0x44]) + u32(APP_ADDRESS) + u32(len(image))
    resp = tp.request(request, timeout=P2_STAR_MS / 1000.0)
    require_positive(resp, 0x34)
    if len(resp) >= 4:
        max_block = int.from_bytes(resp[2:4], "big")
        print("  max block length: %d" % max_block)
    else:
        print("  max block length: (not reported)")


def uds_transfer_data(tp, image):
    offset = 0
    bsc = 1
    total = len(image)
    while offset < total:
        chunk = image[offset:offset + BLOCK_SIZE]
        resp = tp.request(bytes([0x36, bsc]) + chunk, timeout=P2_STAR_MS / 1000.0)
        require_positive(resp, 0x36)
        if resp[1] != bsc:
            raise UdsError(resp)
        offset += len(chunk)
        bsc = (bsc + 1) & 0xFF
        sys.stdout.write("\r  transfer %6d / %6d bytes" % (offset, total))
        sys.stdout.flush()
    sys.stdout.write("\n")


def uds_transfer_exit(tp):
    resp = require_positive(tp.request(bytes([0x37]), timeout=P2_STAR_MS / 1000.0), 0x37)
    print("  transfer exit: %s" % resp.hex(" ").upper())


def uds_check_crc(tp, image):
    crc = zlib.crc32(image) & 0xFFFFFFFF
    resp = tp.request(bytes([0x31, 0x01, 0xFF, 0x01]) + u32(crc),
                      timeout=P2_STAR_MS / 1000.0)
    require_positive(resp, 0x31)
    print("  crc32 0x%08X: %s" % (crc, resp.hex(" ").upper()))
    return crc


def uds_activate(tp, version, date):
    resp = tp.request(bytes([0x31, 0x01, 0xFF, 0x02]) + u32(version) + u32(date),
                      timeout=30.0)
    require_positive(resp, 0x31)
    print("  activate: %s (ECU resets)" % resp.hex(" ").upper())


def flash(filename, bus, version, date):
    with open(filename, "rb") as f:
        image = f.read()
    if not image:
        raise SystemExit("固件文件为空: %s" % filename)

    print("firmware: %s (%d bytes)" % (filename, len(image)))
    tp = IsoTpClient(bus)

    print("[1/7] 10 02 programming session")
    uds_session(tp, 0x02)

    print("[2/7] 27 security access")
    uds_security(tp)

    print("[3/7] 34 request download")
    uds_download(tp, image)

    print("[4/7] 36 transfer data")
    uds_transfer_data(tp, image)

    print("[5/7] 37 transfer exit")
    uds_transfer_exit(tp)

    print("[6/7] 31 01 FF01 check CRC")
    uds_check_crc(tp, image)

    print("[7/7] 31 01 FF02 activate")
    uds_activate(tp, version, date)


def raw_request(bus, text):
    payload = bytes(int(b, 16) for b in text.replace(",", " ").split())
    if not payload:
        raise SystemExit("--raw 需要至少一个字节")
    tp = IsoTpClient(bus)
    print("-> " + payload.hex(" ").upper())
    resp = tp.request(payload)
    print("<- " + resp.hex(" ").upper())


# ---------------------------------------------------------------------------
# 0x22 ReadDataByIdentifier
# ---------------------------------------------------------------------------

DIDS = (
    (0x0100, "boot version", "version"),
    (0x0101, "boot build date", "ascii"),
    (0x0102, "app version", "hex32"),
    (0x0103, "app date", "hex32"),
    (0x0104, "app size", "int"),
    (0x0105, "app crc", "hex32"),
    (0x0106, "flash status", "status"),
    (0xF186, "active session", "hex"),
    (0xF187, "part number", "ascii"),
    (0xF189, "sw version", "ascii"),
    (0xF191, "hw version", "ascii"),
)


def _format_did(kind, data):
    if kind == "ascii":
        return data.rstrip(b"\x00").decode("ascii", "replace")
    if not data:
        return "(empty)"
    if kind == "int":
        return "%d" % int.from_bytes(data, "little")
    if kind == "hex32":
        return "0x%08X" % int.from_bytes(data, "little")
    if kind == "version" and len(data) >= 3:
        return "%d.%d.%d" % (data[0], data[1], data[2])
    if kind == "status" and len(data) >= 10:
        states = {0: "none", 1: "staging", 2: "committed"}
        state = states.get(data[1], "0x%02X" % data[1])
        return ("record_valid=%d state=%s flash_count=%d last_result=0x%08X"
                % (data[0], state,
                   int.from_bytes(data[2:6], "little"),
                   int.from_bytes(data[6:10], "little")))
    return data.hex(" ").upper()


def read_info(bus):
    """读取 Boot/APP 版本、刷写状态（默认会话即可，无需解锁）。"""
    tp = IsoTpClient(bus)
    for did, name, kind in DIDS:
        resp = tp.request(bytes([0x22, did >> 8, did & 0xFF]), timeout=2.0)
        if not resp or resp[0] != 0x62:
            print("  %04X %-16s %s" % (did, name, resp.hex(" ").upper()))
            continue
        print("  %04X %-16s %s" % (did, name, _format_did(kind, resp[3:])))


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        description="UDS flashing client for the CAN bootloader (0x7E0/0x7E8)")
    parser.add_argument("-i", "--interface", default="pcan",
                        help="python-can interface, e.g. pcan, vector, socketcan, kvaser")
    parser.add_argument("-c", "--channel", default=None,
                        help="interface channel, e.g. PCAN_USBBUS1, can0")
    parser.add_argument("-b", "--bitrate", type=parse_int, default=500000,
                        help="CAN bitrate (default 500000)")
    parser.add_argument("--req-id", type=parse_int, default=REQ_ID,
                        help="request arbitration ID (default 0x7E0)")
    parser.add_argument("--resp-id", type=parse_int, default=RESP_ID,
                        help="response arbitration ID (default 0x7E8)")
    parser.add_argument("-f", "--file", help="firmware .bin to flash")
    parser.add_argument("--version", type=parse_int, default=0x00010000,
                        help="INFO version word, little-endian (default 0x00010000)")
    parser.add_argument("--date", type=parse_int, default=0,
                        help="INFO date word, little-endian (default 0)")
    parser.add_argument("--raw", help="send one raw UDS request, e.g. \"3E 00\"")
    parser.add_argument("--info", action="store_true",
                        help="read boot/app version and flash status via 0x22")
    args = parser.parse_args(argv)

    if not args.file and not args.raw and not args.info:
        parser.error("需要 -f <bin>、--raw <hex> 或 --info")

    bus_kwargs = {"interface": args.interface, "bitrate": args.bitrate}
    if args.channel:
        bus_kwargs["channel"] = args.channel

    try:
        bus = can.Bus(**bus_kwargs)
    except Exception as exc:
        sys.stderr.write("无法打开 CAN 设备: %s\n" % exc)
        return 2

    try:
        if args.raw:
            raw_request(bus, args.raw)
        elif args.info:
            read_info(bus)
        else:
            flash(args.file, bus, args.version, args.date)
    except (IsoTpError, UdsError) as exc:
        sys.stderr.write("\n失败: %s\n" % exc)
        return 1
    finally:
        try:
            bus.shutdown()
        except Exception:
            pass

    print("完成。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
