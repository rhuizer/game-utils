#!/usr/bin/python
#
# EXEPACK unpacker for 16-bit MS-DOS applications.
#
# Written to unpack Champions of Krynn START.EXE and untested with anything
# else.
#
# Quick and dirty hack.  No design, I know.
#
#  -- Ronald Huizer (C) 2012 / rhuizer@hexpedition.com
#
import sys
import struct
import cStringIO

def roundup(x, y):
    return x if x % y == 0 else x + y - x % y

def pack_dos_header(dos_header):
    ret = ""

    ret += struct.pack("<H", dos_header['e_magic'])
    ret += struct.pack("<H", dos_header['e_cblp'])
    ret += struct.pack("<H", dos_header['e_cp'])
    ret += struct.pack("<H", dos_header['e_crlc'])
    ret += struct.pack("<H", dos_header['e_cparhdr'])
    ret += struct.pack("<H", dos_header['e_minalloc'])
    ret += struct.pack("<H", dos_header['e_maxalloc'])
    ret += struct.pack("<H", dos_header['e_ss'])
    ret += struct.pack("<H", dos_header['e_sp'])
    ret += struct.pack("<H", dos_header['e_csum'])
    ret += struct.pack("<H", dos_header['e_ip'])
    ret += struct.pack("<H", dos_header['e_cs'])
    ret += struct.pack("<H", dos_header['e_lfarlc'])
    ret += struct.pack("<H", dos_header['e_ovno'])

    return ret

def unpack_dos_header(data):
    header_keys = [
        'e_magic',    'e_cblp',     'e_cp',     'e_crlc', 'e_cparhdr',
        'e_minalloc', 'e_maxalloc', 'e_ss',     'e_sp',   'e_csum',
        'e_ip',       'e_cs',       'e_lfarlc', 'e_ovno'
    ]

    # Unpack the 16-bit MS-DOS header data.
    header_values = list(struct.unpack_from("<14H", data))

    return dict(zip(header_keys, header_values))

def dos_header_file_size(dos_header):
    return (dos_header['e_cp'] - 1) * 512 + dos_header['e_cblp']

def unpack_exepack_header(data):
    header_keys = [
        'real_ip', 'real_cs', 'start_seg', 'exepack_size',
        'real_sp', 'real_ss', 'real_size', 'skip_len', 'signature'
    ]

    # Unpack the EXEPACK header.
    header_values = list(struct.unpack("<9H", data))

    return dict(zip(header_keys, header_values))

def inflate_exepack_data(data):
    # EXEPACK uses backward processing, so we reverse the data we inflate.
    s = cStringIO.StringIO(data[::-1])

    # Skip leading 0xFF bytes, as they're used for padding.
    while ord(s.read(1)) == 0xFF:
        pass
    s.seek(-1, 1)

    # EXEPACK inflate loop.  Note that we reversed the string, so we process
    # words in big endian notation.
    ret = ""
    while True:
        opcode, count = struct.unpack(">BH", s.read(3))

        if opcode & 0xFE == 0xB0:
            ret += s.read(1) * count
        elif opcode & 0xFE == 0xB2:
            ret += s.read(count)
        else:
            raise RuntimeError("Packed file is corrupt")

        if opcode & 1 == 1:
            break

    # If there is still data left in the string, add it.
    ret += s.read()

    return ret[::-1]

def exepack_relocations(exepack_data):
    errormsg = 'Packed file is corrupt'
    index = exepack_data.find(errormsg)
    if index == -1:
        raise RuntimeError('Unknown EXEPACK format')

    s = cStringIO.StringIO(exepack_data)
    s.seek(index + len(errormsg))

    relocations = []
    for segment in xrange(0, 0x10000, 0x1000):
        count, = struct.unpack("<H", s.read(2))
        offsets = struct.unpack("<%dH" % count, s.read(count * 2))
        relocations.extend([(segment, offset) for offset in offsets])

    return relocations

def valid_dos_header(dos_header):
    # The file has at least one page.
    if dos_header['e_cp'] == 0:
        return False

    # The last page shall not hold 0 bytes.
    if dos_header['e_cblp'] == 0:
        return False

    # The header always spans an even number of paragraphs.
    if dos_header['e_cparhdr'] % 2 != 0:
        return False

    # Overlays are not supported at the moment.
    if dos_header['e_ovno'] != 0:
        return False

    # Existing relocations are not supported.
    if dos_header['e_crlc'] != 0:
        return False

    return True

