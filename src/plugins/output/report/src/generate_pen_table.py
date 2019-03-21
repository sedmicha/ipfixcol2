#!/usr/bin/env python3
import os
import re
import sys
import time
import urllib.request

url = 'https://www.iana.org/assignments/enterprise-numbers/enterprise-numbers'

# Escape characters that would break literal C string
def escape(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')

# Change directory to scripts directory
os.chdir(os.path.dirname(sys.argv[0]))

r = urllib.request.urlopen(url)
pens = re.findall('(\d+)\n {2}(.+)\n {4}(.+)\n {6}(.+)\n', r.read().decode('utf-8'))

with open('pen_table.h', 'w') as f:
    f.write('// Generated on {} from {}\n'.format(time.strftime('%F %T'), url))
    f.write('#define PEN_TABLE_MAX {}\n'.format(pens[-1][0]))
    f.write('struct pen_entry_s { \n')
    f.write('    unsigned short decimal;\n')
    f.write('    const char *organization;\n')
    f.write('    const char *contact;\n')
    f.write('    const char *email; \n')
    f.write('};\n')
    f.write('const struct pen_entry_s pen_table[] = {\n')
    prev_dec = -1
    for dec, org, contact, email in pens:
        dec = int(dec)
        # Fill the gap with empty records if dec does not match array index
        for i in range(1, dec - prev_dec):
            f.write('    {{ {}, {}, {}, {} }},\n'.format(prev_dec + i, 'NULL', 'NULL', 'NULL')) 
        f.write('    {{ {}, "{}", "{}", "{}" }},\n'.format(dec, escape(org), escape(contact), escape(email)))
        prev_dec = dec
    f.write('};\n')
