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

print('// Generated on {} from {}'.format(time.strftime('%F %T'), url))
print('#define PEN_TABLE_MAX {}'.format(pens[-1][0]))
print('struct pen_entry_s { ')
print('    unsigned short decimal;')
print('    const char *organization;')
print('    const char *contact;')
print('    const char *email; ')
print('};')
print('const struct pen_entry_s pen_table[] = {')
prev_dec = -1
for dec, org, contact, email in pens:
    dec = int(dec)
    # Fill the gap with empty records if dec does not match array index
    for i in range(1, dec - prev_dec):
        print('    {{ {}, {}, {}, {} }},'.format(prev_dec + i, 'NULL', 'NULL', 'NULL')) 
    print('    {{ {}, "{}", "{}", "{}" }},'.format(dec, escape(org), escape(contact), escape(email)))
    prev_dec = dec
print('};')