if len(sys.argv) < 2:
    filename = sys.argv[0] if sys.argv[0] != None else 'unpack.py'
    sys.exit('Use as: %s <packed file> [unpacked file]' % filename)

with open(sys.argv[1], 'rb') as packed_file:
    raw_header = packed_file.read(28)

    if len(raw_header) != 28:
        sys.exit('%s is not a valid MS-DOS executable.' % sys.argv[1])

    dos_header = unpack_dos_header(raw_header)
    if dos_header['e_magic'] != 0x5A4D:
        sys.exit('%s is not a valid MS-DOS executable.' % sys.argv[1])

    if not valid_dos_header(dos_header):
        sys.exit('%s is an unsupported MS-DOS executable.' % sys.argv[1])

    print dos_header

    # Calculate the offset of the EXEPACK header, and read it.
    exepack_header_offset = (dos_header['e_cparhdr'] + dos_header['e_cs']) * 16
    packed_file.seek(exepack_header_offset)
    raw_header = packed_file.read(18)

    if len(raw_header) != 18:
        sys.exit('%s is not a valid EXEPACK executable.' % sys.argv[1])

    exepack_header = unpack_exepack_header(raw_header)
    if exepack_header['signature'] != 0x4252:
        sys.exit('%s is not a valid EXEPACK executable.' % sys.argv[1])

    print "EXEPACK header found at offset 0x%x" % exepack_header_offset

    # Read the EXEPACK data.
    exepack_data = packed_file.read(exepack_header['exepack_size'] - 18)
    if len(exepack_data) != exepack_header['exepack_size'] - 18:
        sys.exit('%s is not a valid EXEPACK executable.' % sys.argv[1])

    # Ensure there is no trailing data, as we don't handle it at the moment.
    if packed_file.tell() != dos_header_file_size(dos_header):
        sys.exit('%s is an EXEPACK executable with data after the unpacker.' % sys.argv[1])

    # Determine the location and length of the packed data.
    packed_data_start = dos_header['e_cparhdr'] * 16
    packed_data_end = exepack_header_offset - (exepack_header['skip_len'] - 1) * 16
    packed_data_len = packed_data_end - packed_data_start

    print "Packed data range 0x%x-0x%x" % (packed_data_start, packed_data_end - 1)

    # Read and inflate EXEPACK packed data.
    packed_file.seek(packed_data_start)
    packed_data = packed_file.read(packed_data_len)
    unpacked_data = inflate_exepack_data(packed_data)

    # Construct the new DOS header.
    relocations = exepack_relocations(exepack_data)

    new_dos_header = dos_header.copy()
    new_dos_header['e_ip'] = exepack_header['real_ip']
    new_dos_header['e_cs'] = exepack_header['real_cs']
    new_dos_header['e_sp'] = exepack_header['real_sp']
    new_dos_header['e_ss'] = exepack_header['real_ss']
    new_dos_header['e_crlc'] = len(relocations)
    new_dos_header['e_lfarlc'] = 0x1C

    # Calculate the new executable header and body length.
    new_dos_header_size = 0x1C + len(relocations) * 4
    # Round up the header size to a multiple of 256 for best portability.
    new_dos_header_size = roundup(new_dos_header_size, 256)
    # Set the number of header paragraphs used in the header.
    new_dos_header['e_cparhdr'] = new_dos_header_size / 16
    new_total_size = new_dos_header_size + len(unpacked_data)
    new_dos_header['e_cp'] = roundup(new_total_size, 256) / 256
    new_dos_header['e_cblp'] = new_total_size % 256

# Finally write out everything to meh.exe for now.
with open("meh.exe", 'wb') as unpacked_file:
    unpacked_file.write(pack_dos_header(new_dos_header))

    # Write out the relocation table.
    for segment, offset in relocations:
        unpacked_file.write(struct.pack("<2H", offset, segment))

    # Pad out the header to a multiple of 256.
    while unpacked_file.tell() % 256 != 0:
        unpacked_file.write("\x00")

    # Write out the inflated data.
    unpacked_file.write(unpacked_data)
