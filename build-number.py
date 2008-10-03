#!/usr/bin/python
import os, sys, re

version = os.popen4("/opt/local/bin/git rev-parse HEAD")[1].read().strip()
git_rev = version[0:7]
info = os.environ['BUILT_PRODUCTS_DIR']+"/"+os.environ['WRAPPER_NAME']+"/Contents/Info.plist"

plist = open(info).read()
shortVersion = re.search('<string>([0-9]+\.[0-9]+\.[0-9]+)</string>', plist)
plist = plist.replace('BUNDLE_VERSION', shortVersion.group(1) + '-' + git_rev)
open(info, "w").write(plist)
