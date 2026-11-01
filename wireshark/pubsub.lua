-- pubsub.lua - a Wireshark plugin that shows our packet header as named fields.
--
-- Without it, Wireshark shows our data as raw bytes ("Data: 0000000100000001...").
-- With it, you see Sequence, Frame ID, Chunk index, ... The byte positions below
-- must match the Packet struct in ../protocol.h.
--
-- Install: copy this file to ~/.config/wireshark/plugins/ and restart Wireshark.
-- (Lua is Wireshark's plugin language. "--" starts a comment, like "//" in C++.)

local PORT = 9090
-- Wireshark can't guess what our UDP packets are, so we say "UDP port 9090 is ours".

local pubsub = Proto("pubsub", "Pub-Sub")
-- Our protocol. "pubsub" is the name used in filters (pubsub.seq == 5),
-- "Pub-Sub" is the name shown on screen.

local type_names = { [1] = "SUBSCRIBE", [2] = "UNSUBSCRIBE", [3] = "DATA" }
-- So Wireshark shows "DATA" instead of "3" (same numbers as the enum in protocol.h).

-- One field per header field: ProtoField.<kind>("filter name", "label on screen").
-- uint32 = 4 bytes, uint16 = 2 bytes, string = text, bytes = raw bytes.
local f_seq         = ProtoField.uint32("pubsub.seq", "Sequence")
local f_frame_id    = ProtoField.uint32("pubsub.frame_id", "Frame ID")
local f_chunk_idx   = ProtoField.uint16("pubsub.chunk_idx", "Chunk index")
local f_chunk_total = ProtoField.uint16("pubsub.chunk_total", "Chunk total")
local f_len         = ProtoField.uint16("pubsub.len", "Data length")
local f_type        = ProtoField.uint16("pubsub.type", "Type", base.DEC, type_names)  -- decimal + name
local f_stream      = ProtoField.string("pubsub.stream", "Stream name")
local f_data        = ProtoField.bytes("pubsub.data", "Data")

pubsub.fields = { f_seq, f_frame_id, f_chunk_idx, f_chunk_total, f_len, f_type, f_stream, f_data }
-- Register the fields, so filters like "pubsub.frame_id == 7" work.

-- Wireshark calls this once for every packet on our port.
--   buf   = the UDP payload: our 32-byte header + data
--   pinfo = packet info (we set the Protocol and Info columns)
--   tree  = the details pane at the bottom of Wireshark
function pubsub.dissector(buf, pinfo, tree)
    pinfo.cols.protocol = "PUBSUB"                           -- "Protocol" column

    local packet = tree:add(pubsub, buf(), "Pub-Sub Packet") -- a "Pub-Sub Packet" line in the details pane

    -- buf(start, length) = those bytes. Wireshark reads numbers big-endian,
    -- which is the network byte order our programs used (htonl/htons).
    packet:add(f_seq,         buf(0, 4))     -- bytes 0-3
    packet:add(f_frame_id,    buf(4, 4))     -- bytes 4-7
    packet:add(f_chunk_idx,   buf(8, 2))     -- bytes 8-9
    packet:add(f_chunk_total, buf(10, 2))    -- bytes 10-11
    packet:add(f_len,         buf(12, 2))    -- bytes 12-13
    packet:add(f_type,        buf(14, 2))    -- bytes 14-15
    packet:add(f_stream,      buf(16, 16))   -- bytes 16-31
    if buf:len() > 32 then
        packet:add(f_data, buf(32))          -- bytes 32 to the end (SUBSCRIBE has no data)
    end

    -- "Info" column, e.g. "DATA seq=42". ".." joins strings, :uint() reads bytes as a number.
    pinfo.cols.info = type_names[buf(14, 2):uint()] .. " seq=" .. buf(0, 4):uint()
end

DissectorTable.get("udp.port"):add(PORT, pubsub)
-- Hook it up: "send UDP packets on PORT to our dissector".
