#!/usr/bin/python

from elftools.common.py3compat import bytes2str
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import *
from array import array

ksymtab = []
ksymtab_gpl = []
ksymtab_offsets = []
ksymtab_gpl_offsets = []
ksymtab_strings = []

with open('vmlinux', 'rb') as f:
    elffile = ELFFile(f)

    section = elffile.get_section_by_name('__ksymtab')
#    print section.header
    if not section:
        sys.stderr.write('__ksymtab not found.\n')
        sys.exit(1)

    a = array('I')
    a.fromstring(section.data())
    ksymtab = a.tolist()[0::2]
    ksymtab_offsets = a.tolist()[1::2]

    section = elffile.get_section_by_name('__ksymtab_gpl')
    if not section:
        sys.stderr.write('__ksymtab_gpl not found.\n')
        sys.exit(1)

    a = array('I')
    a.fromstring(section.data())
    ksymtab_gpl = a.tolist()[0::2]
    ksymtab_gpl_offsets = a.tolist()[1::2]

    section = elffile.get_section_by_name('__ksymtab_strings')
#    print section.header
    if not section:
        sys.stderr.write('__ksymtab_strings not found.\n')
        sys.exit(1)

    ksymtab_strings = section.data()

    # Relocate the offsets.
    ksymtab_offsets = [o - section['sh_addr'] for o in ksymtab_offsets]
    ksymtab_gpl_offsets = [o - section['sh_addr'] for o in ksymtab_gpl_offsets]

for c, i in enumerate(ksymtab_offsets):
    print "%x %s" % (ksymtab[c], ksymtab_strings[i:].split('\0')[0])

for c, i in enumerate(ksymtab_gpl_offsets):
    print "%x %s" % (ksymtab_gpl[c], ksymtab_strings[i:].split('\0')[0])
