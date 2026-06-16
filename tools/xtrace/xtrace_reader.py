"""xtrace_reader.py - read xTrace v2 LEB128 byte-stream records.

Stream format per record:
  [event_id : LEB128]  [delta_ts : LEB128]  [param0..N : LEB128 each]

The BOOT record (ID=0x01) uses delta_ts as the absolute session timestamp.
The GAP  record (ID=0x00) signals dropped records: params=(dropped_count,).

Callers receive raw RawRecord namedtuples; xtrace_decoder.py converts them
to TraceRecord objects with schema enrichment and absolute timestamps.
"""

import io
import sys
from collections import namedtuple

# Reserved event IDs (match xTRACE_EV_* constants in xtrace.h)
EV_GAP       = 0x00
EV_BOOT      = 0x01
EV_TIME_SYNC = 0x02

RawRecord = namedtuple("RawRecord", ["event_id", "delta_ts", "params"])


# -- LEB128 --------------------------------------------------------------------

def read_leb128(stream):
    """Read one unsigned LEB128 value from stream.
    Returns (value, bytes_consumed) or raises EOFError / ValueError."""
    value = 0
    shift = 0
    consumed = 0
    while True:
        raw = stream.read(1)
        if not raw:
            raise EOFError(f"partial LEB128 at byte {consumed}")
        b = raw[0]
        consumed += 1
        value |= (b & 0x7F) << shift
        shift += 7
        if (b & 0x80) == 0:
            return value, consumed
        if shift >= 35:   # uint32 max: 5 LEB128 bytes
            raise ValueError(f"LEB128 value exceeds uint32 range at shift={shift}")


def read_exactly(stream, n):
    """Read exactly n bytes from stream, retrying on short reads."""
    data = bytearray()
    while len(data) < n:
        chunk = stream.read(n - len(data))
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


# -- Record reading ------------------------------------------------------------

def _read_one_record(stream, schema):
    """Read one record from the stream.

    schema: dict mapping event_id (int) -> param_count (int).
    Returns a RawRecord or raises EOFError at a clean boundary.
    """
    # 1. Read record length prefix
    raw = stream.read(1)
    if not raw:
        raise EOFError("clean EOF")
    
    first_byte = raw[0]
    record_len = first_byte & 0x7F
    shift = 7
    byte = first_byte
    while byte & 0x80:
        raw2 = stream.read(1)
        if not raw2:
            raise EOFError("Truncated multi-byte record length prefix")
        byte = raw2[0]
        record_len |= (byte & 0x7F) << shift
        shift += 7
        if shift >= 35:
            raise ValueError("record length prefix exceeds uint32 range")
            
    # 2. Read the payload of record_len bytes
    record_data = read_exactly(stream, record_len)
    if len(record_data) < record_len:
        raise EOFError(f"Truncated record payload: expected {record_len} bytes, got {len(record_data)}")
        
    payload_stream = io.BytesIO(record_data)
    
    # 3. Read event_id from payload
    try:
        event_id, _ = read_leb128(payload_stream)
    except EOFError:
        raise EOFError("Truncated event_id in record payload")
    
    # 4. Read delta_ts from payload
    try:
        delta_ts, _ = read_leb128(payload_stream)
    except EOFError:
        raise EOFError("Truncated delta_ts in record payload")
    
    # 5. Read all remaining bytes in the record payload as parameters.
    # OBJECT_NAME strings are printable ASCII, so each name byte is also an
    # unambiguous one-byte LEB128 parameter.
    params = []
    while True:
        peek = payload_stream.read(1)
        if not peek:
            break
        # Put back the peeked byte
        payload_stream.seek(-1, io.SEEK_CUR)
        try:
            v, _ = read_leb128(payload_stream)
            params.append(v)
        except EOFError:
            raise EOFError("Truncated parameter in record payload")

    return RawRecord(event_id=event_id, delta_ts=delta_ts, params=tuple(params))


# -- COBS framing -------------------------------------------------------------

def cobs_decode(encoded: bytes) -> bytes:
    """Decode one COBS-encoded packet (without its trailing 0x00 delimiter).

    COBS (Consistent Overhead Byte Stuffing) eliminates 0x00 bytes from a
    payload so that 0x00 can be used unambiguously as a frame delimiter.
    A single corrupt byte desynchronises only the current packet; the decoder
    resyncs at the next 0x00 - making COBS the correct transport framing for
    UART or Ethernet streams where bit errors can occur.

    Algorithm (RFC-like):
      Each byte CODE encodes how many non-zero bytes follow before the next
      zero.  CODE == 0xFF means 254 non-zero bytes with no implicit zero
      appended; any other CODE < 0xFF appends an implicit 0x00 after the run.
    """
    output = bytearray()
    i = 0
    n = len(encoded)
    while i < n:
        code = encoded[i]
        if code == 0:
            break           # unexpected 0x00 - treat as end of valid data
        i += 1
        run = code - 1
        if i + run > n:    # truncated packet
            break
        output.extend(encoded[i:i + run])
        i += run
        if code != 0xFF and i < n:
            output.append(0)   # implicit zero between chunks
    return bytes(output)


def records_from_cobs_stream(stream, schema=None):
    """Yield RawRecord objects from a COBS-framed LEB128 binary stream.

    Each COBS frame is terminated by a 0x00 byte.  The frame payload is
    COBS-decoded and then parsed as a sequence of LEB128 records by
    records_from_stream().  Empty frames (back-to-back 0x00 bytes) are
    silently skipped.

    Use this function instead of records_from_stream() when the xTRACE
    target transport wraps the LEB128 stream in COBS packets (e.g. for UART
    or Ethernet where resync after bit errors is required).
    """
    if schema is None:
        schema = {}
    frame_buf = bytearray()
    while True:
        byte = stream.read(1)
        if not byte:
            if frame_buf:
                print("[xtrace_reader] WARNING: unterminated COBS frame at EOF",
                      file=sys.stderr)
            break
        if byte[0] == 0x00:
            if frame_buf:
                decoded = cobs_decode(bytes(frame_buf))
                if decoded:
                    yield from records_from_stream(io.BytesIO(decoded), schema)
                frame_buf.clear()
        else:
            frame_buf.append(byte[0])


def records_from_file(path, schema=None, cobs=False):
    """Yield RawRecord objects from a binary LEB128 capture file.

    cobs: set True when the file was produced by a COBS-framing transport
          (e.g. UART).  Each 0x00-delimited packet is COBS-decoded before
          LEB128 parsing.
    """
    if schema is None:
        schema = {}
    with open(path, "rb") as f:
        if cobs:
            yield from records_from_cobs_stream(f, schema)
        else:
            yield from records_from_stream(f, schema)


def records_from_stream(stream, schema=None):
    """Yield RawRecord objects from an open binary stream."""
    if schema is None:
        schema = {}
    while True:
        try:
            rec = _read_one_record(stream, schema)
            yield rec
        except EOFError as exc:
            msg = str(exc)
            if msg != "clean EOF":
                # EOF arrived mid-record - partial/truncated data discarded.
                print(f"[xtrace_reader] WARNING: truncated record discarded "
                      f"({msg})",
                      file=sys.stderr)
            break
        except ValueError as exc:
            print(f"[xtrace_reader] WARNING: stream decode aborted (corrupt data): {exc}",
                  file=sys.stderr)
            break
