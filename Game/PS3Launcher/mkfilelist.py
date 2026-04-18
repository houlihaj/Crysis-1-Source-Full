#!/usr/bin/env python

"""Script for generating the 'files.list' file in the root directory of the
game.  The 'files.list' file is read by the game at startup and is used for
optimizing file access (i.e. don't try to read files that don't exist).

The script will check if all path components are unique within their containing
directories, e.g. it is an error if a directory contains the files 'foo' and
'FOO'.  The script will fail if ambiguous file component names are found.
"""

import sys, os, os.path

ambiguous_component = False

def check(path, l):
  global ambiguous_component

  l_lower = [ ]
  for name in l:
    name_lower = name.lower()
    if name_lower in l_lower:
      sys.stderr.write('ambiguous path component '
	  + repr(name) + ' in directory ' + repr(path)
	  + '\n')
      ambiguous_component = True
    l_lower.append(name_lower)

def list(path, out):
  l = os.listdir(path)
  check(path, l)
  l.sort()
  n = len(l)
  s = 0
  for name in l: s += len(name) + 1
  out.write('%i,%i\n' % (n, s))
  for name in l:
    if os.path.isdir(path + '/' + name):
      out.write(name + '/\n')
      list(path + '/' + name, out)
    else:
      out.write(name + '\n')

def main():
  global ambiguous_component

  list('.', sys.stdout)
  if ambiguous_component:
    sys.exit(1)

if __name__ == '__main__': main()

