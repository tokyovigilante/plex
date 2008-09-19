#!/opt/local/bin/python

import os, sys
from Foundation import NSMutableDictionary

version = os.popen4("/opt/local/bin/git rev-parse HEAD")[1].read().strip()
git_rev = version[0:7]
info = os.environ['BUILT_PRODUCTS_DIR']+"/"+os.environ['WRAPPER_NAME']+"/Contents/Info.plist"

plist = NSMutableDictionary.dictionaryWithContentsOfFile_(info)
plist['CFBundleVersion'] = plist['CFBundleShortVersionString'] + '-' + git_rev
plist.writeToFile_atomically_(info, 1)
